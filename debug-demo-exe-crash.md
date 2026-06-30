# debug-demo-exe-crash.md

Status: [OPEN]

## Symptom

- User reports that `demo.exe` from GitHub Actions artifacts flashes and exits immediately on Windows.
- User states the package contains `.exe`, `.dll`, and `.lib` together.

## Scope

- Affects the packaged demo executable from CI artifacts.
- Current focus is runtime behavior of `ch341locator_demo.exe`.

## Hypotheses

1. `H1`: The program is not crashing; it is a short-lived console app that exits immediately after printing output.
2. `H2`: Runtime startup fails because the process cannot locate a required dependency or initialization path, but the user only sees a flash due to double-click launch.
3. `H3`: The demo enters exported DLL code and crashes while reading device data or converting path strings.
4. `H4`: The packaged artifact layout or actual launch directory differs from expectations, causing runtime behavior to differ from local assumptions.

## Evidence Log

- User explicitly states the executable is launched from GitHub Actions artifacts and flashes immediately.
- No Windows error dialog text, console output, event log, or stack trace has been provided yet.
- New evidence from GitHub Actions shows the debug build fails before artifact generation:
  - `error C2712: Cannot use __try in functions that require object unwinding`
  - Location: `examples/ch341locator_demo.cpp`
  - This means the current debug-instrumented demo does not build under MSVC, so no runtime evidence can be collected from that build.

## Next Step

- Fix the demo instrumentation structure so it compiles on MSVC.
- Preserve startup trace logging.
- Rebuild in GitHub Actions and then collect `ch341locator_demo_trace.txt` from a real Windows run.
