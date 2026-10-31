# Using the launcher in a Maven build

The Maven plugin has every launcher binary bundled, so an app's build needs no native tools and no other download. It works from any build machine for any target platform.

```xml
<plugin>
  <groupId>cz.bliksoft.java</groupId>
  <artifactId>bsnative-launcher-maven-plugin</artifactId>
  <version>0.3.0</version>
  <executions>
    <execution>
      <goals><goal>copy</goal></goals>
      <configuration>
        <name>MyApp</name>
        <platform>${javacpp.platform}</platform>
      </configuration>
    </execution>
  </executions>
</plugin>
```

This writes `target/launcher/MyApp`, or `MyApp.exe` for Windows, in the `prepare-package` phase. Add it to your package with your own ini file (see [ini.md](ini.md)), for example with `maven-assembly-plugin`:

```xml
<fileSet>
  <directory>${project.build.directory}/launcher</directory>
  <outputDirectory>/</outputDirectory>
  <fileMode>0755</fileMode>
</fileSet>
```

## Settings

| Setting | Meaning |
|---|---|
| `name` | Required. The launcher's name, with `.exe` added on Windows |
| `platform` | The target. Default: the machine running the build |
| `platforms` | Several targets instead of one |
| `outputDirectory` | Default `${project.build.directory}/launcher`. `{platform}` stands for each platform as you wrote it, and is required with `platforms` |
| `skip` | Or `-Dbsnative.launcher.skip=true` |

Platforms can use `javacpp.platform` or `javafx.platform` names:

| Platform | Also accepted |
|---|---|
| `windows-x86_64` | `win` |
| `windows-arm64` | |
| `linux-x86_64` | `linux` |
| `linux-arm64` | `linux-aarch64` |
| `macosx-x86_64` | `mac` |
| `macosx-arm64` | `mac-aarch64` |

To build every platform in one run:

```xml
<configuration>
  <name>MyApp</name>
  <platforms>
    <platform>windows-x86_64</platform>
    <platform>linux-x86_64</platform>
    <platform>linux-arm64</platform>
    <platform>macosx-x86_64</platform>
    <platform>macosx-arm64</platform>
  </platforms>
  <outputDirectory>${project.build.directory}/dist/{platform}/launcher</outputDirectory>
</configuration>
```

## Other ways to get it

The binaries are also on [GitHub Releases](https://github.com/jelinj8/Java-BSNative-launcher/releases), with checksums. On Maven Central they exist only inside the plugin.

## Good to know

- JavaFX and other platform-specific jars must match the target platform.
- Maven doesn't keep the executable permission in every step, so set `fileMode` 0755 when packaging, as above.
- The launcher is never patched, so one version can serve any number of apps.
- The Linux launcher needs glibc 2.17 or newer, so it won't run on musl systems such as Alpine.
- On macOS the launcher is unsigned, so Gatekeeper may block a downloaded copy. Tools that identify apps by bundle ID also need an `.app` bundle, which this doesn't create.
