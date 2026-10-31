# Using the launcher in a Maven build

The launcher is published to Maven Central as a plain file, so an app's build only has to copy and rename it. No native tools are needed.

| Platform | Coordinates |
|---|---|
| Windows x64 | `cz.bliksoft.java:bsnative-launcher:<version>:exe:win-x64` |

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
            <version>0.1.0</version>
            <type>exe</type>
            <classifier>win-x64</classifier>
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

The exe is never patched, so one version of the launcher can serve any number of apps.

JavaFX and other platform-specific jars must match the target platform.
