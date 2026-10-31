#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdlib.h>
#endif

#include "../common/launcher.h"

static int find_exe(char *exe) {
#ifdef __APPLE__
	char path[PATH_MAX + 1];
	uint32_t size = sizeof(path);
	return _NSGetExecutablePath(path, &size) == 0 && realpath(path, exe) != NULL;
#else
	ssize_t length = readlink("/proc/self/exe", exe, PATH_MAX);
	if (length <= 0)
		return 0;
	exe[length] = '\0';
	return 1;
#endif
}

int main(int argc, char **argv) {
	char exe[PATH_MAX + 1];
	if (!find_exe(exe)) {
		fputs("Can't find this program's own path\n", stderr);
		return 2;
	}
	char *app_dir = str_dup(exe);
	*strrchr(app_dir, '/') = '\0';

	strlist args = {0};
	for (int i = 1; i < argc; i++)
		strlist_add(&args, argv[i]);
	return launcher_run(exe, app_dir, &args);
}
