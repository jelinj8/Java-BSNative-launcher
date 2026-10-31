#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common/launcher.h"

const char PLAT_PATH_SEPARATOR = ':';

#if defined(__aarch64__)
const char *const PLAT_ARCH_NAME = "aarch64";
#elif defined(__arm__)
const char *const PLAT_ARCH_NAME = "arm";
#else
const char *const PLAT_ARCH_NAME = "x86_64";
#endif

int plat_arch_matches(const char *os_arch) {
#if defined(__aarch64__)
	return strcmp(os_arch, "aarch64") == 0 || strcmp(os_arch, "arm64") == 0;
#elif defined(__arm__)
	return strcmp(os_arch, "arm") == 0 || strcmp(os_arch, "aarch32") == 0;
#else
	return strcmp(os_arch, "x86_64") == 0 || strcmp(os_arch, "amd64") == 0;
#endif
}

void plat_write_stdout(const char *text) {
	fputs(text, stdout);
	fflush(stdout);
}

void plat_write_stderr(const char *text) {
	fputs(text, stderr);
	fflush(stderr);
}

int plat_has_stderr(void) {
	return fcntl(2, F_GETFD) != -1;
}

/** Shows the message with the first dialog tool available. Returns whether one was found. */
static int show_dialog(const char *message) {
	const char *const commands[][6] = {
		{"zenity", "--error", "--title=Can't start the application", "--no-markup", "--text", NULL},
		{"kdialog", "--title", "Can't start the application", "--error", NULL, NULL},
		{"xmessage", "-center", NULL, NULL, NULL, NULL},
	};
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		pid_t child = fork();
		if (child < 0)
			return 0;
		if (child == 0) {
			const char *arguments[7] = {0};
			int count = 0;
			while (count < 6 && commands[i][count]) {
				arguments[count] = commands[i][count];
				count++;
			}
			arguments[count] = message;
			int null_output = open("/dev/null", O_WRONLY);
			if (null_output >= 0) {
				dup2(null_output, 1);
				dup2(null_output, 2);
			}
			execvp(arguments[0], (char *const *)arguments);
			_exit(127);
		}
		int status = 0;
		waitpid(child, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 127)
			return 1;
	}
	return 0;
}

/** Started from a desktop, stderr goes nowhere the user can see, so a dialog helps too. */
void plat_report_error(const char *message) {
	plat_write_stderr(str_fmt("%s\n", message));
	char *no_dialog = plat_getenv("BSNATIVE_LAUNCHER_NO_DIALOG");
	if ((no_dialog && *no_dialog) || isatty(2))
		return;
	if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY"))
		show_dialog(message);
}

void plat_open_log(const char *path) {
	char *copy = str_dup(path);
	for (char *p = copy + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(copy, 0777);
			*p = '/';
		}
	}
	int log = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (log < 0)
		return; // the app should still start without its log
	dup2(log, 1);
	dup2(log, 2);
	close(log);
}

void plat_set_app_id(const char *app_id) {
	(void)app_id; // only meaningful on Windows
}

int plat_change_dir(const char *path) {
	return chdir(path) == 0;
}

int plat_same_extension(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}

/** Paths are bytes here, and Java decodes them using the locale. */
char *plat_encode_option(const char *text, char **error) {
	(void)error;
	return str_dup(text);
}

void *plat_load_jvm(const char *home, char **error) {
	char *path = plat_join(home, "lib/server/libjvm.so");
	void *jvm = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
	if (!jvm) {
		*error = str_fmt("Can't load %s: %s", path, dlerror());
		return NULL;
	}
	void *create_vm = dlsym(jvm, "JNI_CreateJavaVM");
	if (!create_vm)
		*error = str_fmt("%s has no JNI_CreateJavaVM.", path);
	return create_vm;
}

typedef struct {
	void (*work)(void *);
	void *parameter;
} thread_start;

static void *run_thread(void *parameter) {
	thread_start *start = parameter;
	start->work(start->parameter);
	return NULL;
}

int plat_run_thread(void (*work)(void *), void *parameter, size_t stack_bytes) {
	thread_start start = {work, parameter};
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_attr_setstacksize(&attributes, stack_bytes);
	pthread_t thread;
	if (pthread_create(&thread, &attributes, run_thread, &start) != 0)
		return 0;
	pthread_join(thread, NULL);
	return 1;
}

char *plat_read_file(const char *path) {
	FILE *file = fopen(path, "rb");
	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size < 0) {
		fclose(file);
		return NULL;
	}
	char *text = malloc((size_t)size + 1);
	size_t read = fread(text, 1, (size_t)size, file);
	text[read] = '\0';
	fclose(file);
	return text;
}

int plat_is_dir(const char *path) {
	struct stat info;
	return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

int plat_is_absolute(const char *path) {
	return path[0] == '/';
}

char *plat_join(const char *dir, const char *name) {
	return str_fmt("%s/%s", dir, name);
}

char *plat_getenv(const char *name) {
	const char *value = getenv(name);
	return value ? str_dup(value) : NULL;
}

int plat_list_jars(const char *dir, strlist *jars) {
	DIR *listing = opendir(dir);
	if (!listing)
		return 0;
	for (struct dirent *entry = readdir(listing); entry; entry = readdir(listing)) {
		size_t length = strlen(entry->d_name);
		if (length > 4 && strcmp(entry->d_name + length - 4, ".jar") == 0) {
			char *path = plat_join(dir, entry->d_name);
			struct stat info;
			if (stat(path, &info) == 0 && S_ISREG(info.st_mode))
				strlist_add(jars, path);
		}
	}
	closedir(listing);
	return 1;
}

/** Links such as /usr/lib/jvm/default-java point at homes listed anyway. */
static void add_unique_home(strlist *homes, const char *path) {
	char resolved[PATH_MAX + 1];
	if (!realpath(path, resolved))
		return;
	for (size_t i = 0; i < homes->count; i++) {
		if (strcmp(homes->items[i], resolved) == 0)
			return;
	}
	strlist_add(homes, resolved);
}

void plat_installed_homes(strlist *homes) {
	const char *const roots[] = {"/usr/lib/jvm", "/usr/java", "/opt/java", "/opt"};
	for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
		DIR *listing = opendir(roots[i]);
		if (!listing)
			continue;
		for (struct dirent *entry = readdir(listing); entry; entry = readdir(listing)) {
			if (entry->d_name[0] == '.')
				continue;
			char *path = plat_join(roots[i], entry->d_name);
			// /opt holds all sorts of software, so only offer what looks like a Java home
			struct stat info;
			if (plat_is_dir(path) && stat(plat_join(path, "lib/server/libjvm.so"), &info) == 0)
				add_unique_home(homes, path);
		}
		closedir(listing);
	}
}

void plat_path_homes(strlist *homes) {
	char *path = plat_getenv("PATH");
	if (!path)
		return;
	for (char *dir = strtok(path, ":"); dir; dir = strtok(NULL, ":")) {
		char resolved[PATH_MAX + 1];
		if (!realpath(plat_join(dir, "java"), resolved))
			continue; // distributions link /usr/bin/java through /etc/alternatives
		char *slash = strrchr(resolved, '/');
		if (!slash)
			continue;
		*slash = '\0';
		slash = strrchr(resolved, '/');
		if (!slash)
			continue;
		*slash = '\0';
		add_unique_home(homes, resolved);
	}
}
