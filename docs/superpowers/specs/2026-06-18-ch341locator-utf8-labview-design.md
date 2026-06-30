# CH341Locator UTF-8 LabVIEW Extension Design

## Overview

This design adds a LabVIEW-friendly UTF-8 string API on top of the existing CH341 locator DLL. The current exported API uses `LPWSTR` and returns `REG_MULTI_SZ`-style UTF-16 data, which is correct for native Windows callers but inconvenient for LabVIEW string handling.

The extension keeps the existing ABI intact and adds two new narrow-string APIs:

- `GetCh341LocationPathUtf8`
- `GetCh341IndexByLocationPathUtf8`

These functions are intended to support tools that prefer a single UTF-8 string instead of UTF-16 multi-string buffers.

## Goals

- Keep all current exported wide-string APIs unchanged.
- Add a simple UTF-8 API that returns the first location path as a normal null-terminated string.
- Add a UTF-8 reverse lookup API that accepts a normal UTF-8 string and resolves the CH341 dynamic index.
- Preserve the current two-call buffer size pattern used elsewhere in the DLL.
- Make the new APIs easy to call from LabVIEW and other FFI consumers that expect byte strings.

## Non-Goals

- No removal or breaking change to existing `LPWSTR` APIs.
- No UTF-8 multi-string output format.
- No array-returning API for all location paths in UTF-8 for this iteration.
- No change to the underlying CH341 detection logic.

## Public API Additions

The public header will be extended with two new exports:

```c
CH341LOCATOR_API BOOL WINAPI GetCh341LocationPathUtf8(
    ULONG iIndex,
    LPSTR buffer,
    DWORD* bufferSize
);

CH341LOCATOR_API BOOL WINAPI GetCh341IndexByLocationPathUtf8(
    LPCSTR locationPath,
    PULONG pIndex
);
```

## API Contract

### `GetCh341LocationPathUtf8`

- Input:
  - `iIndex`: zero-based CH341 dynamic index.
  - `buffer`: output buffer for a UTF-8 string. May be `NULL` for size query.
  - `bufferSize`: input/output byte count. Must not be `NULL`.
- Behavior:
  - Resolves the target CH341 device by dynamic index.
  - Reads the existing location path list using the current internal wide-string logic.
  - Chooses the first location path only.
  - Converts that first path from UTF-16 to UTF-8.
  - Writes a standard null-terminated UTF-8 string.
- Return behavior:
  - Returns `TRUE` when the UTF-8 string is successfully written.
  - Returns `FALSE` when `buffer` is `NULL`, the buffer is too small, the device is missing, or no location path is available.
  - On size-query or buffer-too-small cases, `bufferSize` reports the required byte count including the trailing `'\0'`.

### `GetCh341IndexByLocationPathUtf8`

- Input:
  - `locationPath`: a UTF-8 null-terminated string.
  - `pIndex`: output pointer for the matched dynamic index.
- Behavior:
  - Converts the UTF-8 string to UTF-16.
  - Reuses the existing wide-string reverse lookup logic.
- Return behavior:
  - Returns `TRUE` on successful match.
  - Returns `FALSE` for invalid input, conversion failure, enumeration failure, or no match.

## Why First Path Only

The current wide-string API can return multiple location paths because Windows may expose more than one path-like property. For LabVIEW integration, the practical need is a stable, displayable, round-trippable string for one device. Returning only the first path keeps the interface simple and avoids reintroducing the same multi-string handling problem in UTF-8 form.

This design explicitly treats the first returned path as the canonical UTF-8 path for LabVIEW callers.

## Internal Implementation

The existing internal wide-string helpers remain the source of truth. The UTF-8 APIs will be thin wrappers around them.

### New helper functions

- `bool WideToUtf8(const std::wstring& input, std::string& output);`
  - Uses `WideCharToMultiByte(CP_UTF8, ...)`.
- `bool Utf8ToWide(const std::string& input, std::wstring& output);`
  - Uses `MultiByteToWideChar(CP_UTF8, ...)`.
- `bool PackUtf8String(const std::string& value, LPSTR buffer, DWORD* bufferSize);`
  - Reports required byte count including terminator.
  - Writes a single null-terminated byte string.

### `GetCh341LocationPathUtf8` flow

1. Validate `bufferSize`.
2. Reuse `FindNthCh341Device(...)`.
3. Reuse `GetLocationPathsForDevice(...)`.
4. If no path exists, return `FALSE`.
5. Convert `values.front()` from UTF-16 to UTF-8.
6. If `buffer == NULL`, return `FALSE` after setting required size.
7. If caller buffer is too small, return `FALSE` after setting required size.
8. Copy UTF-8 bytes and append `'\0'`.

### `GetCh341IndexByLocationPathUtf8` flow

1. Validate `locationPath` and `pIndex`.
2. Convert the UTF-8 input to UTF-16.
3. Call the existing `GetCh341IndexByLocationPath(...)`.
4. Return the result directly.

## Buffer Rules

The new UTF-8 output API follows the same size-query style as the wide output API:

- `buffer = NULL` means "tell me how many bytes are needed"
- required size always includes the trailing null terminator
- caller allocates the buffer
- second call writes the actual string

This keeps the API consistent across the DLL while still being simple for LabVIEW.

## LabVIEW Compatibility

The new UTF-8 API avoids the main issues seen with the existing `LPWSTR` API:

- no UTF-16 buffer interpretation problem
- no embedded null separators from `REG_MULTI_SZ`
- no need to parse multi-string output
- easy display as a normal string control

LabVIEW callers can now:

1. call `GetCh341LocationPathUtf8` once to get byte length
2. allocate a byte/string buffer
3. call it again to get a displayable string
4. pass that string into `GetCh341IndexByLocationPathUtf8`

## Files To Change

- `include/CH341Locator.h`
  - add the two new exported function declarations
- `src/CH341Locator.cpp`
  - add UTF-8 conversion helpers
  - add the two new exported function implementations
- `README.md`
  - document the new UTF-8 APIs and recommend them for LabVIEW callers
- `examples/ch341locator_demo.cpp`
  - optional: extend demo output to print the UTF-8 path too if useful

## Error Handling

- Invalid pointers return `FALSE`.
- Empty strings return `FALSE`.
- UTF-8/UTF-16 conversion failure returns `FALSE`.
- No CH341 match returns `FALSE`.
- Missing location path returns `FALSE`.
- On output size query or insufficient buffer, `bufferSize` still reports required size.

## Testing Strategy

### Build-level verification

- Ensure the header compiles with the two new prototypes.
- Ensure the DLL exports the new functions without changing existing names.

### Manual runtime verification on Windows

- Call `GetCh341LocationPathUtf8(index, NULL, &size)` and verify `size > 0`.
- Allocate `size` bytes and call again; verify a readable ASCII/UTF-8 path string appears.
- Feed that string into `GetCh341IndexByLocationPathUtf8` and verify the same index is returned.
- Confirm the existing wide-string APIs still behave unchanged.

## Risks And Mitigations

### Multiple path ambiguity

Risk:
- A device may expose more than one path entry.

Mitigation:
- Use the first path only for the UTF-8 API and document that choice clearly.

### Conversion mismatch

Risk:
- A caller may pass non-UTF-8 data.

Mitigation:
- Fail conversion cleanly and return `FALSE`.

### Divergence from wide-string logic

Risk:
- Future changes might accidentally make UTF-8 and UTF-16 lookups behave differently.

Mitigation:
- Keep UTF-8 reverse lookup as a wrapper over the existing wide reverse lookup implementation.

## Acceptance Criteria

The extension is accepted when all of the following are true:

- the public header exports `GetCh341LocationPathUtf8`
- the public header exports `GetCh341IndexByLocationPathUtf8`
- the UTF-8 path API returns a single null-terminated UTF-8 string
- the UTF-8 path API supports size query through `buffer = NULL`
- the UTF-8 reverse lookup API resolves the same index from a previously returned UTF-8 path
- the existing wide-string APIs remain unchanged
- the README explains that LabVIEW should prefer the UTF-8 APIs
