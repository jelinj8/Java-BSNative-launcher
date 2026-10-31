package cz.bliksoft.bsnative.launcher;

import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Platform names: the launcher's own, which are those of javacpp.platform, and javafx.platform's. */
final class Platforms {
	static final List<String> ALL = List.of("windows-x86_64", "windows-arm64", "linux-x86_64", "linux-arm64",
			"macosx-x86_64", "macosx-arm64");

	private static final Map<String, String> JAVAFX_NAMES = Map.of("win", "windows-x86_64", "linux", "linux-x86_64",
			"linux-aarch64", "linux-arm64", "mac", "macosx-x86_64", "mac-aarch64", "macosx-arm64");

	private Platforms() {
	}

	/** @return the launcher's name for a javacpp.platform or javafx.platform value */
	static String normalize(String platform) {
		if (ALL.contains(platform))
			return platform;
		String name = JAVAFX_NAMES.get(platform);
		if (name == null)
			throw new IllegalArgumentException("Unknown platform '" + platform + "'. Use one of " + ALL + " or "
					+ JAVAFX_NAMES.keySet());
		return name;
	}

	static boolean isWindows(String platform) {
		return platform.startsWith("windows-");
	}

	/** @return the platform of the machine running the build */
	static String current(String osName, String osArch) {
		String os = osName.toLowerCase(Locale.ROOT);
		boolean arm = osArch.equals("aarch64") || osArch.equals("arm64");
		String arch = arm ? "arm64" : "x86_64";
		if (!arm && !osArch.equals("amd64") && !osArch.equals("x86_64"))
			throw new IllegalArgumentException("No launcher for architecture '" + osArch + "'. Set <platform>.");
		if (os.contains("windows"))
			return "windows-" + arch;
		if (os.contains("mac"))
			return "macosx-" + arch;
		if (os.contains("linux"))
			return "linux-" + arch;
		throw new IllegalArgumentException("No launcher for OS '" + osName + "'. Set <platform>.");
	}

	/** @return the file name of a platform's binary among the plugin's resources */
	static String binaryResource(String platform) {
		return "binaries/bsnative-launcher-" + platform + (isWindows(platform) ? ".exe" : "");
	}
}
