// Portable launcher logic. All strings are UTF-8; platform code converts at its boundaries.
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <stddef.h>

typedef struct {
	char **items;
	size_t count;
} strlist;

void strlist_add(strlist *list, const char *value);
char *str_fmt(const char *format, ...);
char *str_dup(const char *value);

typedef struct {
	char *main_class;
	strlist classpath;
	int min_java;
	int max_java;
	char *runtime;
	strlist jvm_options;
	strlist args;
	char *app_id;
	char *log_file;
	char *working_dir;
} launcher_config;

/** Parses ini text. Returns 0, or -1 with a message in *error. */
int config_parse(const char *text, const char *app_dir, launcher_config *config, char **error);

/** Compares versions such as 25.0.4.1 or 1.8.0_292: negative, zero or positive. */
int version_compare(const char *a, const char *b);
/** The Java feature version: 25 for 25.0.4.1, 8 for 1.8.0_292. */
int version_feature(const char *version);

/** Reads a value from the text of a Java home's release file. NULL if absent. */
char *release_value(const char *release_text, const char *key);

typedef struct {
	char *home;
	char *version;
} java_runtime;

/**
 * Checks one Java home against the config. Returns 1 if usable, filling *runtime
 * if not NULL; otherwise 0, with the reason in *reason.
 */
int runtime_check(const char *home, const launcher_config *config, java_runtime *runtime, char **reason);

/**
 * Finds the runtime to use. Returns 1 if found. Every candidate considered is
 * added to *report, for showing when none is usable.
 */
int runtime_select(const launcher_config *config, const char *app_dir, java_runtime *runtime, strlist *report);

/** Expands classpath entries into one string. Returns NULL with a message in *error. */
char *classpath_build(const launcher_config *config, const char *app_dir, char **error);

// Provided by each platform.
char *plat_read_file(const char *path);
int plat_is_dir(const char *path);
int plat_is_absolute(const char *path);
char *plat_join(const char *dir, const char *name);
char *plat_getenv(const char *name);
/** Adds the full paths of the .jar files in a directory, in no particular order. */
int plat_list_jars(const char *dir, strlist *jars);
/** Adds Java homes known to the OS, such as those in the Windows registry. */
void plat_installed_homes(strlist *homes);
/** Adds the Java homes of any java executable on PATH. */
void plat_path_homes(strlist *homes);
extern const char PLAT_PATH_SEPARATOR;
/** The OS_ARCH this build's JVM must have. */
int plat_arch_matches(const char *os_arch);
extern const char *const PLAT_ARCH_NAME;

#endif
