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

#ifndef CH341LOCATOR_H
#define CH341LOCATOR_H

#include <stdint.h>
#include <stddef.h>

// 版本号 — CI 自动据此打 tag 和 release（语义化版本）
#define CH341LOCATOR_VERSION "0.7.4"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Platform abstraction macros
// ============================================================

#if defined(_WIN32)
  #define CH341LOCATOR_API __declspec(dllexport)
  #define CH341LOCATOR_CALL __stdcall
#elif defined(__GNUC__) && (__GNUC__ >= 4)
  #define CH341LOCATOR_API __attribute__((visibility("default")))
  #define CH341LOCATOR_CALL
#else
  #define CH341LOCATOR_API
  #define CH341LOCATOR_CALL
#endif

// On Windows, wchar_t is 16-bit (UTF-16); use explicit uint16_t for portability.
// On other platforms wchar_t is 32-bit (UTF-32), so the W variants are less common
// but provided for API consistency.

// ============================================================
// 通用
// ============================================================

/// 返回当前连接的 CH341 设备数量（VID_1A86 + PID_5512 / PID_5523）
CH341LOCATOR_API uint32_t CH341LOCATOR_CALL GetCh341DeviceCount(void);

/// 返回库版本和版权信息（UTF-8），调用 GetCh341FreeString 释放
CH341LOCATOR_API char*    CH341LOCATOR_CALL GetCh341LocatorVersion(void);

/// 释放 GetCh341LocationPath / GetCh341LocationInfo / GetCh341LocatorVersion 返回的字符串
CH341LOCATOR_API void     CH341LOCATOR_CALL GetCh341FreeString(void* ptr);

// ============================================================
// 正向：index（i2c-N / 序号） → USB 拓扑路径
// ============================================================

/// 返回第 iIndex 个 CH341 设备的 USB 拓扑路径（UTF-8）
/// 成功返回 UTF-8 字符串（须调用 GetCh341FreeString 释放）
/// 失败返回 NULL
CH341LOCATOR_API char*    CH341LOCATOR_CALL GetCh341LocationPath(uint32_t iIndex);

// ============================================================
// 反向：USB 拓扑路径 → index（i2c-N / 序号）
// ============================================================

/// 根据 USB 拓扑路径字符串（UTF-8）反查设备序号
/// 成功返回设备序号（0, 1, 2, ...），失败返回 UINT32_MAX
CH341LOCATOR_API uint32_t CH341LOCATOR_CALL GetCh341IndexByLocationPathUtf8(const char* locationPath);

// ============================================================
// Windows XP 兼容（仅 Windows 平台有效）
// 其他平台上 GetCh341LocationPath 已返回唯一拓扑标识
// ============================================================

#if defined(_WIN32)

/// 返回第 iIndex 个 CH341 设备的 LocationInfo（读取 SPDRP_LOCATION_INFORMATION，UTF-8）
CH341LOCATOR_API char*     CH341LOCATOR_CALL GetCh341LocationInfo(uint32_t iIndex);

/// 根据 LocationInfo 字符串（UTF-8）反查设备序号
CH341LOCATOR_API uint32_t CH341LOCATOR_CALL GetCh341IndexByLocationInfoUtf8(const char* locationInfo);

#endif // _WIN32

#ifdef __cplusplus
}
#endif

#endif // CH341LOCATOR_H
