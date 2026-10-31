# The ini file

The launcher reads `<its own name>.ini` from its own folder, leaving out any `.exe`. So `StorageManager.exe` or `StorageManager` reads `StorageManager.ini`. The file is UTF-8, with one `key=value` per line. Lines starting with `#` or `;` are comments; a `#` later in a line is part of the value. Unknown keys are errors, so typos don't go unnoticed.

| Key | Required | Meaning |
|---|---|---|
| `main-class` | Yes | The class with `public static void main(String[])` |
| `classpath` | Yes | Jars or folders. Repeatable, or separated by `;`. `lib/*` means every `.jar` in `lib`, sorted by name |
| `min-java` | No | Lowest allowed Java feature version, such as `21` |
| `max-java` | No | Highest allowed Java feature version |
| `runtime` | No | `auto` (default), or the path of the Java home to use |
| `jvm-option` | No | One JVM option per line. Repeatable |
| `arg` | No | One application argument per line, placed before command-line arguments. Repeatable |
| `app-id` | No | Windows taskbar grouping ID |
| `log-file` | No | File to append the app's output to. Folders are created |
| `working-dir` | No | The app's working directory: a path, or `inherit` to keep the caller's. Default: the launcher's folder |

Relative paths are relative to the launcher's folder. Values can use `${APP_DIR}` and `${ENV:NAME}`; a variable that isn't set becomes empty.

## Finding Java

With `runtime=auto`, the first of these with a usable Java wins, and the newest usable one within a source:

1. `runtime` folder next to the launcher
2. `JAVA_HOME`
3. Installed JDKs. On Windows, the registry: `SOFTWAREclipse AdoptiumJDK` and `SOFTWAREJavaSoftJDK`. On Linux: `/usr/lib/jvm`, `/usr/java`, `/opt/java` and `/opt`
4. `java` on `PATH`

A Java is usable if its `release` file gives a version within `min-java` and `max-java`, and an architecture matching the launcher. If none is usable, the launcher lists what it tried and why each was rejected.

## Good to know

- **JavaFX:** a main class that extends `Application` can start directly with JavaFX on the classpath, without the separate launcher class the `java` command needs. Tested with one real app.
- **Encoding:** on Windows, Java takes JVM options and the classpath in the system code page, and a path with characters outside it fails with a clear message. On Linux, Java decodes them using the locale. Application arguments aren't affected.
- **Errors:** startup failures show a dialog and exit with code 2. On Linux, that means stderr, plus `zenity`, `kdialog` or `xmessage` if available when not started from a terminal. A Java exception escaping `main` exits with code 1.
- **Console:** on Windows there's no console window. When started from a terminal, output goes to that terminal, but the terminal doesn't wait for the app to finish.
- **Environment variables:** `BSNATIVE_LAUNCHER_NO_DIALOG=1` writes errors only to stderr, for scripts. `BSNATIVE_LAUNCHER_DRY_RUN=1` prints the selected Java and classpath, then exits.
