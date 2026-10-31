package cz.bliksoft.bsnative.launcher;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.PosixFileAttributeView;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.List;

import org.apache.maven.plugin.AbstractMojo;
import org.apache.maven.plugin.MojoExecutionException;
import org.apache.maven.plugins.annotations.LifecyclePhase;
import org.apache.maven.plugins.annotations.Mojo;
import org.apache.maven.plugins.annotations.Parameter;

/**
 * Writes the launcher for one or more platforms, named after the app. Add the ini
 * file yourself; see the project's docs/ini.md.
 */
@Mojo(name = "copy", defaultPhase = LifecyclePhase.PREPARE_PACKAGE, threadSafe = true)
public class CopyLauncherMojo extends AbstractMojo {
	private static final String PLATFORM_PLACEHOLDER = "{platform}";

	/** The app's name. The launcher becomes this, with .exe added on Windows. */
	@Parameter(required = true)
	private String name;

	/**
	 * The target platform, in javacpp.platform or javafx.platform names. Default: the
	 * machine running the build.
	 */
	@Parameter
	private String platform;

	/** Several target platforms instead of one; use {platform} in outputDirectory. */
	@Parameter
	private List<String> platforms;

	/** Where to write. {platform} stands for each platform as written in the configuration. */
	@Parameter(defaultValue = "${project.build.directory}/launcher")
	private String outputDirectory;

	@Parameter(property = "bsnative.launcher.skip", defaultValue = "false")
	private boolean skip;

	@Override
	public void execute() throws MojoExecutionException {
		if (skip)
			return;
		try {
			if (platforms != null && !platforms.isEmpty()) {
				if (!outputDirectory.contains(PLATFORM_PLACEHOLDER))
					throw new MojoExecutionException("With <platforms>, outputDirectory must contain " + PLATFORM_PLACEHOLDER);
				for (String each : platforms)
					write(each);
			} else if (platform != null && !platform.isBlank()) {
				write(platform);
			} else {
				write(Platforms.current(System.getProperty("os.name"), System.getProperty("os.arch")));
			}
		} catch (IOException | IllegalArgumentException e) {
			throw new MojoExecutionException(e.getMessage(), e);
		}
	}

	private void write(String configured) throws IOException, MojoExecutionException {
		Path directory = Paths.get(outputDirectory.replace(PLATFORM_PLACEHOLDER, configured));
		Path file = copy(Platforms.normalize(configured), name, directory);
		getLog().info("Launcher for " + configured + ": " + file);
	}

	/** Writes one platform's launcher and returns its path. */
	static Path copy(String platform, String appName, Path directory) throws IOException, MojoExecutionException {
		String resource = Platforms.binaryResource(platform);
		Files.createDirectories(directory);
		Path file = directory.resolve(Platforms.isWindows(platform) ? appName + ".exe" : appName);
		try (InputStream binary = CopyLauncherMojo.class.getResourceAsStream(resource)) {
			if (binary == null)
				throw new MojoExecutionException("This plugin build has no launcher for " + platform);
			Files.copy(binary, file, StandardCopyOption.REPLACE_EXISTING);
		}
		if (Files.getFileAttributeView(file, PosixFileAttributeView.class) != null)
			Files.setPosixFilePermissions(file, PosixFilePermissions.fromString("rwxr-xr-x"));
		return file;
	}
}
