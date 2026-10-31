#include "launcher.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *check_alloc(void *memory) {
	if (!memory) {
		fputs("Out of memory\n", stderr);
		exit(70);
	}
	return memory;
}

char *str_dup(const char *value) {
	size_t size = strlen(value) + 1;
	return memcpy(check_alloc(malloc(size)), value, size);
}

char *str_fmt(const char *format, ...) {
	va_list args;
	va_start(args, format);
	int length = vsnprintf(NULL, 0, format, args);
	va_end(args);
	char *result = check_alloc(malloc((size_t)length + 1));
	va_start(args, format);
	vsnprintf(result, (size_t)length + 1, format, args);
	va_end(args);
	return result;
}

void strlist_add(strlist *list, const char *value) {
	list->items = check_alloc(realloc(list->items, (list->count + 1) * sizeof(char *)));
	list->items[list->count++] = str_dup(value);
}

// ---- ini ----

static char *trim(char *text) {
	while (*text && isspace((unsigned char)*text))
		text++;
	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return text;
}

/** Replaces ${APP_DIR} and ${ENV:NAME}. A variable that isn't set becomes empty. */
static char *expand(const char *value, const char *app_dir) {
	char *result = str_dup("");
	const char *rest = value;
	for (;;) {
		const char *start = strstr(rest, "${");
		const char *end = start ? strchr(start, '}') : NULL;
		if (!end)
			return str_fmt("%s%s", result, rest);
		char *name = str_fmt("%.*s", (int)(end - start - 2), start + 2);
		const char *replacement = "";
		if (strcmp(name, "APP_DIR") == 0) {
			replacement = app_dir;
		} else if (strncmp(name, "ENV:", 4) == 0) {
			char *variable = plat_getenv(name + 4);
			replacement = variable ? variable : "";
		} else {
			replacement = str_fmt("${%s}", name);
		}
		result = str_fmt("%s%.*s%s", result, (int)(start - rest), rest, replacement);
		rest = end + 1;
	}
}

static int parse_int(const char *value, int *out) {
	char *end;
	long number = strtol(value, &end, 10);
	if (end == value || *end || number <= 0)
		return 0;
	*out = (int)number;
	return 1;
}

int config_parse(const char *text, const char *app_dir, launcher_config *config, char **error) {
	memset(config, 0, sizeof(*config));
	if (strncmp(text, "\xEF\xBB\xBF", 3) == 0)
		text += 3;
	char *copy = str_dup(text);
	int line_number = 0;
	for (char *line = copy; line;) {
		char *next = strchr(line, '\n');
		if (next)
			*next++ = '\0';
		line_number++;
		char *content = trim(line);
		line = next;
		if (!*content || *content == '#' || *content == ';')
			continue;
		char *equals = strchr(content, '=');
		if (!equals) {
			*error = str_fmt("Line %d: expected key=value, found: %s", line_number, content);
			return -1;
		}
		*equals = '\0';
		const char *key = trim(content);
		char *value = expand(trim(equals + 1), app_dir);

		if (strcmp(key, "main-class") == 0) {
			config->main_class = value;
		} else if (strcmp(key, "classpath") == 0) {
			strlist_add(&config->classpath, value);
		} else if (strcmp(key, "runtime") == 0) {
			config->runtime = strcmp(value, "auto") == 0 ? NULL : value;
		} else if (strcmp(key, "jvm-option") == 0) {
			strlist_add(&config->jvm_options, value);
		} else if (strcmp(key, "arg") == 0) {
			strlist_add(&config->args, value);
		} else if (strcmp(key, "app-id") == 0) {
			config->app_id = value;
		} else if (strcmp(key, "log-file") == 0) {
			config->log_file = value;
		} else if (strcmp(key, "working-dir") == 0) {
			config->working_dir = value;
		} else if (strcmp(key, "min-java") == 0 || strcmp(key, "max-java") == 0) {
			int *target = key[1] == 'i' ? &config->min_java : &config->max_java;
			if (!parse_int(value, target)) {
				*error = str_fmt("Line %d: %s must be a Java version number such as 21, not: %s", line_number, key, value);
				return -1;
			}
		} else {
			*error = str_fmt("Line %d: unknown key: %s", line_number, key);
			return -1;
		}
	}
	if (!config->main_class || !*config->main_class) {
		*error = str_dup("main-class is required");
		return -1;
	}
	return 0;
}

// ---- versions ----

static int version_parts(const char *version, long parts[6]) {
	int count = 0;
	memset(parts, 0, 6 * sizeof(long));
	for (const char *p = version; *p && count < 6;) {
		if (isdigit((unsigned char)*p)) {
			char *end;
			parts[count++] = strtol(p, &end, 10);
			p = end;
		} else if (*p == '.' || *p == '_') {
			p++;
		} else {
			break; // a suffix such as -ea
		}
	}
	return count;
}

int version_compare(const char *a, const char *b) {
	long left[6], right[6];
	version_parts(a, left);
	version_parts(b, right);
	for (int i = 0; i < 6; i++) {
		if (left[i] != right[i])
			return left[i] < right[i] ? -1 : 1;
	}
	return 0;
}

int version_feature(const char *version) {
	long parts[6];
	version_parts(version, parts);
	return (int)(parts[0] == 1 ? parts[1] : parts[0]);
}

// ---- runtime selection ----

char *release_value(const char *release_text, const char *key) {
	size_t key_length = strlen(key);
	for (const char *line = release_text; line && *line;) {
		if (strncmp(line, key, key_length) == 0 && line[key_length] == '=') {
			const char *value = line + key_length + 1;
			size_t length = strcspn(value, "\r\n");
			if (length >= 2 && value[0] == '"' && value[length - 1] == '"') {
				value++;
				length -= 2;
			}
			return str_fmt("%.*s", (int)length, value);
		}
		line = strchr(line, '\n');
		if (line)
			line++;
	}
	return NULL;
}

int runtime_check(const char *home, const launcher_config *config, java_runtime *runtime, char **reason) {
	if (!plat_is_dir(home)) {
		*reason = str_dup("folder not found");
		return 0;
	}
	char *text = plat_read_file(plat_join(home, "release"));
	if (!text) {
		*reason = str_dup("no release file, so not a Java home");
		return 0;
	}
	char *version = release_value(text, "JAVA_VERSION");
	if (!version) {
		*reason = str_dup("release file has no JAVA_VERSION");
		return 0;
	}
	int feature = version_feature(version);
	if (config->min_java && feature < config->min_java) {
		*reason = str_fmt("Java %s is older than min-java %d", version, config->min_java);
		return 0;
	}
	if (config->max_java && feature > config->max_java) {
		*reason = str_fmt("Java %s is newer than max-java %d", version, config->max_java);
		return 0;
	}
	char *arch = release_value(text, "OS_ARCH");
	if (arch && !plat_arch_matches(arch)) {
		*reason = str_fmt("Java %s is for %s, this launcher is for %s", version, arch, PLAT_ARCH_NAME);
		return 0;
	}
	if (runtime) {
		runtime->home = str_dup(home);
		runtime->version = version;
	}
	return 1;
}

/** Picks the newest usable home of one source. */
static int select_from(const char *source, const strlist *homes, const launcher_config *config, java_runtime *best, strlist *report) {
	int found = 0;
	for (size_t i = 0; i < homes->count; i++) {
		java_runtime candidate;
		char *reason;
		if (!runtime_check(homes->items[i], config, &candidate, &reason)) {
			strlist_add(report, str_fmt("%s: %s: %s", source, homes->items[i], reason));
			continue;
		}
		strlist_add(report, str_fmt("%s: %s: Java %s", source, homes->items[i], candidate.version));
		if (!found || version_compare(candidate.version, best->version) > 0)
			*best = candidate;
		found = 1;
	}
	return found;
}

int runtime_select(const launcher_config *config, const char *app_dir, java_runtime *runtime, strlist *report) {
	if (config->runtime) {
		strlist homes = {0};
		strlist_add(&homes, plat_is_absolute(config->runtime) ? config->runtime : plat_join(app_dir, config->runtime));
		return select_from("runtime setting", &homes, config, runtime, report);
	}
	char *bundled = plat_join(app_dir, "runtime");
	if (plat_is_dir(bundled)) {
		strlist homes = {0};
		strlist_add(&homes, bundled);
		if (select_from("bundled", &homes, config, runtime, report))
			return 1;
	}
	char *java_home = plat_getenv("JAVA_HOME");
	if (java_home && *java_home) {
		strlist homes = {0};
		strlist_add(&homes, java_home);
		if (select_from("JAVA_HOME", &homes, config, runtime, report))
			return 1;
	}
	strlist installed = {0};
	plat_installed_homes(&installed);
	if (select_from("installed", &installed, config, runtime, report))
		return 1;
	strlist on_path = {0};
	plat_path_homes(&on_path);
	return select_from("PATH", &on_path, config, runtime, report);
}

// ---- classpath ----

static int compare_strings(const void *a, const void *b) {
	return strcmp(*(char *const *)a, *(char *const *)b);
}

static int is_wildcard(const char *entry) {
	size_t length = strlen(entry);
	return length >= 1 && entry[length - 1] == '*' && (length == 1 || entry[length - 2] == '/' || entry[length - 2] == '\\');
}

char *classpath_build(const launcher_config *config, const char *app_dir, char **error) {
	strlist entries = {0};
	char separators[3] = {';', PLAT_PATH_SEPARATOR, '\0'};
	for (size_t i = 0; i < config->classpath.count; i++) {
		char *copy = str_dup(config->classpath.items[i]);
		// a Windows drive's colon isn't a separator, and only ; separates there
		for (char *entry = strtok(copy, PLAT_PATH_SEPARATOR == ';' ? ";" : separators); entry; entry = strtok(NULL, PLAT_PATH_SEPARATOR == ';' ? ";" : separators)) {
			char *path = plat_is_absolute(entry) ? str_dup(entry) : plat_join(app_dir, entry);
			if (!is_wildcard(entry)) {
				strlist_add(&entries, path);
				continue;
			}
			path[strlen(path) - 2] = '\0'; // the separator and the *
			strlist jars = {0};
			plat_list_jars(path, &jars);
			if (jars.count == 0) {
				*error = str_fmt("classpath: no .jar files in %s", path);
				return NULL;
			}
			qsort(jars.items, jars.count, sizeof(char *), compare_strings);
			for (size_t j = 0; j < jars.count; j++)
				strlist_add(&entries, jars.items[j]);
		}
	}
	if (entries.count == 0) {
		*error = str_dup("classpath is required");
		return NULL;
	}
	char *result = str_dup(entries.items[0]);
	for (size_t i = 1; i < entries.count; i++)
		result = str_fmt("%s%c%s", result, PLAT_PATH_SEPARATOR, entries.items[i]);
	return result;
}
