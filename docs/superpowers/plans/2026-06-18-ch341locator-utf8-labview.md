# CH341Locator UTF-8 LabVIEW Extension Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add LabVIEW-friendly UTF-8 APIs that return a single location path string and resolve CH341 index from a UTF-8 location path.

**Architecture:** Keep the existing wide-string APIs unchanged and add two UTF-8 wrapper exports on top of the current internal wide-string logic. Reuse the existing device lookup and path resolution helpers, and isolate encoding conversion into small helper functions inside the DLL implementation file.

**Tech Stack:** C++17, Windows API, SetupAPI, CMake

---

## File Structure

- Modify: `include/CH341Locator.h`
  - Add two new exported UTF-8 function declarations.
- Modify: `src/CH341Locator.cpp`
  - Add UTF-8 conversion helpers and implement the two new exports.
- Modify: `README.md`
  - Document the UTF-8 APIs and recommend them for LabVIEW.
- Optional Modify: `examples/ch341locator_demo.cpp`
  - Only if useful to show UTF-8 API usage without making the demo noisy.

## Task 1: Extend The Public Header

**Files:**
- Modify: `include/CH341Locator.h`

- [ ] **Step 1: Add the new UTF-8 function declarations**

Replace `include/CH341Locator.h` with this exact content:

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

CH341LOCATOR_API BOOL WINAPI GetCh341LocationPathUtf8(
    ULONG iIndex,
    LPSTR buffer,
    DWORD* bufferSize
);

CH341LOCATOR_API BOOL WINAPI GetCh341IndexByLocationPathUtf8(
    LPCSTR locationPath,
    PULONG pIndex
);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Check the header for obvious type consistency**

Run:

```bash
sed -n '1,220p' include/CH341Locator.h
```

Expected:
- Existing prototypes remain unchanged
- New UTF-8 prototypes use `LPSTR` and `LPCSTR`
- Export macro style remains consistent

- [ ] **Step 3: Commit the header change**

Run:

```bash
git add include/CH341Locator.h
git commit -m "feat: add UTF-8 API declarations"
```

## Task 2: Add UTF-8 Conversion Helpers

**Files:**
- Modify: `src/CH341Locator.cpp`

- [ ] **Step 1: Add the UTF-8 helper functions**

In `src/CH341Locator.cpp`, add `#include <string>` if it is not already present and add these helper functions inside the anonymous namespace before the exported APIs:

```cpp
bool WideToUtf8(const std::wstring& input, std::string& output) {
    output.clear();

    if (input.empty()) {
        output.clear();
        return true;
    }

    const int requiredBytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        input.c_str(),
        static_cast<int>(input.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (requiredBytes <= 0) {
        return false;
    }

    output.resize(static_cast<size_t>(requiredBytes));
    const int convertedBytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        input.c_str(),
        static_cast<int>(input.size()),
        output.data(),
        requiredBytes,
        nullptr,
        nullptr
    );

    return convertedBytes == requiredBytes;
}

bool Utf8ToWide(const std::string& input, std::wstring& output) {
    output.clear();

    if (input.empty()) {
        output.clear();
        return true;
    }

    const int requiredChars = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.c_str(),
        static_cast<int>(input.size()),
        nullptr,
        0
    );

    if (requiredChars <= 0) {
        return false;
    }

    output.resize(static_cast<size_t>(requiredChars));
    const int convertedChars = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.c_str(),
        static_cast<int>(input.size()),
        output.data(),
        requiredChars
    );

    return convertedChars == requiredChars;
}

bool PackUtf8String(const std::string& value, LPSTR buffer, DWORD* bufferSize) {
    if (bufferSize == nullptr) {
        return false;
    }

    const DWORD requiredBytes = static_cast<DWORD>(value.size() + 1);
    const DWORD callerBytes = *bufferSize;
    *bufferSize = requiredBytes;

    if (buffer == nullptr) {
        return false;
    }

    if (callerBytes < requiredBytes) {
        return false;
    }

    memcpy(buffer, value.c_str(), value.size());
    buffer[value.size()] = '\0';
    return true;
}
```

- [ ] **Step 2: Review the helper section**

Run:

```bash
sed -n '1,260p' src/CH341Locator.cpp
```

Expected:
- Helper functions sit inside the anonymous namespace
- Conversion uses `CP_UTF8`
- Buffer packing reports byte size including the null terminator

- [ ] **Step 3: Commit the helper layer**

Run:

```bash
git add src/CH341Locator.cpp
git commit -m "feat: add UTF-8 conversion helpers"
```

## Task 3: Implement `GetCh341LocationPathUtf8`

**Files:**
- Modify: `src/CH341Locator.cpp`

- [ ] **Step 1: Add the UTF-8 forward lookup export**

In `src/CH341Locator.cpp`, append this exact function after the existing wide-string exports:

```cpp
BOOL WINAPI GetCh341LocationPathUtf8(ULONG iIndex, LPSTR buffer, DWORD* bufferSize) {
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

    std::string utf8Value;
    if (!WideToUtf8(values.front(), utf8Value)) {
        return FALSE;
    }

    return PackUtf8String(utf8Value, buffer, bufferSize) ? TRUE : FALSE;
}
```

- [ ] **Step 2: Review the function in context**

Run:

```bash
sed -n '220,380p' src/CH341Locator.cpp
```

Expected:
- Function uses `values.front()` only
- Function reuses existing CH341 device/path lookup logic
- No existing wide-string API behavior changes

- [ ] **Step 3: Commit the UTF-8 forward lookup**

Run:

```bash
git add src/CH341Locator.cpp
git commit -m "feat: add UTF-8 location path lookup"
```

## Task 4: Implement `GetCh341IndexByLocationPathUtf8`

**Files:**
- Modify: `src/CH341Locator.cpp`

- [ ] **Step 1: Add the UTF-8 reverse lookup export**

In `src/CH341Locator.cpp`, append this exact function after `GetCh341LocationPathUtf8`:

```cpp
BOOL WINAPI GetCh341IndexByLocationPathUtf8(LPCSTR locationPath, PULONG pIndex) {
    if (locationPath == nullptr || locationPath[0] == '\0' || pIndex == nullptr) {
        return FALSE;
    }

    std::wstring widePath;
    if (!Utf8ToWide(std::string(locationPath), widePath)) {
        return FALSE;
    }

    return GetCh341IndexByLocationPath(widePath.c_str(), pIndex);
}
```

- [ ] **Step 2: Review the reverse lookup implementation**

Run:

```bash
sed -n '240,420p' src/CH341Locator.cpp
```

Expected:
- Reverse lookup is a thin wrapper over the existing wide-string reverse lookup
- Input validation happens before conversion
- No duplicate CH341 enumeration logic is added

- [ ] **Step 3: Commit the UTF-8 reverse lookup**

Run:

```bash
git add src/CH341Locator.cpp
git commit -m "feat: add UTF-8 reverse lookup"
```

## Task 5: Update The README

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add UTF-8 API documentation for LabVIEW callers**

Update `README.md` so the exported API section becomes:

```md
## 导出 API

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

BOOL WINAPI GetCh341LocationPathUtf8(
    ULONG iIndex,
    LPSTR buffer,
    DWORD* bufferSize
);

BOOL WINAPI GetCh341IndexByLocationPathUtf8(
    LPCSTR locationPath,
    PULONG pIndex
);
```
```

Add this new section after the existing `GetCh341LocationPaths` usage section:

```md
## LabVIEW 推荐接口

如果你是在 LabVIEW 里调用，推荐优先使用 UTF-8 接口：

- `GetCh341LocationPathUtf8`
- `GetCh341IndexByLocationPathUtf8`

原因：

- 返回的是普通单字符串，不是 UTF-16 `REG_MULTI_SZ`
- 不会出现大量 `00` 导致显示为空的问题
- 更适合 LabVIEW 直接接字符串缓冲区

`GetCh341LocationPathUtf8` 的调用方式：

1. 第一次传入 `buffer = NULL`，`bufferSize = 0`
2. DLL 返回所需字节数
3. 分配对应长度的字符串/字节缓冲区
4. 第二次调用获取实际 UTF-8 路径

`GetCh341IndexByLocationPathUtf8` 可直接传入上一步拿到的 UTF-8 字符串做反查。
```

- [ ] **Step 2: Review the README formatting**

Run:

```bash
sed -n '1,260p' README.md
```

Expected:
- New API signatures render correctly
- The LabVIEW recommendation is clear
- Existing build and packaging sections remain intact

- [ ] **Step 3: Commit the README update**

Run:

```bash
git add README.md
git commit -m "docs: add UTF-8 API usage for LabVIEW"
```

## Task 6: Final Validation

**Files:**
- Modify if needed: `include/CH341Locator.h`
- Modify if needed: `src/CH341Locator.cpp`
- Modify if needed: `README.md`

- [ ] **Step 1: Check edited files for diagnostics**

Run the IDE diagnostics check for:

- `include/CH341Locator.h`
- `src/CH341Locator.cpp`
- `README.md`

Expected:
- No new diagnostics beyond the known platform-specific Windows SDK limitation on non-Windows hosts

- [ ] **Step 2: Confirm the repo diff is limited to the intended files**

Run:

```bash
git status --short
```

Expected:
- Only the header, implementation, and README are modified for this feature

- [ ] **Step 3: Commit any final cleanup**

Run:

```bash
git add include/CH341Locator.h src/CH341Locator.cpp README.md
git commit -m "chore: finalize UTF-8 LabVIEW API extension"
```

## Spec Coverage Check

- Header exports for the two new UTF-8 APIs are covered by Task 1.
- UTF-8 conversion helpers are covered by Task 2.
- Forward UTF-8 path lookup is covered by Task 3.
- Reverse UTF-8 path lookup is covered by Task 4.
- README guidance for LabVIEW callers is covered by Task 5.
- Validation and containment of changes are covered by Task 6.

## Placeholder Scan

- No placeholder markers remain.
- Every file path is explicit.
- Every command is concrete.
- Every code-writing step contains exact code.

## Type Consistency Check

- `GetCh341LocationPathUtf8` consistently uses `LPSTR` + `DWORD* bufferSize`.
- `GetCh341IndexByLocationPathUtf8` consistently uses `LPCSTR` + `PULONG`.
- UTF-8 reverse lookup reuses `GetCh341IndexByLocationPath`.
- Existing wide-string APIs remain unchanged.
