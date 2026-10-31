# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

A native launcher that's copied and renamed per app, reads `<name>.ini`, finds a JVM, and runs Java in its own process through `JNI_CreateJavaVM`. The binary is never patched per app. See `docs/ini.md`.

- `src/common/`: portable logic, all strings UTF-8. Keep platform calls behind the `plat_*` functions in `launcher.h`.
- `src/windows/`: wide-character APIs, converting at the boundary. Windows x64 is the only platform so far; Linux and Mac are planned, and Maven Central publishing and packaging from Maven are the long-term goal.
- Don't use `JLI_Launch`: it's an internal JDK API that assumes it runs from inside the JDK.

## Commands

No compiler may be installed locally. GitHub Actions builds every push (`gh run download` fetches `launcher-win-x64`).

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
pwsh test/integration.ps1 -Launcher build/launcher.exe -JavaHome $env:JAVA_HOME
```
