#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../common/launcher.h"

int main(int argc, char **argv) {
	char exe[PATH_MAX + 1];
	ssize_t length = readlink("/proc/self/exe", exe, PATH_MAX);
	if (length <= 0) {
		fputs("Can't find this program's path in /proc/self/exe\n", stderr);
		return 2;
	}
	exe[length] = '\0';
	char *app_dir = str_dup(exe);
	*strrchr(app_dir, '/') = '\0';

	strlist args = {0};
	for (int i = 1; i < argc; i++)
		strlist_add(&args, argv[i]);
	return launcher_run(exe, app_dir, &args);
}
