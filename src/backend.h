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

#ifndef CH341LOCATOR_BACKEND_H
#define CH341LOCATOR_BACKEND_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 单个 CH341 设备的信息（由 backend 填充）
/// 所有字符串均以 UTF-16（char16_t）存储，不受平台 wchar_t 大小影响。
typedef struct {
    char16_t usbTopology[128];   // USB 拓扑路径（如 u"1-2.3"、u"0x14200000"、u"PCIROOT(0)#..."）
    char16_t locationPath[1024]; // 平台原生位置路径
    int      busId;              // 平台总线 ID（Linux: i2c-N 的 N, Windows/macOS: -1）
} Ch341BackendDevice;

/// 枚举当前系统中所有 CH341 设备
/// @param devices  输出缓冲区
/// @param maxCount 缓冲区大小（最大设备数）
/// @return 实际找到的 CH341 设备数量
int Ch341Backend_Enumerate(Ch341BackendDevice* devices, int maxCount);

#ifdef __cplusplus
}
#endif

#endif // CH341LOCATOR_BACKEND_H
