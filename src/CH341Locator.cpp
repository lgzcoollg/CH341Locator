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

#include "CH341Locator.h"
#include "backend.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

// ============================================================
// 内部统一使用 UTF-16（uint16_t / char16_t），
// 不受平台 wchar_t 大小差异影响。
// ============================================================
using u16string = std::basic_string<char16_t>;

// ============================================================
// 平台无关：手动 UTF-8 ↔ UTF-16 编解码
// ============================================================

static bool WideToUtf8(const u16string& in, std::string& out) {
    out.clear();
    if (in.empty()) return true;

    size_t len = 0;
    for (size_t i = 0; i < in.size();) {
        uint32_t cp = (uint32_t)(uint16_t)in[i];
        if ((cp & 0xFC00) == 0xD800) {
            if (++i >= in.size()) return false;
            uint32_t lo = (uint32_t)(uint16_t)in[i];
            if ((lo & 0xFC00) != 0xDC00) return false;
            cp = 0x10000 + ((cp & 0x3FF) << 10) + (lo & 0x3FF);
            ++i;
        } else if ((cp & 0xFC00) == 0xDC00) {
            return false;
        } else {
            ++i;
        }
        if (cp <= 0x7F)       len += 1;
        else if (cp <= 0x7FF) len += 2;
        else if (cp <= 0xFFFF)len += 3;
        else                  len += 4;
    }

    out.resize(len);
    if (len == 0) return true;

    size_t pos = 0;
    for (size_t i = 0; i < in.size();) {
        uint32_t cp = (uint32_t)(uint16_t)in[i];
        if ((cp & 0xFC00) == 0xD800) {
            uint32_t lo = (uint32_t)(uint16_t)in[i + 1];
            cp = 0x10000 + ((cp & 0x3FF) << 10) + (lo & 0x3FF);
            i += 2;
        } else {
            i += 1;
        }

        if (cp <= 0x7F) {
            out[pos++] = (char)cp;
        } else if (cp <= 0x7FF) {
            out[pos++] = 0xC0 | (cp >> 6);
            out[pos++] = 0x80 | (cp & 0x3F);
        } else if (cp <= 0xFFFF) {
            out[pos++] = 0xE0 | (cp >> 12);
            out[pos++] = 0x80 | ((cp >> 6) & 0x3F);
            out[pos++] = 0x80 | (cp & 0x3F);
        } else {
            out[pos++] = 0xF0 | (cp >> 18);
            out[pos++] = 0x80 | ((cp >> 12) & 0x3F);
            out[pos++] = 0x80 | ((cp >> 6) & 0x3F);
            out[pos++] = 0x80 | (cp & 0x3F);
        }
    }
    return true;
}

static bool Utf8ToWide(const std::string& in, u16string& out) {
    out.clear();
    if (in.empty()) return true;

    size_t len = 0;
    for (size_t i = 0; i < in.size();) {
        uint8_t c = (uint8_t)in[i];
        uint32_t cp;
        size_t extra;
        if (c <= 0x7F)            { cp = c; extra = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 4; }
        else return false;

        if (i + extra > in.size()) return false;
        for (size_t j = 1; j < extra; ++j) {
            uint8_t b = (uint8_t)in[i + j];
            if ((b & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (b & 0x3F);
        }

        if ((extra == 2 && cp < 0x80) ||
            (extra == 3 && cp < 0x800) ||
            (extra == 4 && cp < 0x10000))
            return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        if (cp > 0x10FFFF) return false;

        len += (cp >= 0x10000) ? 2 : 1;
        i += extra;
    }

    out.resize(len);
    if (len == 0) return true;

    size_t pos = 0;
    for (size_t i = 0; i < in.size();) {
        uint8_t c = (uint8_t)in[i];
        uint32_t cp;
        size_t extra;
        if (c <= 0x7F)            { cp = c; extra = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 3; }
        else                          { cp = c & 0x07; extra = 4; }

        for (size_t j = 1; j < extra; ++j)
            cp = (cp << 6) | ((uint8_t)in[i + j] & 0x3F);

        if (cp >= 0x10000) {
            cp -= 0x10000;
            out[pos++] = (char16_t)(0xD800 | (cp >> 10));
            out[pos++] = (char16_t)(0xDC00 | (cp & 0x3FF));
        } else {
            out[pos++] = (char16_t)cp;
        }
        i += extra;
    }
    return true;
}

static char* AllocUtf8(const std::string& s) {
    if (s.empty()) return NULL;
    size_t sz = s.size() + 1;
    char* buf = (char*)malloc(sz);
    if (!buf) return NULL;
    memcpy(buf, s.c_str(), sz);
    return buf;
}

// ============================================================
// 平台相关：通过 backend 枚举设备
// ============================================================

static std::vector<Ch341BackendDevice> g_devices;

static uint32_t RefreshDevices() {
    g_devices.clear();
    Ch341BackendDevice buf[64];
    int n = Ch341Backend_Enumerate(buf, 64);
    g_devices.assign(buf, buf + n);
    return (uint32_t)g_devices.size();
}

// ============================================================
// 公共 API 实现
// ============================================================

uint32_t CH341LOCATOR_CALL GetCh341DeviceCount(void) {
    return RefreshDevices();
}

char* CH341LOCATOR_CALL GetCh341LocatorVersion(void) {
    static const char* fmt =
        "CH341Locator v" CH341LOCATOR_VERSION "\n"
        "Copyright (C) 2026 Flyin. All rights reserved.\n"
        "License: Apache 2.0\n"
        "Build: " __DATE__ " " __TIME__;
    return AllocUtf8(std::string(fmt));
}

void CH341LOCATOR_CALL GetCh341FreeString(void* ptr) {
    if (ptr) free(ptr);
}

// ----- 正向：index → USB 拓扑路径 -----

char* CH341LOCATOR_CALL GetCh341LocationPath(uint32_t iIndex) {
    RefreshDevices();
    if (iIndex >= (uint32_t)g_devices.size()) return NULL;
    const char16_t* src = g_devices[iIndex].usbTopology;
    if (!src || !src[0]) return NULL;
    u16string wp(src);
    std::string u8;
    if (!WideToUtf8(wp, u8)) return NULL;
    return AllocUtf8(u8);
}

// ----- 反向：USB 拓扑路径 → index -----

uint32_t CH341LOCATOR_CALL GetCh341IndexByLocationPathUtf8(const char* locationPath) {
    if (!locationPath || !locationPath[0]) return UINT32_MAX;
    u16string target;
    if (!Utf8ToWide(std::string(locationPath), target)) return UINT32_MAX;
    RefreshDevices();
    for (size_t i = 0; i < g_devices.size(); ++i) {
        if (u16string(g_devices[i].usbTopology) == target)
            return (uint32_t)i;
    }
    return UINT32_MAX;
}

// ============================================================
// Windows XP 兼容 API
// ============================================================

#if defined(_WIN32)

#include <windows.h>
#include <setupapi.h>

static bool ReadInfoFirst(HDEVINFO di, SP_DEVINFO_DATA& dd, u16string& out) {
    wchar_t buf[1024]; DWORD pt = 0, rs = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(di, &dd, SPDRP_LOCATION_INFORMATION,
            &pt, (PBYTE)buf, sizeof(buf), &rs))
        return false;
    // Win32 wchar_t is UTF-16LE, same as char16_t
    out = (const char16_t*)buf;
    return !out.empty();
}

static bool FindCh341ByIdx(uint32_t idx, SP_DEVINFO_DATA& outDD, HDEVINFO& outDI) {
    outDI = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (outDI == INVALID_HANDLE_VALUE) return false;
    SP_DEVINFO_DATA dd = { sizeof(dd) };
    uint32_t c = 0;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(outDI, i, &dd); ++i) {
        wchar_t hwid[1024]; DWORD dt, rs;
        if (!SetupDiGetDeviceRegistryPropertyW(outDI, &dd, SPDRP_HARDWAREID,
                &dt, (PBYTE)hwid, sizeof(hwid), &rs)) continue;
        if (!wcsstr(hwid, L"VID_1A86")) continue;
        if (!wcsstr(hwid, L"PID_5512") && !wcsstr(hwid, L"PID_5523")) continue;
        if (c == idx) { outDD = dd; return true; }
        ++c;
    }
    SetupDiDestroyDeviceInfoList(outDI);
    outDI = INVALID_HANDLE_VALUE;
    return false;
}

char* CH341LOCATOR_CALL GetCh341LocationInfo(uint32_t iIndex) {
    HDEVINFO di;
    SP_DEVINFO_DATA dd = { sizeof(dd) };
    if (!FindCh341ByIdx(iIndex, dd, di)) return NULL;
    u16string w;
    if (!ReadInfoFirst(di, dd, w)) { SetupDiDestroyDeviceInfoList(di); return NULL; }
    SetupDiDestroyDeviceInfoList(di);
    std::string u8;
    if (!WideToUtf8(w, u8)) return NULL;
    return AllocUtf8(u8);
}

uint32_t CH341LOCATOR_CALL GetCh341IndexByLocationInfoUtf8(const char* locationInfo) {
    if (!locationInfo || !locationInfo[0]) return UINT32_MAX;
    u16string target;
    if (!Utf8ToWide(std::string(locationInfo), target)) return UINT32_MAX;

    HDEVINFO di = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (di == INVALID_HANDLE_VALUE) return UINT32_MAX;
    SP_DEVINFO_DATA dd = { sizeof(dd) };
    uint32_t ci = 0;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(di, i, &dd); ++i) {
        wchar_t hwid[1024]; DWORD dt, rs;
        if (!SetupDiGetDeviceRegistryPropertyW(di, &dd, SPDRP_HARDWAREID,
                &dt, (PBYTE)hwid, sizeof(hwid), &rs)) continue;
        if (!wcsstr(hwid, L"VID_1A86")) continue;
        if (!wcsstr(hwid, L"PID_5512") && !wcsstr(hwid, L"PID_5523")) continue;
        u16string s;
        if (ReadInfoFirst(di, dd, s) && s == target) {
            SetupDiDestroyDeviceInfoList(di);
            return ci;
        }
        ++ci;
    }
    SetupDiDestroyDeviceInfoList(di);
    return UINT32_MAX;
}

#endif // _WIN32
