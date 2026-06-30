# CH341Locator DLL Design

## Overview

This project builds a Windows dynamic library that exposes a small C-compatible API for locating CH341 devices and resolving their USB location paths. The DLL is intended to be called from other programs such as C/C++, C#, Python, or other FFI-capable environments.

The repository currently starts empty, so the first version focuses on a minimal, production-friendly deliverable:

- a native Windows DLL
- a public header for callers
- a minimal example program
- a top-level README
- GitHub Actions packaging for `Win32` and `x64`
- GitLab CI configuration for `Win32` and `x64` builds on a Windows runner

## Goals

- Expose a stable C ABI that is simple to call from multiple languages.
- Provide a way to count CH341 devices currently present in the system.
- Provide a way to query the `Location Paths` for a CH341 device by dynamic index.
- Provide a way to resolve a CH341 device index from a known `Location Path`.
- Package consumable build artifacts for both `Win32` and `x64`.
- Keep the implementation self-contained and free of external runtime dependencies beyond Windows system libraries.

## Non-Goals

- No GUI application in the first version.
- No device open/read/write API in the first version.
- No background watcher service or hotplug event callback API in the first version.
- No promise that device index is stable across unplug/replug or system reboot.
- No cross-platform support outside Windows.

## Primary Use Case

The main workflow is:

1. A caller enumerates how many CH341 devices are present.
2. The caller asks for the `Location Paths` for each index.
3. The caller persists the preferred path for later use.
4. On a later run, the caller resolves the current CH341 index by the persisted path.

This allows callers to identify a physical CH341 device by USB topology instead of relying on a volatile dynamic index.

## Public API

The DLL exports three functions through a C ABI and the `WINAPI` calling convention.

```c
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

## API Contract

### `GetCh341DeviceCount`

- Returns the number of currently present CH341 devices detected by SetupAPI.
- Returns `0` when no matching device is present or enumeration fails.

### `GetCh341LocationPaths`

- Input:
  - `iIndex`: zero-based dynamic CH341 device index.
  - `buffer`: output buffer for UTF-16 path data. May be `NULL` for size query.
  - `bufferSize`: input/output byte count. Must not be `NULL`.
- Output:
  - On success, writes a UTF-16 `REG_MULTI_SZ`-style string list.
  - Individual paths are separated by `L'\0'`.
  - The list ends with an additional `L'\0'`.
  - `bufferSize` receives the number of bytes written or required.
- Return behavior:
  - Returns `TRUE` when data is successfully written.
  - Returns `FALSE` if the device does not exist, if no path is available, or if the buffer is missing/too small.
  - When `buffer == NULL` or the buffer is too small, `bufferSize` still reports the required byte count.

### `GetCh341IndexByLocationPath`

- Input:
  - `locationPath`: a single UTF-16 location path string to match.
  - `pIndex`: receives the matching zero-based dynamic index.
- Output:
  - On success, writes the matched index into `pIndex`.
- Return behavior:
  - Returns `TRUE` on exact match.
  - Returns `FALSE` when the input is invalid, no device matches, or enumeration fails.

## Detection Strategy

The implementation uses Windows SetupAPI to enumerate present devices and filter CH341-related entries by hardware ID.

Initial filter rules:

- match `VID_1A86`
- accept `PID_5512`
- accept `PID_5523`

The code will search `SPDRP_HARDWAREID` and perform substring matching on the first hardware ID string.

This first version intentionally keeps the filter narrow and explicit to avoid matching unrelated devices.

## Location Path Resolution

For each matching device, the implementation resolves its location information in this order:

1. `SPDRP_LOCATION_PATHS`
2. `SPDRP_LOCATION_INFORMATION`

`SPDRP_LOCATION_PATHS` is preferred because it provides a more stable USB topology-oriented identifier. `SPDRP_LOCATION_INFORMATION` is a compatibility fallback for systems where the preferred property is unavailable.

If multiple location paths exist, all are returned from `GetCh341LocationPaths`. The reverse lookup API compares the caller-provided string against every returned path for every detected CH341 device.

## Internal Structure

The implementation should stay in a small set of focused helpers inside the DLL source:

- `GetHardwareId(...)`
  - Reads the first hardware ID string for a device.
- `IsTargetCh341HardwareId(...)`
  - Applies the VID/PID filter.
- `CollectPresentCh341Devices(...)`
  - Enumerates all present devices and keeps only matching CH341 entries.
- `GetLocationPathsForDevice(...)`
  - Reads and parses `SPDRP_LOCATION_PATHS`, then falls back to `SPDRP_LOCATION_INFORMATION`.
- `PackMultiSz(...)`
  - Packs multiple `std::wstring` values into caller-provided `REG_MULTI_SZ` layout.

The exported functions remain thin wrappers over these helpers.

## Error Handling

The exported APIs keep the contract intentionally simple:

- boolean success/failure for lookup functions
- count-or-zero for device count
- required output size reported through `bufferSize`

The first version does not expose a dedicated `GetLastError`-style DLL API. Internally, the code should still follow predictable failure paths:

- reject null required pointers
- reject out-of-range device index
- return `FALSE` if no location path is available
- avoid partial buffer writes

## ABI and Compatibility Rules

- Export only C symbols.
- Use `WINAPI` consistently.
- Use Windows scalar types such as `DWORD`, `BOOL`, `ULONG`, `LPWSTR`, and `LPCWSTR`.
- Keep memory ownership with the caller.
- Do not return STL containers, C++ classes, or heap-allocated buffers across the DLL boundary.
- Build with Unicode enabled.

These rules keep the DLL friendly to C#, Python `ctypes`, and other FFI consumers.

## Repository Layout

The repository should be created with the following structure:

```text
include/CH341Locator.h
src/CH341Locator.cpp
examples/ch341locator_demo.cpp
CMakeLists.txt
README.md
.github/workflows/build.yml
.gitlab-ci.yml
```

The example program is intentionally small and exists only to prove the public API works.

## Build System

The project uses CMake as the single source of build configuration.

Planned targets:

- `CH341Locator` shared library
- `ch341locator_demo` executable

Planned build properties:

- Unicode build
- C++17
- link against `setupapi.lib`
- install or package the public header with the build artifacts

## Packaging

Each architecture build should produce a zip package containing:

- `CH341Locator.dll`
- `CH341Locator.lib`
- `CH341Locator.h`
- `README.md`
- `ch341locator_demo.exe`

Suggested package names:

- `CH341Locator-win32.zip`
- `CH341Locator-x64.zip`

## GitHub Actions

GitHub Actions is expected to run on a Windows-hosted runner and build both architectures.

Workflow expectations:

- trigger on `push`, `pull_request`, and optionally `workflow_dispatch`
- matrix build for `Win32` and `x64`
- configure with CMake
- build in `Release`
- archive outputs into per-architecture zip files
- upload artifacts

## GitLab CI

The repository should also include a `.gitlab-ci.yml` that mirrors the same build intent for GitLab.

Important constraint:

- actual execution requires a Windows GitLab Runner with Visual Studio build tools or equivalent CMake-capable environment

The CI file will be committed as a ready-to-use template, but successful artifact generation depends on the user providing a compatible Windows runner.

## README Content

The README should cover:

- project purpose
- exported APIs
- CMake build steps
- basic example usage
- notes about `Location Paths`
- artifact contents
- GitHub Actions and GitLab CI notes

## Testing Strategy

The first version keeps testing lightweight and practical.

### Automated checks

- Build the DLL and example program for `Win32` and `x64`.
- Confirm the public header compiles from the demo target.

### Manual verification

- Run the demo on a Windows machine with zero CH341 devices.
- Run the demo on a Windows machine with one or more CH341 devices.
- Confirm that querying size with `buffer = NULL` reports the required byte count.
- Confirm that `GetCh341IndexByLocationPath` can resolve an index from a previously returned path.

## Risks and Mitigations

### Dynamic index instability

Risk:
- `iIndex` depends on enumeration order and is not a persistent identity.

Mitigation:
- document that callers should persist `Location Paths`, not indexes.

### Property availability differences

Risk:
- not every Windows environment exposes `SPDRP_LOCATION_PATHS` the same way.

Mitigation:
- implement fallback to `SPDRP_LOCATION_INFORMATION`.

### GitLab Windows runner availability

Risk:
- the committed GitLab CI config cannot run on Linux-only runners.

Mitigation:
- document the requirement clearly in both README and CI comments.

## Implementation Scope Boundary

This spec intentionally stops at a minimal DLL deliverable with packaging. The next phase may add:

- richer diagnostics
- case-insensitive path matching if needed
- more CH341 PID variants if real hardware testing shows they are needed
- a device watcher API
- a higher-level wrapper for managed languages

## Acceptance Criteria

The first implementation is accepted when all of the following are true:

- the repository builds a Windows DLL and a demo program via CMake
- the public header exposes exactly the three approved APIs
- `GetCh341LocationPaths` supports size-query and `REG_MULTI_SZ` output
- `GetCh341IndexByLocationPath` resolves a path returned by the DLL back to the current index
- GitHub Actions publishes `Win32` and `x64` artifacts
- `.gitlab-ci.yml` is present and ready for a Windows runner
- README explains build, usage, and packaging
