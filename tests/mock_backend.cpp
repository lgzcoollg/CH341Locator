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
#include <map>
#include <mutex>

// ============================================================
// Mock backend — 替换 Ch341Backend_Enumerate 用于单元测试
// 所有字符串使用 char16_t（UTF-16），与平台 wchar_t 大小无关。
// ============================================================

namespace CH341LocatorTest {

struct DeviceEntry {
    const char16_t* hardwareId;
    const char16_t* const* locationPaths;
    int             locationPathCount;
    const char16_t* locationInformation;
};

struct Scenario {
    const char16_t* name;
    const DeviceEntry* devices;
    int deviceCount;
};

// --- 预置场景 ---

static const char16_t* kSinglePaths[] = { u"PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(1)#USB(2)" };

static const DeviceEntry kSingleDeviceList[] = {
    { u"USB\\VID_1A86&PID_5523&REV_0100", kSinglePaths, 1, u"Port_#0001.Hub_#0002" },
};

static const char16_t* kTwoDev0Paths[] = {
    u"PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(12)#USB(2)",
    u"PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(12)#USB(3)",
    u"ACPI(_SB_)#ACPI(PCI0)#ACPI(XHC_)#ACPI(RHUB)#ACPI(HS01)"
};
static const char16_t* kTwoDev1Paths[] = { u"PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(3)" };
static const char16_t* kTwoDev2Paths[] = {
    u"PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(12)#USB(3)",
    u"PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(4)"
};

static const DeviceEntry kTwoMixedList[] = {
    { u"USB\\VID_1A86&PID_5512&REV_0100", kTwoDev0Paths, 3, u"Port_#0001.Hub_#0002" },
    { u"USB\\VID_1234&PID_5678&REV_0200", kTwoDev1Paths, 1, u"Port_#0003" },
    { u"USB\\VID_1A86&PID_5523&REV_0200", kTwoDev2Paths, 2, u"Port_#0004.Hub_#0003" },
};

const Scenario kScenarioNoDevices     = { u"no_devices",     nullptr,           0 };
const Scenario kScenarioSingleCh341   = { u"single_ch341",   kSingleDeviceList, 1 };
const Scenario kScenarioTwoCh341Mixed = { u"two_ch341_mixed", kTwoMixedList,     3 };

// --- 状态 ---

static const Scenario* g_activeScenario = &kScenarioNoDevices;

void SetScenario(const Scenario* scenario) {
    g_activeScenario = scenario;
}

} // namespace CH341LocatorTest

using namespace CH341LocatorTest;

// ============================================================
// Mock 实现
// ============================================================

int Ch341Backend_Enumerate(Ch341BackendDevice* devices, int maxCount) {
    if (!devices || maxCount <= 0) return 0;
    if (!g_activeScenario || !g_activeScenario->devices) return 0;

    int found = 0;
    for (int i = 0; i < g_activeScenario->deviceCount && found < maxCount; ++i) {
        const DeviceEntry& entry = g_activeScenario->devices[i];

        // 检查 VID/PID 是否匹配 CH341
        bool isCh341 = false;
        for (const char16_t* p = entry.hardwareId; *p; ++p) {
            // 简易匹配：查找 "VID_1A86" 和 "PID_5512"/"PID_5523"
        }
        // 用 C 字符串查找
        auto strstr16 = [](const char16_t* haystack, const char16_t* needle) -> bool {
            // 简易子串匹配
            for (size_t hi = 0; haystack[hi]; ++hi) {
                size_t ni = 0;
                while (needle[ni] && haystack[hi + ni] == needle[ni]) ++ni;
                if (!needle[ni]) return true;
            }
            return false;
        };

        if (!strstr16(entry.hardwareId, u"VID_1A86")) continue;
        if (!strstr16(entry.hardwareId, u"PID_5512") &&
            !strstr16(entry.hardwareId, u"PID_5523")) continue;

        Ch341BackendDevice* dev = &devices[found];

        // USB 拓扑路径 = 第一个 locationPath
        if (entry.locationPathCount > 0 && entry.locationPaths[0]) {
            size_t j = 0;
            while (entry.locationPaths[0][j] && j < 127) {
                dev->usbTopology[j] = entry.locationPaths[0][j];
                ++j;
            }
            dev->usbTopology[j] = u'\0';

            j = 0;
            while (entry.locationPaths[0][j] && j < 1023) {
                dev->locationPath[j] = entry.locationPaths[0][j];
                ++j;
            }
            dev->locationPath[j] = u'\0';
        } else {
            dev->usbTopology[0] = u'\0';
            dev->locationPath[0] = u'\0';
        }

        dev->busId = -1;
        ++found;
    }

    return found;
}
