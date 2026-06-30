# CH341Locator DLL Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows DLL that enumerates CH341 devices, returns their `Location Paths`, resolves device index by location path, and packages `Win32` and `x64` artifacts with GitHub Actions and GitLab CI support.

**Architecture:** Use a small CMake-based C++17 project with a single shared library target and a minimal demo executable. Keep the public ABI C-compatible by exporting only `extern "C"` functions with `WINAPI`, and isolate Windows SetupAPI enumeration logic inside a focused implementation file.

**Tech Stack:** C++17, CMake, Windows SetupAPI, GitHub Actions, GitLab CI, PowerShell

---

## File Structure

- Create: `include/CH341Locator.h`
  - Public DLL header with export macros and the three approved APIs.
- Create: `src/CH341Locator.cpp`
  - Internal SetupAPI implementation and exported function bodies.
- Create: `examples/ch341locator_demo.cpp`
  - Small demo executable that calls the DLL APIs and prints results.
- Create: `CMakeLists.txt`
  - Defines the DLL target, demo target, include paths, and packaging-friendly output layout.
- Create: `README.md`
  - Documents purpose, build, API usage, packaging, and CI notes.
- Create: `.github/workflows/build.yml`
  - Windows matrix build for `Win32` and `x64`, zip artifact packaging, artifact upload.
- Create: `.gitlab-ci.yml`
  - Windows runner CI template that builds `Win32` and `x64` and archives zip artifacts.
- Optional Create: `packaging/README-artifact.txt`
  - Only if artifact staging needs a tiny helper file. Skip unless packaging becomes awkward.

## Task 1: Scaffold The Buildable Project

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/CH341Locator.h`
- Create: `src/CH341Locator.cpp`
- Create: `examples/ch341locator_demo.cpp`

- [ ] **Step 1: Write the failing build skeleton**

Create `CMakeLists.txt` with the full project skeleton:

```cmake
cmake_minimum_required(VERSION 3.20)

project(CH341Locator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(CH341Locator SHARED
    src/CH341Locator.cpp
)

target_compile_definitions(CH341Locator
    PRIVATE
        CH341LOCATOR_EXPORTS
        UNICODE
        _UNICODE
)

target_include_directories(CH341Locator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(CH341Locator
    PRIVATE
        setupapi
)

set_target_properties(CH341Locator PROPERTIES
    OUTPUT_NAME "CH341Locator"
)

add_executable(ch341locator_demo
    examples/ch341locator_demo.cpp
)

target_compile_definitions(ch341locator_demo
    PRIVATE
        UNICODE
        _UNICODE
)

target_include_directories(ch341locator_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ch341locator_demo
    PRIVATE
        CH341Locator
)
```

Create `include/CH341Locator.h`:

```c
#pragma once

#include <windows.h>

#ifdef CH341LOCATOR_EXPORTS
#define CH341LOCATOR_API __declspec(dllexport)
#else
#define CH341LOCATOR_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

CH341LOCATOR_API DWORD WINAPI GetCh341DeviceCount(void);

CH341LOCATOR_API BOOL WINAPI GetCh341LocationPaths(
    ULONG iIndex,
    LPWSTR buffer,
    DWORD* bufferSize
);

CH341LOCATOR_API BOOL WINAPI GetCh341IndexByLocationPath(
    LPCWSTR locationPath,
    PULONG pIndex
);

#ifdef __cplusplus
}
#endif
```

Create `src/CH341Locator.cpp` with stub implementations:

```cpp
#include "CH341Locator.h"

DWORD WINAPI GetCh341DeviceCount(void) {
    return 0;
}

BOOL WINAPI GetCh341LocationPaths(ULONG iIndex, LPWSTR buffer, DWORD* bufferSize) {
    (void)iIndex;
    (void)buffer;
    (void)bufferSize;
    return FALSE;
}

BOOL WINAPI GetCh341IndexByLocationPath(LPCWSTR locationPath, PULONG pIndex) {
    (void)locationPath;
    (void)pIndex;
    return FALSE;
}
```

Create `examples/ch341locator_demo.cpp`:

```cpp
#include <iostream>
#include "CH341Locator.h"

int wmain() {
    std::wcout << L"CH341 count: " << GetCh341DeviceCount() << std::endl;
    return 0;
}
```

- [ ] **Step 2: Run configure to verify the scaffold is valid**

Run:

```bash
cmake -S . -B build
```

Expected:
- Configure succeeds
- Generated build files appear in `build/`

- [ ] **Step 3: Run the build to verify the skeleton compiles**

Run:

```bash
cmake --build build --config Release
```

Expected:
- `CH341Locator` builds successfully
- `ch341locator_demo` builds successfully

- [ ] **Step 4: Commit the scaffold**

Run:

```bash
git add CMakeLists.txt include/CH341Locator.h src/CH341Locator.cpp examples/ch341locator_demo.cpp
git commit -m "build: scaffold CH341Locator DLL project"
```

## Task 2: Implement CH341 Enumeration Helpers

**Files:**
- Modify: `src/CH341Locator.cpp`
- Test: `examples/ch341locator_demo.cpp`

- [ ] **Step 1: Write the failing implementation target**

Replace `src/CH341Locator.cpp` with helper declarations and partial implementation:

```cpp
#include "CH341Locator.h"

#include <setupapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")

namespace {

bool GetHardwareId(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData, std::wstring& hardwareId);
bool IsTargetCh341HardwareId(const std::wstring& hardwareId);
bool CollectPresentCh341DeviceIndexes(std::vector<DWORD>& deviceIndexes);

bool GetHardwareId(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData, std::wstring& hardwareId) {
    wchar_t buffer[1024] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;

    if (!SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_HARDWAREID,
            &propertyType,
            reinterpret_cast<PBYTE>(buffer),
            sizeof(buffer),
            &requiredSize)) {
        return false;
    }

    hardwareId.assign(buffer);
    return !hardwareId.empty();
}

bool IsTargetCh341HardwareId(const std::wstring& hardwareId) {
    return hardwareId.find(L"VID_1A86") != std::wstring::npos &&
           (hardwareId.find(L"PID_5512") != std::wstring::npos ||
            hardwareId.find(L"PID_5523") != std::wstring::npos);
}

bool CollectPresentCh341DeviceIndexes(std::vector<DWORD>& deviceIndexes) {
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(deviceInfoSet, index, &deviceInfoData); ++index) {
        std::wstring hardwareId;
        if (!GetHardwareId(deviceInfoSet, deviceInfoData, hardwareId)) {
            continue;
        }

        if (IsTargetCh341HardwareId(hardwareId)) {
            deviceIndexes.push_back(index);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return true;
}

}  // namespace

DWORD WINAPI GetCh341DeviceCount(void) {
    std::vector<DWORD> deviceIndexes;
    if (!CollectPresentCh341DeviceIndexes(deviceIndexes)) {
        return 0;
    }
    return static_cast<DWORD>(deviceIndexes.size());
}

BOOL WINAPI GetCh341LocationPaths(ULONG iIndex, LPWSTR buffer, DWORD* bufferSize) {
    (void)iIndex;
    (void)buffer;
    (void)bufferSize;
    return FALSE;
}

BOOL WINAPI GetCh341IndexByLocationPath(LPCWSTR locationPath, PULONG pIndex) {
    (void)locationPath;
    (void)pIndex;
    return FALSE;
}
```

- [ ] **Step 2: Build to confirm helper wiring compiles**

Run:

```bash
cmake --build build --config Release
```

Expected:
- Build succeeds
- No unresolved SetupAPI symbols

- [ ] **Step 3: Commit the enumeration helper layer**

Run:

```bash
git add src/CH341Locator.cpp
git commit -m "feat: add CH341 device enumeration helpers"
```

## Task 3: Implement Location Path Query

**Files:**
- Modify: `src/CH341Locator.cpp`
- Modify: `examples/ch341locator_demo.cpp`

- [ ] **Step 1: Add helper code for location path retrieval**

Extend `src/CH341Locator.cpp` by replacing the helper area with this full implementation:

```cpp
#include "CH341Locator.h"

#include <setupapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")

namespace {

bool GetHardwareId(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData, std::wstring& hardwareId) {
    wchar_t buffer[1024] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;

    if (!SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfoData,
            SPDRP_HARDWAREID,
            &propertyType,
            reinterpret_cast<PBYTE>(buffer),
            sizeof(buffer),
            &requiredSize)) {
        return false;
    }

    hardwareId.assign(buffer);
    return !hardwareId.empty();
}

bool IsTargetCh341HardwareId(const std::wstring& hardwareId) {
    return hardwareId.find(L"VID_1A86") != std::wstring::npos &&
           (hardwareId.find(L"PID_5512") != std::wstring::npos ||
            hardwareId.find(L"PID_5523") != std::wstring::npos);
}

bool ReadMultiSzProperty(
    HDEVINFO deviceInfoSet,
    SP_DEVINFO_DATA& deviceInfoData,
    DWORD property,
    std::vector<std::wstring>& values) {
    wchar_t buffer[4096] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;

    if (!SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfoData,
            property,
            &propertyType,
            reinterpret_cast<PBYTE>(buffer),
            sizeof(buffer),
            &requiredSize)) {
        return false;
    }

    const wchar_t* current = buffer;
    while (*current != L'\0') {
        values.emplace_back(current);
        current += values.back().size() + 1;
    }
    return !values.empty();
}

bool ReadSingleStringProperty(
    HDEVINFO deviceInfoSet,
    SP_DEVINFO_DATA& deviceInfoData,
    DWORD property,
    std::vector<std::wstring>& values) {
    wchar_t buffer[1024] = {};
    DWORD propertyType = 0;
    DWORD requiredSize = 0;

    if (!SetupDiGetDeviceRegistryPropertyW(
            deviceInfoSet,
            &deviceInfoData,
            property,
            &propertyType,
            reinterpret_cast<PBYTE>(buffer),
            sizeof(buffer),
            &requiredSize)) {
        return false;
    }

    if (buffer[0] == L'\0') {
        return false;
    }

    values.emplace_back(buffer);
    return true;
}

bool GetLocationPathsForDevice(
    HDEVINFO deviceInfoSet,
    SP_DEVINFO_DATA& deviceInfoData,
    std::vector<std::wstring>& values) {
    values.clear();
    if (ReadMultiSzProperty(deviceInfoSet, deviceInfoData, SPDRP_LOCATION_PATHS, values)) {
        return true;
    }

    values.clear();
    return ReadSingleStringProperty(deviceInfoSet, deviceInfoData, SPDRP_LOCATION_INFORMATION, values);
}

bool FindNthCh341Device(
    ULONG requestedIndex,
    SP_DEVINFO_DATA& resultDeviceInfoData,
    HDEVINFO& resultDeviceInfoSet) {
    resultDeviceInfoSet = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (resultDeviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    ULONG currentCh341Index = 0;
    for (DWORD enumIndex = 0; SetupDiEnumDeviceInfo(resultDeviceInfoSet, enumIndex, &deviceInfoData); ++enumIndex) {
        std::wstring hardwareId;
        if (!GetHardwareId(resultDeviceInfoSet, deviceInfoData, hardwareId)) {
            continue;
        }

        if (!IsTargetCh341HardwareId(hardwareId)) {
            continue;
        }

        if (currentCh341Index == requestedIndex) {
            resultDeviceInfoData = deviceInfoData;
            return true;
        }

        ++currentCh341Index;
    }

    SetupDiDestroyDeviceInfoList(resultDeviceInfoSet);
    resultDeviceInfoSet = INVALID_HANDLE_VALUE;
    return false;
}

DWORD ComputeMultiSzBytes(const std::vector<std::wstring>& values) {
    DWORD totalBytes = sizeof(wchar_t);
    for (const auto& value : values) {
        totalBytes += static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    }
    return totalBytes;
}

bool PackMultiSz(const std::vector<std::wstring>& values, LPWSTR buffer, DWORD bufferSizeBytes) {
    DWORD requiredBytes = ComputeMultiSzBytes(values);
    if (buffer == nullptr || bufferSizeBytes < requiredBytes) {
        return false;
    }

    wchar_t* writeCursor = buffer;
    for (const auto& value : values) {
        memcpy(writeCursor, value.c_str(), value.size() * sizeof(wchar_t));
        writeCursor += value.size();
        *writeCursor++ = L'\0';
    }
    *writeCursor = L'\0';
    return true;
}

}  // namespace

DWORD WINAPI GetCh341DeviceCount(void) {
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD count = 0;
    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(deviceInfoSet, index, &deviceInfoData); ++index) {
        std::wstring hardwareId;
        if (!GetHardwareId(deviceInfoSet, deviceInfoData, hardwareId)) {
            continue;
        }
        if (IsTargetCh341HardwareId(hardwareId)) {
            ++count;
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return count;
}

BOOL WINAPI GetCh341LocationPaths(ULONG iIndex, LPWSTR buffer, DWORD* bufferSize) {
    if (bufferSize == nullptr) {
        return FALSE;
    }

    HDEVINFO deviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    if (!FindNthCh341Device(iIndex, deviceInfoData, deviceInfoSet)) {
        return FALSE;
    }

    std::vector<std::wstring> values;
    const bool found = GetLocationPathsForDevice(deviceInfoSet, deviceInfoData, values);
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!found || values.empty()) {
        return FALSE;
    }

    const DWORD requiredBytes = ComputeMultiSzBytes(values);
    *bufferSize = requiredBytes;

    if (buffer == nullptr) {
        return FALSE;
    }

    if (!PackMultiSz(values, buffer, requiredBytes)) {
        return FALSE;
    }

    return TRUE;
}

BOOL WINAPI GetCh341IndexByLocationPath(LPCWSTR locationPath, PULONG pIndex) {
    (void)locationPath;
    (void)pIndex;
    return FALSE;
}
```

- [ ] **Step 2: Update the demo to query the buffer size**

Replace `examples/ch341locator_demo.cpp`:

```cpp
#include <iostream>
#include <string>
#include <vector>

#include "CH341Locator.h"

int wmain() {
    const DWORD count = GetCh341DeviceCount();
    std::wcout << L"CH341 count: " << count << std::endl;

    for (DWORD index = 0; index < count; ++index) {
        DWORD bytesNeeded = 0;
        if (GetCh341LocationPaths(index, nullptr, &bytesNeeded) || bytesNeeded == 0) {
            std::wcout << L"Device " << index << L": failed to query size" << std::endl;
            continue;
        }

        std::vector<wchar_t> buffer(bytesNeeded / sizeof(wchar_t), L'\0');
        if (!GetCh341LocationPaths(index, buffer.data(), &bytesNeeded)) {
            std::wcout << L"Device " << index << L": failed to read paths" << std::endl;
            continue;
        }

        std::wcout << L"Device " << index << L" paths:" << std::endl;
        const wchar_t* current = buffer.data();
        while (*current != L'\0') {
            std::wcout << L"  " << current << std::endl;
            current += wcslen(current) + 1;
        }
    }

    return 0;
}
```

- [ ] **Step 3: Build to verify path query implementation**

Run:

```bash
cmake --build build --config Release
```

Expected:
- Build succeeds
- Demo links against the DLL target

- [ ] **Step 4: Commit the path query implementation**

Run:

```bash
git add src/CH341Locator.cpp examples/ch341locator_demo.cpp
git commit -m "feat: add CH341 location path query"
```

## Task 4: Fix Buffer Size Handling And Implement Reverse Lookup

**Files:**
- Modify: `src/CH341Locator.cpp`
- Modify: `examples/ch341locator_demo.cpp`

- [ ] **Step 1: Correct `GetCh341LocationPaths` buffer handling**

In `src/CH341Locator.cpp`, replace the body of `GetCh341LocationPaths` with this exact version:

```cpp
BOOL WINAPI GetCh341LocationPaths(ULONG iIndex, LPWSTR buffer, DWORD* bufferSize) {
    if (bufferSize == nullptr) {
        return FALSE;
    }

    HDEVINFO deviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    if (!FindNthCh341Device(iIndex, deviceInfoData, deviceInfoSet)) {
        return FALSE;
    }

    std::vector<std::wstring> values;
    const bool found = GetLocationPathsForDevice(deviceInfoSet, deviceInfoData, values);
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!found || values.empty()) {
        return FALSE;
    }

    const DWORD requiredBytes = ComputeMultiSzBytes(values);
    *bufferSize = requiredBytes;

    if (buffer == nullptr) {
        return FALSE;
    }

    if (*bufferSize < requiredBytes) {
        *bufferSize = requiredBytes;
        return FALSE;
    }

    if (!PackMultiSz(values, buffer, *bufferSize)) {
        return FALSE;
    }

    return TRUE;
}
```

- [ ] **Step 2: Implement `GetCh341IndexByLocationPath`**

In `src/CH341Locator.cpp`, replace the stub with this exact function:

```cpp
BOOL WINAPI GetCh341IndexByLocationPath(LPCWSTR locationPath, PULONG pIndex) {
    if (locationPath == nullptr || locationPath[0] == L'\0' || pIndex == nullptr) {
        return FALSE;
    }

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    SP_DEVINFO_DATA deviceInfoData{};
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    ULONG currentCh341Index = 0;
    for (DWORD enumIndex = 0; SetupDiEnumDeviceInfo(deviceInfoSet, enumIndex, &deviceInfoData); ++enumIndex) {
        std::wstring hardwareId;
        if (!GetHardwareId(deviceInfoSet, deviceInfoData, hardwareId)) {
            continue;
        }

        if (!IsTargetCh341HardwareId(hardwareId)) {
            continue;
        }

        std::vector<std::wstring> values;
        if (GetLocationPathsForDevice(deviceInfoSet, deviceInfoData, values)) {
            for (const auto& value : values) {
                if (value == locationPath) {
                    *pIndex = currentCh341Index;
                    SetupDiDestroyDeviceInfoList(deviceInfoSet);
                    return TRUE;
                }
            }
        }

        ++currentCh341Index;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return FALSE;
}
```

- [ ] **Step 3: Update the demo to exercise reverse lookup**

Replace `examples/ch341locator_demo.cpp`:

```cpp
#include <iostream>
#include <string>
#include <vector>

#include "CH341Locator.h"

int wmain() {
    const DWORD count = GetCh341DeviceCount();
    std::wcout << L"CH341 count: " << count << std::endl;

    for (DWORD index = 0; index < count; ++index) {
        DWORD bytesNeeded = 0;
        GetCh341LocationPaths(index, nullptr, &bytesNeeded);
        if (bytesNeeded == 0) {
            std::wcout << L"Device " << index << L": no paths" << std::endl;
            continue;
        }

        std::vector<wchar_t> buffer(bytesNeeded / sizeof(wchar_t), L'\0');
        DWORD bufferBytes = bytesNeeded;
        if (!GetCh341LocationPaths(index, buffer.data(), &bufferBytes)) {
            std::wcout << L"Device " << index << L": failed to read paths" << std::endl;
            continue;
        }

        std::wcout << L"Device " << index << L" paths:" << std::endl;
        const wchar_t* current = buffer.data();
        bool triedReverseLookup = false;
        while (*current != L'\0') {
            std::wcout << L"  " << current << std::endl;

            if (!triedReverseLookup) {
                ULONG resolvedIndex = 0;
                if (GetCh341IndexByLocationPath(current, &resolvedIndex)) {
                    std::wcout << L"  resolved index: " << resolvedIndex << std::endl;
                } else {
                    std::wcout << L"  reverse lookup failed" << std::endl;
                }
                triedReverseLookup = true;
            }

            current += wcslen(current) + 1;
        }
    }

    return 0;
}
```

- [ ] **Step 4: Build to verify reverse lookup**

Run:

```bash
cmake --build build --config Release
```

Expected:
- Build succeeds
- DLL exports all three functions

- [ ] **Step 5: Commit reverse lookup**

Run:

```bash
git add src/CH341Locator.cpp examples/ch341locator_demo.cpp
git commit -m "feat: add reverse lookup by location path"
```

## Task 5: Refine CMake Output Layout For Packaging

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Update `CMakeLists.txt` for predictable output directories**

Replace `CMakeLists.txt` with this exact version:

```cmake
cmake_minimum_required(VERSION 3.20)

project(CH341Locator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(CH341Locator SHARED
    src/CH341Locator.cpp
)

target_compile_definitions(CH341Locator
    PRIVATE
        CH341LOCATOR_EXPORTS
        UNICODE
        _UNICODE
)

target_include_directories(CH341Locator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(CH341Locator
    PRIVATE
        setupapi
)

set_target_properties(CH341Locator PROPERTIES
    OUTPUT_NAME "CH341Locator"
)

add_executable(ch341locator_demo
    examples/ch341locator_demo.cpp
)

target_compile_definitions(ch341locator_demo
    PRIVATE
        UNICODE
        _UNICODE
)

target_include_directories(ch341locator_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ch341locator_demo
    PRIVATE
        CH341Locator
)
```

- [ ] **Step 2: Reconfigure and rebuild**

Run:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Expected:
- `build/bin/` contains `CH341Locator.dll` and `ch341locator_demo.exe`
- `build/lib/` contains `CH341Locator.lib`

- [ ] **Step 3: Commit the packaging-friendly build layout**

Run:

```bash
git add CMakeLists.txt
git commit -m "build: standardize output directories"
```

## Task 6: Write The README

**Files:**
- Create: `README.md`

- [ ] **Step 1: Add the top-level README**

Create `README.md`:

```md
# CH341Locator

`CH341Locator` is a small Windows DLL for locating CH341 devices through SetupAPI. It exposes a C-compatible ABI so other programs can count CH341 devices, read their USB location paths, and resolve a current device index from a saved location path.

## Exported APIs

```c
DWORD WINAPI GetCh341DeviceCount(void);

BOOL WINAPI GetCh341LocationPaths(
    ULONG iIndex,
    LPWSTR buffer,
    DWORD* bufferSize
);

BOOL WINAPI GetCh341IndexByLocationPath(
    LPCWSTR locationPath,
    PULONG pIndex
);
```

## Build

### Configure

```powershell
cmake -S . -B build
```

### Build

```powershell
cmake --build build --config Release
```

## Output

- `build/bin/CH341Locator.dll`
- `build/bin/ch341locator_demo.exe`
- `build/lib/CH341Locator.lib`

## How `GetCh341LocationPaths` Works

- Input index is a dynamic CH341 index, starting at `0`
- Output is UTF-16 `REG_MULTI_SZ` style data
- Individual paths are separated by `\0`
- The list ends with an extra `\0`
- Call once with `buffer = NULL` to get the required size in bytes

## Demo

The demo prints:

- detected CH341 count
- every location path for each device
- reverse lookup result for the first returned path

## Packaging

CI packages these files for `Win32` and `x64`:

- `CH341Locator.dll`
- `CH341Locator.lib`
- `CH341Locator.h`
- `README.md`
- `ch341locator_demo.exe`

## CI Notes

### GitHub Actions

GitHub Actions uses a Windows runner and publishes `Win32` and `x64` artifacts.

### GitLab CI

GitLab CI configuration is included, but it requires a Windows GitLab Runner to execute successfully. Linux-only GitLab runners cannot build this project.
```

- [ ] **Step 2: Review the README for formatting**

Run:

```bash
sed -n '1,220p' README.md
```

Expected:
- Header renders correctly
- Code fences are balanced
- API signatures match the public header

- [ ] **Step 3: Commit the README**

Run:

```bash
git add README.md
git commit -m "docs: add project README"
```

## Task 7: Add GitHub Actions Packaging

**Files:**
- Create: `.github/workflows/build.yml`

- [ ] **Step 1: Add the GitHub Actions workflow**

Create `.github/workflows/build.yml`:

```yaml
name: build

on:
  push:
  pull_request:
  workflow_dispatch:

jobs:
  windows-build:
    runs-on: windows-latest
    strategy:
      fail-fast: false
      matrix:
        platform: [Win32, x64]

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Configure
        run: cmake -S . -B build -A ${{ matrix.platform }}

      - name: Build
        run: cmake --build build --config Release

      - name: Stage package
        shell: pwsh
        run: |
          $arch = "${{ matrix.platform }}"
          $packageDir = "package/$arch"
          New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
          Copy-Item "build/bin/Release/CH341Locator.dll" "$packageDir/"
          Copy-Item "build/bin/Release/ch341locator_demo.exe" "$packageDir/"
          Copy-Item "build/lib/Release/CH341Locator.lib" "$packageDir/"
          Copy-Item "include/CH341Locator.h" "$packageDir/"
          Copy-Item "README.md" "$packageDir/"

      - name: Archive package
        shell: pwsh
        run: |
          $arch = "${{ matrix.platform }}"
          $zipName = if ($arch -eq "Win32") { "CH341Locator-win32.zip" } else { "CH341Locator-x64.zip" }
          Compress-Archive -Path "package/$arch/*" -DestinationPath $zipName -Force

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: ${{ matrix.platform }}
          path: CH341Locator-*.zip
```

- [ ] **Step 2: Review workflow syntax**

Run:

```bash
sed -n '1,240p' .github/workflows/build.yml
```

Expected:
- YAML indentation is correct
- Matrix values are `Win32` and `x64`
- Packaged filenames are architecture-specific

- [ ] **Step 3: Commit the GitHub Actions workflow**

Run:

```bash
git add .github/workflows/build.yml
git commit -m "ci: add GitHub Actions Windows packaging"
```

## Task 8: Add GitLab CI Packaging

**Files:**
- Create: `.gitlab-ci.yml`

- [ ] **Step 1: Add the GitLab CI pipeline**

Create `.gitlab-ci.yml`:

```yaml
stages:
  - build

variables:
  BUILD_TYPE: "Release"

.build_template:
  stage: build
  tags:
    - windows
  script:
    - cmake -S . -B build -A %CMAKE_PLATFORM%
    - cmake --build build --config %BUILD_TYPE%
    - powershell -Command "New-Item -ItemType Directory -Force -Path package/%CMAKE_PLATFORM% | Out-Null"
    - powershell -Command "Copy-Item build/bin/%BUILD_TYPE%/CH341Locator.dll package/%CMAKE_PLATFORM%/"
    - powershell -Command "Copy-Item build/bin/%BUILD_TYPE%/ch341locator_demo.exe package/%CMAKE_PLATFORM%/"
    - powershell -Command "Copy-Item build/lib/%BUILD_TYPE%/CH341Locator.lib package/%CMAKE_PLATFORM%/"
    - powershell -Command "Copy-Item include/CH341Locator.h package/%CMAKE_PLATFORM%/"
    - powershell -Command "Copy-Item README.md package/%CMAKE_PLATFORM%/"
    - powershell -Command "Compress-Archive -Path package/%CMAKE_PLATFORM%/* -DestinationPath CH341Locator-%ARCHIVE_NAME%.zip -Force"
  artifacts:
    paths:
      - CH341Locator-*.zip

build:win32:
  extends: .build_template
  variables:
    CMAKE_PLATFORM: "Win32"
    ARCHIVE_NAME: "win32"

build:x64:
  extends: .build_template
  variables:
    CMAKE_PLATFORM: "x64"
    ARCHIVE_NAME: "x64"
```

- [ ] **Step 2: Review the GitLab CI file**

Run:

```bash
sed -n '1,240p' .gitlab-ci.yml
```

Expected:
- Pipeline has two Windows jobs
- Zip names match the GitHub artifact names
- No Linux-only shell syntax sneaks into Windows commands

- [ ] **Step 3: Commit the GitLab CI pipeline**

Run:

```bash
git add .gitlab-ci.yml
git commit -m "ci: add GitLab Windows packaging template"
```

## Task 9: Final Validation

**Files:**
- Modify if needed: `CMakeLists.txt`
- Modify if needed: `src/CH341Locator.cpp`
- Modify if needed: `examples/ch341locator_demo.cpp`
- Modify if needed: `README.md`
- Modify if needed: `.github/workflows/build.yml`
- Modify if needed: `.gitlab-ci.yml`

- [ ] **Step 1: Run a clean local configure**

Run:

```bash
rm -rf build
cmake -S . -B build
```

Expected:
- Configure succeeds from a clean tree

- [ ] **Step 2: Run a clean local build**

Run:

```bash
cmake --build build --config Release
```

Expected:
- DLL and demo both build successfully

- [ ] **Step 3: Check edited files for diagnostics**

Run the IDE diagnostics check for:

- `include/CH341Locator.h`
- `src/CH341Locator.cpp`
- `examples/ch341locator_demo.cpp`
- `CMakeLists.txt`
- `README.md`
- `.github/workflows/build.yml`
- `.gitlab-ci.yml`

Expected:
- No new diagnostics introduced by the final edits

- [ ] **Step 4: Review artifact paths against the actual generator output**

Run:

```bash
find build -maxdepth 3 -type f | sort
```

Expected:
- Confirm whether files land in `build/bin/Release` and `build/lib/Release` for multi-config generators
- If paths differ, update CI copy commands to match the actual output layout

- [ ] **Step 5: Commit any final fixes**

Run:

```bash
git add CMakeLists.txt include/CH341Locator.h src/CH341Locator.cpp examples/ch341locator_demo.cpp README.md .github/workflows/build.yml .gitlab-ci.yml
git commit -m "chore: finalize CH341Locator packaging and docs"
```

## Spec Coverage Check

- DLL ABI with exactly three public APIs is covered by Tasks 1, 3, and 4.
- CH341 SetupAPI detection logic is covered by Tasks 2 and 3.
- `REG_MULTI_SZ` output behavior is covered by Tasks 3 and 4.
- Reverse lookup by location path is covered by Task 4.
- CMake build and packaging-friendly output layout is covered by Tasks 1 and 5.
- README requirement is covered by Task 6.
- GitHub Actions `Win32` and `x64` packaging is covered by Task 7.
- GitLab CI Windows-runner packaging template is covered by Task 8.
- Final validation and path correction are covered by Task 9.

## Placeholder Scan

- No `TBD`, `TODO`, or deferred implementation markers remain in the plan.
- Every file path is explicit.
- Every command step contains an exact command.
- Every code-writing step contains concrete code.

## Type Consistency Check

- Public API names are consistent across header, README, demo, and CI packaging references.
- `GetCh341LocationPaths` consistently uses byte-sized `bufferSize`.
- `GetCh341IndexByLocationPath` consistently writes to `PULONG`.
- Artifact names consistently use `CH341Locator-win32.zip` and `CH341Locator-x64.zip`.
