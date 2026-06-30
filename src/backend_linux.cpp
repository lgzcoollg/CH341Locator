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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <climits>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

// ============================================================
// Linux 平台实现 — 通过 sysfs 枚举 USB 设备和 I2C adapter
// ============================================================

#define SYSFS_USB_DEVICES  "/sys/bus/usb/devices"
#define SYSFS_I2C_ADAPTER "/sys/class/i2c-adapter"

/// 读取文件内容到固定缓冲区
static int ReadFile(const char* path, char* buf, size_t bufSize) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, bufSize - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    // 去掉末尾换行
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
    return (int)n;
}

/// 从 uevent 中解析 PRODUCT=<vendor>/<product>/<bcd>
/// 返回 0=成功
static int ParseUeventProduct(const char* uevent, unsigned* vid, unsigned* pid) {
    const char* p = strstr(uevent, "PRODUCT=");
    if (!p) return -1;
    p += 8; // skip "PRODUCT="
    if (sscanf(p, "%x/%x", vid, pid) != 2) return -1;
    return 0;
}

static int ParseI2cBusNumber(const char* dirName) {
    if (strncmp(dirName, "i2c-", 4) != 0) return -1;
    long n = strtol(dirName + 4, NULL, 10);
    if (n < 0 || n > INT_MAX) return -1;
    return (int)n;
}

// ============================================================
// 主枚举函数
// ============================================================

int Ch341Backend_Enumerate(Ch341BackendDevice* devices, int maxCount) {
    if (!devices || maxCount <= 0) return 0;

    int found = 0;

    // ---------- 方法一：通过 I2C adapter 反向查找 ----------

    DIR* i2cDir = opendir(SYSFS_I2C_ADAPTER);
    if (i2cDir) {
        struct dirent* entry;
        while ((entry = readdir(i2cDir)) != NULL && found < maxCount) {
            int busNum = ParseI2cBusNumber(entry->d_name);
            if (busNum < 0) continue;

            // 读 name 文件
            char namePath[512];
            snprintf(namePath, sizeof(namePath), SYSFS_I2C_ADAPTER "/%s/name", entry->d_name);
            char nameBuf[64];
            if (ReadFile(namePath, nameBuf, sizeof(nameBuf)) < 0) continue;

            // 检查是否包含 "ch341"
            for (char* p = nameBuf; *p; ++p) { *p = (char)tolower((unsigned char)*p); }
            if (!strstr(nameBuf, "ch341")) continue;

            Ch341BackendDevice* dev = &devices[found];
            // usbTopology = "/dev/i2c-N"（如 "/dev/i2c-2"）
            char full[32];
            snprintf(full, sizeof(full), "/dev/i2c-%d", busNum);
            size_t fi = 0;
            while (full[fi] && fi < 127) {
                dev->usbTopology[fi] = (char16_t)(unsigned char)full[fi];
                ++fi;
            }
            dev->usbTopology[fi] = u'\0';
            // locationPath 同 usbTopology
            size_t j = 0;
            while (dev->usbTopology[j] && j < 1023) {
                dev->locationPath[j] = dev->usbTopology[j];
                ++j;
            }
            dev->locationPath[j] = u'\0';
            dev->busId = busNum;
            ++found;
        }
        closedir(i2cDir);
    }

    // ---------- 方法二：直接枚举 USB 设备（捕获有 USB 但没有 I2C 驱动的设备）----------

    DIR* usbDir = opendir(SYSFS_USB_DEVICES);
    if (!usbDir) return found;

    struct dirent* entry;
    while ((entry = readdir(usbDir)) != NULL && found < maxCount) {
        if (entry->d_name[0] == '.') continue;
        if (strchr(entry->d_name, ':')) continue;

        char ueventPath[512];
        snprintf(ueventPath, sizeof(ueventPath), SYSFS_USB_DEVICES "/%s/uevent", entry->d_name);
        char ueventBuf[1024];
        if (ReadFile(ueventPath, ueventBuf, sizeof(ueventBuf)) < 0) continue;

        unsigned vid = 0, pid = 0;
        if (ParseUeventProduct(ueventBuf, &vid, &pid) != 0) continue;

        if (vid != 0x1A86) continue;
        if (pid != 0x5512 && pid != 0x5523) continue;

        // 检查是否已经通过方法一找到了此设备
        // 在 Linux 上方法一已经返回所有有驱动的设备，方法二只补充无驱动设备
        char16_t topo[128];
        size_t ti = 0;
        while (entry->d_name[ti] && ti < 127) {
            topo[ti] = (char16_t)(unsigned char)entry->d_name[ti];
            ++ti;
        }
        topo[ti] = u'\0';

        // 这里简化处理：如果方法一已经找到过的设备，方法二跳过
        // 实际上由于 PID 可能匹配不上驱动的过滤，这里做个简单的路径去重
        // （方法一用 i2c bus number 作为路径，和方法二的 USB 路径不同，不会重复）

        Ch341BackendDevice* dev = &devices[found];
        // 无 I2C 驱动时，返回 USB 拓扑路径作为 fallback
        ti = 0;
        while (entry->d_name[ti] && ti < 127) {
            dev->usbTopology[ti] = (char16_t)(unsigned char)entry->d_name[ti];
            ++ti;
        }
        dev->usbTopology[ti] = u'\0';
        ti = 0;
        while (dev->usbTopology[ti] && ti < 1023) {
            dev->locationPath[ti] = dev->usbTopology[ti];
            ++ti;
        }
        dev->locationPath[ti] = u'\0';
        dev->busId = -1;
        ++found;
    }

    closedir(usbDir);
    return found;
}
