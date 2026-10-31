# Using the launcher in a Maven build

The launcher is published to Maven Central as a plain file, so an app's build only has to copy and rename it. No native tools are needed.

Coordinates are `cz.bliksoft.java:bsnative-launcher:<version>:bin:<platform>`, with these platforms, named as in JavaCPP's `javacpp.platform`:

| Platform | Notes |
|---|---|
| `windows-x86_64` | Name the copy `MyApp.exe` |
| `windows-arm64` | Name the copy `MyApp.exe` |
| `linux-x86_64` | |
| `linux-arm64` | Includes 64-bit Raspberry Pi OS |
| `macosx-x86_64` | |
| `macosx-arm64` | |

Binaries and their checksums are also on [GitHub Releases](https://github.com/jelinj8/Java-BSNative-launcher/releases).

## With `maven-dependency-plugin`

This example puts everything in `target/dist/MyApp`. Add an ini file as `src/dist/MyApp.ini` (see [ini.md](ini.md)).

```xml
<plugin>
  <artifactId>maven-dependency-plugin</artifactId>
  <executions>
    <execution>
      <id>copy-launcher</id>
      <phase>package</phase>
      <goals><goal>copy</goal></goals>
      <configuration>
        <artifactItems>
          <artifactItem>
            <groupId>cz.bliksoft.java</groupId>
            <artifactId>bsnative-launcher</artifactId>
            <version>0.2.0</version>
            <type>bin</type>
            <classifier>windows-x86_64</classifier>
            <outputDirectory>${project.build.directory}/dist/MyApp</outputDirectory>
            <destFileName>MyApp.exe</destFileName>
          </artifactItem>
        </artifactItems>
      </configuration>
    </execution>
    <execution>
      <id>copy-jars</id>
      <phase>package</phase>
      <goals><goal>copy-dependencies</goal></goals>
      <configuration>
        <includeScope>runtime</includeScope>
        <outputDirectory>${project.build.directory}/dist/MyApp/lib</outputDirectory>
      </configuration>
    </execution>
  </executions>
</plugin>
```

Also copy the app's own jar to `lib/` and the ini next to the exe, for example with `maven-resources-plugin` or `maven-assembly-plugin`, which can zip the folder too.

The launcher is never patched, so one version of the launcher can serve any number of apps.

JavaFX and other platform-specific jars must match the target platform. On Linux, Maven doesn't keep the executable permission, so set it when packaging: `fileMode` 0755 in `maven-assembly-plugin`, or `chmod +x`. The Linux launcher needs glibc 2.17 or newer, so it won't run on musl systems such as Alpine.

On macOS the launcher is unsigned, so Gatekeeper may block a downloaded copy. Studio-style tools that identify apps by bundle ID also need an `.app` bundle, which this doesn't create.
