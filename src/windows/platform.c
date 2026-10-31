#include "platform_win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char PLAT_PATH_SEPARATOR = ';';

#if defined(_M_ARM64) || defined(__aarch64__)
const char *const PLAT_ARCH_NAME = "aarch64";
#else
const char *const PLAT_ARCH_NAME = "x86_64";
#endif

int plat_arch_matches(const char *os_arch) {
#if defined(_M_ARM64) || defined(__aarch64__)
	return strcmp(os_arch, "aarch64") == 0;
#else
	return strcmp(os_arch, "x86_64") == 0 || strcmp(os_arch, "amd64") == 0;
#endif
}

wchar_t *to_wide(const char *text) {
	int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	wchar_t *result = malloc((size_t)length * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, text, -1, result, length);
	return result;
}

char *to_utf8(const wchar_t *text) {
	int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	char *result = malloc((size_t)length);
	WideCharToMultiByte(CP_UTF8, 0, text, -1, result, length, NULL, NULL);
	return result;
}

char *plat_read_file(const char *path) {
	FILE *file = _wfopen(to_wide(path), L"rb");
	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char *text = malloc((size_t)size + 1);
	size_t read = fread(text, 1, (size_t)size, file);
	text[read] = '\0';
	fclose(file);
	return text;
}

int plat_is_dir(const char *path) {
	DWORD attributes = GetFileAttributesW(to_wide(path));
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

int plat_is_absolute(const char *path) {
	if (path[0] == '\\' || path[0] == '/')
		return 1;
	return path[0] && path[1] == ':';
}

char *plat_join(const char *dir, const char *name) {
	char *result = str_fmt("%s\\%s", dir, name);
	for (char *p = result; *p; p++) {
		if (*p == '/')
			*p = '\\';
	}
	return result;
}

char *plat_getenv(const char *name) {
	wchar_t *wide_name = to_wide(name);
	DWORD size = GetEnvironmentVariableW(wide_name, NULL, 0);
	if (size == 0)
		return NULL;
	wchar_t *value = malloc(size * sizeof(wchar_t));
	GetEnvironmentVariableW(wide_name, value, size);
	return to_utf8(value);
}

int plat_list_jars(const char *dir, strlist *jars) {
	WIN32_FIND_DATAW data;
	HANDLE find = FindFirstFileW(to_wide(plat_join(dir, "*.jar")), &data);
	if (find == INVALID_HANDLE_VALUE)
		return 0;
	do {
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			strlist_add(jars, plat_join(dir, to_utf8(data.cFileName)));
	} while (FindNextFileW(find, &data));
	FindClose(find);
	return 1;
}

/**
 * Adds the value from each version's subkey. Adoptium keeps it under
 * <version>\hotspot\MSI as Path, JavaSoft directly under <version> as JavaHome.
 */
static void add_registry_homes(const wchar_t *root, const wchar_t *subpath, const wchar_t *value, strlist *homes) {
	HKEY key;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, root, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
		return;
	wchar_t version[256];
	for (DWORD index = 0;; index++) {
		DWORD length = 256;
		if (RegEnumKeyExW(key, index, version, &length, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
			break;
		wchar_t path[512];
		_snwprintf(path, 512, L"%ls\\%ls%ls", root, version, subpath);
		path[511] = L'\0';
		wchar_t home[1024];
		DWORD size = sizeof(home);
		if (RegGetValueW(HKEY_LOCAL_MACHINE, path, value, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, NULL, home, &size) == ERROR_SUCCESS) {
			char *text = to_utf8(home);
			size_t text_length = strlen(text);
			if (text_length && text[text_length - 1] == '\\')
				text[text_length - 1] = '\0';
			int known = 0;
			for (size_t i = 0; i < homes->count; i++)
				known = known || _stricmp(homes->items[i], text) == 0;
			if (!known)
				strlist_add(homes, text);
		}
	}
	RegCloseKey(key);
}

void plat_installed_homes(strlist *homes) {
	add_registry_homes(L"SOFTWARE\\Eclipse Adoptium\\JDK", L"\\hotspot\\MSI", L"Path", homes);
	add_registry_homes(L"SOFTWARE\\JavaSoft\\JDK", L"", L"JavaHome", homes);
}

void plat_path_homes(strlist *homes) {
	char *path = plat_getenv("PATH");
	if (!path)
		return;
	for (char *dir = strtok(path, ";"); dir; dir = strtok(NULL, ";")) {
		char *exe = plat_join(dir, "java.exe");
		DWORD attributes = GetFileAttributesW(to_wide(exe));
		if (attributes == INVALID_FILE_ATTRIBUTES)
			continue;
		char *home = str_dup(dir);
		size_t length = strlen(home);
		while (length && (home[length - 1] == '\\' || home[length - 1] == '/'))
			home[--length] = '\0';
		char *slash = strrchr(home, '\\');
		if (slash)
			*slash = '\0';
		strlist_add(homes, home);
	}
}
