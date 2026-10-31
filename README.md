# Java-BSNative-launcher

A small native launcher for Java apps. Copy it next to your app, rename it, and add an ini file. Your app then runs under its own executable name instead of `java` or `javaw`, with no console window, and finds a suitable Java by itself.

That matters to anything that identifies apps by executable, such as per-app profiles on stream decks, the firewall, and Task Manager.

```
StorageManager.exe    a renamed copy of the launcher
StorageManager.ini
lib/                  your jars
runtime/              optional bundled Java
```

```ini
main-class=com.example.Main
classpath=lib/*
min-java=21
```

Java runs inside the launcher's own process through the standard JNI invocation API, so the app's windows belong to your exe.

Every copy has the same generic play icon and the description "Java application", so nothing is patched and a signature would survive the rename. Your app's windows, and so its taskbar entry, and any shortcuts can have their own icons.

**Platforms:** Windows, Linux and macOS, each on x64 and ARM64.

See [docs/ini.md](docs/ini.md) for all settings, and [docs/packaging.md](docs/packaging.md) for the Maven plugin, which bundles every platform's binary.

## Building

Needs CMake, a C compiler (MinGW-w64 on Windows, gcc on Linux, Xcode tools on macOS), and `JAVA_HOME` pointing at a JDK for its JNI headers. GitHub Actions builds every push and keeps the binary as a build artifact.

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
pwsh test/integration.ps1 -Launcher build/launcher.exe -JavaHome $env:JAVA_HOME
```

## License

[Apache License 2.0](LICENSE)
