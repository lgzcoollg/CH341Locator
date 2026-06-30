// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "backend.h"

#include <cstring>
#include <cstdio>

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

// ============================================================
// macOS 平台实现 — 使用 IOKit 枚举 USB 设备
//
// 编译需要链接:
//   -framework IOKit -framework CoreFoundation
// ============================================================

/// 从 CFNumber 读取 uint32_t 值
static bool GetCFNumber32(CFTypeRef ref, uint32_t* out) {
    if (!ref || CFGetTypeID(ref) != CFNumberGetTypeID()) return false;
    return CFNumberGetValue((CFNumberRef)ref, kCFNumberSInt32Type, out) != 0;
}

/// 从 IOUSB 设备字典中提取 uint32_t 属性
static bool GetUSBPropertyU32(io_object_t usbDevice, CFStringRef key, uint32_t* out) {
    CFTypeRef ref = IORegistryEntryCreateCFProperty(usbDevice, key, kCFAllocatorDefault, 0);
    if (!ref) return false;
    bool ok = GetCFNumber32(ref, out);
    CFRelease(ref);
    return ok;
}

/// 将 uint32_t 格式化为 char16_t 十六进制字符串
static void FormatHexU16(char16_t* buf, size_t bufMax, uint32_t val) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "0x%08X", val);
    size_t i = 0;
    while (tmp[i] && i < bufMax - 1) {
        buf[i] = (char16_t)(unsigned char)tmp[i];
        ++i;
    }
    buf[i] = u'\0';
}

int Ch341Backend_Enumerate(Ch341BackendDevice* devices, int maxCount) {
    if (!devices || maxCount <= 0) return 0;

    // 创建 USB 设备匹配字典
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if (!matchingDict) return 0;

    // 添加 VID 匹配
    uint32_t targetVid = 0x1A86;
    CFNumberRef vidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &targetVid);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), vidRef);
    CFRelease(vidRef);

    // 获取匹配的 IO 服务迭代器
    io_iterator_t iterator = 0;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator);
    if (kr != KERN_SUCCESS) return 0;

    int found = 0;
    io_service_t usbDevice;
    while ((usbDevice = IOIteratorNext(iterator)) != 0 && found < maxCount) {
        // 读取 PID
        uint32_t pid = 0;
        if (!GetUSBPropertyU32(usbDevice, CFSTR(kUSBProductID), &pid)) {
            IOObjectRelease(usbDevice);
            continue;
        }

        // 检查是否匹配 PID_5512 或 PID_5523
        if (pid != 0x5512 && pid != 0x5523) {
            IOObjectRelease(usbDevice);
            continue;
        }

        Ch341BackendDevice* dev = &devices[found];

        // 读取 locationID
        uint32_t locationID = 0;
        if (GetUSBPropertyU32(usbDevice, CFSTR("locationID"), &locationID)) {
            FormatHexU16(dev->usbTopology, 128, locationID);
        } else {
            // fallback: 使用 vid:pid 作为标识
            char fallback[64];
            snprintf(fallback, sizeof(fallback), "1a86:%04x", pid);
            size_t fi = 0;
            while (fallback[fi] && fi < 127) {
                dev->usbTopology[fi] = (char16_t)(unsigned char)fallback[fi];
                ++fi;
            }
            dev->usbTopology[fi] = u'\0';
        }

        // locationPath = usbTopology（macOS 上相同）
        size_t j = 0;
        while (dev->usbTopology[j] && j < 1023) {
            dev->locationPath[j] = dev->usbTopology[j];
            ++j;
        }
        dev->locationPath[j] = u'\0';

        dev->busId = -1;
        ++found;

        IOObjectRelease(usbDevice);
    }

    IOObjectRelease(iterator);
    return found;
}
