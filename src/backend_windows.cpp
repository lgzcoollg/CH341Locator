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

#include <windows.h>
#include <setupapi.h>

// ============================================================
// Windows 平台实现 — 使用 SetupAPI 枚举 USB 设备
// Win32 wchar_t 是 UTF-16LE，与 char16_t 等宽，可直接转换。
// ============================================================

/// 检查硬件 ID 是否匹配 CH341（VID_1A86 + PID_5512 / PID_5523）
static int IsTargetCh341(const wchar_t* hwid) {
    return (wcsstr(hwid, L"VID_1A86") != NULL &&
            (wcsstr(hwid, L"PID_5512") != NULL ||
             wcsstr(hwid, L"PID_5523") != NULL));
}

/// 安全复制 wchar_t 字符串到 char16_t 数组
static void CopyToU16(char16_t* dst, size_t dstWords, const wchar_t* src) {
    size_t i = 0;
    while (src[i] && i + 1 < dstWords) {
        dst[i] = (char16_t)src[i];
        ++i;
    }
    dst[i] = u'\0';
}

int Ch341Backend_Enumerate(Ch341BackendDevice* devices, int maxCount) {
    if (!devices || maxCount <= 0) return 0;

    HDEVINFO di = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (di == INVALID_HANDLE_VALUE) return 0;

    int found = 0;
    SP_DEVINFO_DATA dd = { sizeof(dd) };

    for (DWORD i = 0; found < maxCount && SetupDiEnumDeviceInfo(di, i, &dd); ++i) {
        wchar_t hwidBuf[1024];
        DWORD reqSize = 0, dataType = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(di, &dd, SPDRP_HARDWAREID,
                &dataType, (PBYTE)hwidBuf, sizeof(hwidBuf), &reqSize)) {
            continue;
        }
        if (!IsTargetCh341(hwidBuf)) continue;

        Ch341BackendDevice* dev = &devices[found];

        // 读取 SPDRP_LOCATION_PATHS（Vista+）
        wchar_t pathBuf[4096];
        if (SetupDiGetDeviceRegistryPropertyW(di, &dd, SPDRP_LOCATION_PATHS,
                &dataType, (PBYTE)pathBuf, sizeof(pathBuf), &reqSize)) {
            CopyToU16(dev->locationPath, 1024, pathBuf);
        } else {
            dev->locationPath[0] = u'\0';
        }

        // USB 拓扑路径 = locationPath（Windows 上用同一个值）
        if (dev->locationPath[0] != u'\0') {
            CopyToU16(dev->usbTopology, 128, (const wchar_t*)dev->locationPath);
        } else {
            dev->usbTopology[0] = u'\0';
        }

        dev->busId = -1;
        ++found;
    }

    SetupDiDestroyDeviceInfoList(di);
    return found;
}
