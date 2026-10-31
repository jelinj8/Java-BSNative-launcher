package cz.bliksoft.bsnative.launcher;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class CopyLauncherMojoTest {
	private static final byte[] PE = { 'M', 'Z' };
	private static final byte[] ELF = { 0x7f, 'E', 'L', 'F' };

	@Test
	void everyPlatformHasABundledBinary() {
		for (String platform : Platforms.ALL)
			assertNotNull(CopyLauncherMojo.class.getResource(Platforms.binaryResource(platform)), platform);
	}

	@Test
	void writesWithTheRightNameAndFormat(@TempDir Path dir) throws Exception {
		Path windows = CopyLauncherMojo.copy("windows-x86_64", "MyApp", dir.resolve("w"));
		assertEquals("MyApp.exe", windows.getFileName().toString());
		assertStartsWith(windows, PE);

		Path linux = CopyLauncherMojo.copy("linux-arm64", "MyApp", dir.resolve("l"));
		assertEquals("MyApp", linux.getFileName().toString());
		assertStartsWith(linux, ELF);

		Path mac = CopyLauncherMojo.copy("macosx-arm64", "MyApp", dir.resolve("m"));
		assertEquals("MyApp", mac.getFileName().toString());
		assertFalse(Files.readAllBytes(mac).length == 0);
	}

	@Test
	void overwritesAnEarlierCopy(@TempDir Path dir) throws Exception {
		Files.writeString(dir.resolve("MyApp"), "old");
		assertStartsWith(CopyLauncherMojo.copy("linux-x86_64", "MyApp", dir), ELF);
	}

	@Test
	void marksTheFileExecutableWherePermissionsExist(@TempDir Path dir) throws Exception {
		assertTrue(Files.isExecutable(CopyLauncherMojo.copy("linux-x86_64", "MyApp", dir)));
	}

	@Test
	void acceptsJavaCppAndJavaFxNames() {
		Map.of("win", "windows-x86_64", "linux", "linux-x86_64", "linux-aarch64", "linux-arm64", "mac",
				"macosx-x86_64", "mac-aarch64", "macosx-arm64", "windows-arm64", "windows-arm64")
				.forEach((given, expected) -> assertEquals(expected, Platforms.normalize(given)));
		assertThrows(IllegalArgumentException.class, () -> Platforms.normalize("solaris"));
	}

	@Test
	void detectsTheBuildMachine() {
		assertEquals("windows-x86_64", Platforms.current("Windows 11", "amd64"));
		assertEquals("windows-arm64", Platforms.current("Windows 11", "aarch64"));
		assertEquals("linux-x86_64", Platforms.current("Linux", "amd64"));
		assertEquals("linux-arm64", Platforms.current("Linux", "aarch64"));
		assertEquals("macosx-arm64", Platforms.current("Mac OS X", "aarch64"));
		assertEquals("macosx-x86_64", Platforms.current("Mac OS X", "x86_64"));
		assertThrows(IllegalArgumentException.class, () -> Platforms.current("Linux", "riscv64"));
		assertThrows(IllegalArgumentException.class, () -> Platforms.current("FreeBSD", "amd64"));
	}

	private static void assertStartsWith(Path file, byte[] start) throws Exception {
		byte[] bytes = Files.readAllBytes(file);
		for (int i = 0; i < start.length; i++)
			assertEquals(start[i], bytes[i], file.toString());
	}
}
