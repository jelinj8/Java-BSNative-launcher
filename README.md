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

The launcher doesn't change the exe's icon or version information, so it needs no patching and a signature would survive the rename. Your app's windows and any shortcuts can still have their own icons.

**Status:** Windows x64 only so far. Linux, Maven Central publishing, and Maven packaging are planned.

See [docs/ini.md](docs/ini.md) for all settings.

## Building

Needs CMake, MinGW-w64, and `JAVA_HOME` pointing at a JDK for its JNI headers. GitHub Actions builds every push and keeps the binary as a build artifact.

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
pwsh test/integration.ps1 -Launcher build/launcher.exe -JavaHome $env:JAVA_HOME
```

## License

[Apache License 2.0](LICENSE)
