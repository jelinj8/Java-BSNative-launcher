// Tests of the portable logic, using real temporary folders as fake Java homes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/windows/platform_win.h"

static int failures;

#define CHECK(condition)                                               \
	do {                                                               \
		if (!(condition)) {                                            \
			printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			failures++;                                                \
		}                                                              \
	} while (0)
#define CHECK_STR(actual, expected)                                                  \
	do {                                                                             \
		const char *a = (actual);                                                    \
		if (!a || strcmp(a, (expected)) != 0) {                                      \
			printf("FAILED %s:%d: got [%s], expected [%s]\n", __FILE__, __LINE__, a ? a : "NULL", (expected)); \
			failures++;                                                              \
		}                                                                            \
	} while (0)

static char *root;

static void write_file(const char *path, const char *text) {
	FILE *file = _wfopen(to_wide(path), L"wb");
	fputs(text, file);
	fclose(file);
}

static char *fake_home(const char *name, const char *version, const char *arch) {
	char *home = plat_join(root, name);
	CreateDirectoryW(to_wide(home), NULL);
	write_file(plat_join(home, "release"), str_fmt("IMPLEMENTOR=\"x\"\r\nJAVA_VERSION=\"%s\"\r\nOS_ARCH=\"%s\"\r\n", version, arch));
	return home;
}

static void test_ini(void) {
	launcher_config c;
	char *error = NULL;
	_wputenv(L"BSN_TEST_VAR=fromenv");
	CHECK(config_parse("\xEF\xBB\xBF# comment\r\n; comment\r\n\r\n main-class = com.example.Main \r\nclasspath=lib/*\nclasspath=${APP_DIR}/x.jar\n"
			"min-java=21\nmax-java=25\njvm-option=-Xmx1m\njvm-option=-Da=b=c\narg=${ENV:BSN_TEST_VAR}\narg=#notacomment\n"
			"runtime=auto\napp-id=my.app\nlog-file=logs/a.log\n", "C:\\app", &c, &error) == 0);
	CHECK_STR(c.main_class, "com.example.Main");
	CHECK(c.classpath.count == 2);
	CHECK_STR(c.classpath.items[1], "C:\\app/x.jar");
	CHECK(c.min_java == 21 && c.max_java == 25);
	CHECK_STR(c.jvm_options.items[1], "-Da=b=c");
	CHECK_STR(c.args.items[0], "fromenv");
	CHECK_STR(c.args.items[1], "#notacomment");
	CHECK(c.runtime == NULL);
	CHECK_STR(c.app_id, "my.app");

	CHECK(config_parse("main-class=A\narg=${ENV:BSN_NOT_SET_X}y\narg=${OTHER}\n", "C:\\app", &c, &error) == 0);
	CHECK_STR(c.args.items[0], "y");
	CHECK_STR(c.args.items[1], "${OTHER}");

	CHECK(config_parse("classpath=a\n", "d", &c, &error) == -1);
	CHECK_STR(error, "main-class is required");
	CHECK(config_parse("main-class=A\nmain-klass=B\n", "d", &c, &error) == -1);
	CHECK_STR(error, "Line 2: unknown key: main-klass");
	CHECK(config_parse("nonsense\n", "d", &c, &error) == -1);
	CHECK(config_parse("main-class=A\nmin-java=abc\n", "d", &c, &error) == -1);
	CHECK(config_parse("main-class=A\nmin-java=0\n", "d", &c, &error) == -1);
}

static void test_versions(void) {
	CHECK(version_feature("25.0.4.1") == 25);
	CHECK(version_feature("1.8.0_292") == 8);
	CHECK(version_feature("21") == 21);
	CHECK(version_feature("26-ea") == 26);
	CHECK(version_compare("25.0.4.1", "25.0.4") > 0);
	CHECK(version_compare("21.0.10", "21.0.9") > 0);
	CHECK(version_compare("9", "21") < 0);
	CHECK(version_compare("21.0.5", "21.0.5") == 0);
	CHECK(version_compare("1.8.0_292", "1.8.0_301") < 0);
}

static void test_release(void) {
	const char *text = "IMPLEMENTOR=\"a\"\nJAVA_VERSION=\"21.0.5\"\r\nNOT_JAVA_VERSION=\"x\"\nRAW=plain";
	CHECK_STR(release_value(text, "JAVA_VERSION"), "21.0.5");
	CHECK_STR(release_value(text, "RAW"), "plain");
	CHECK(release_value(text, "MISSING") == NULL);
	CHECK(release_value(text, "VERSION") == NULL);
}

static void test_runtime_check(void) {
	launcher_config c = {0};
	char *reason = NULL;
	java_runtime r;
	char *home = fake_home("j21", "21.0.5", "x86_64");
	CHECK(runtime_check(home, &c, &r, &reason) == 1);
	CHECK_STR(r.version, "21.0.5");
	c.min_java = 21;
	c.max_java = 21;
	CHECK(runtime_check(home, &c, &r, &reason) == 1);
	c.min_java = 22;
	c.max_java = 0;
	CHECK(runtime_check(home, &c, &r, &reason) == 0);
	CHECK(strstr(reason, "older than min-java 22") != NULL);
	c.min_java = 0;
	c.max_java = 17;
	CHECK(runtime_check(home, &c, &r, &reason) == 0);
	CHECK(strstr(reason, "newer than max-java 17") != NULL);
	c.max_java = 0;
	CHECK(runtime_check(fake_home("arm", "21", "aarch64"), &c, &r, &reason) == 0);
	CHECK(strstr(reason, "aarch64") != NULL);
	CHECK(runtime_check(plat_join(root, "absent"), &c, &r, &reason) == 0);
	char *empty = plat_join(root, "empty");
	CreateDirectoryW(to_wide(empty), NULL);
	CHECK(runtime_check(empty, &c, &r, &reason) == 0);
	CHECK(strstr(reason, "no release file") != NULL);
}

static void test_runtime_select(void) {
	char *app = plat_join(root, "app");
	CreateDirectoryW(to_wide(app), NULL);
	launcher_config c = {0};
	java_runtime r;
	strlist report = {0};

	c.runtime = fake_home("explicit", "11.0.1", "x86_64");
	CHECK(runtime_select(&c, app, &r, &report) == 1);
	CHECK_STR(r.version, "11.0.1");
	c.runtime = plat_join(root, "absent");
	CHECK(runtime_select(&c, app, &r, &report) == 0);
	c.runtime = NULL;

	char *java_home = fake_home("envhome", "17.0.2", "x86_64");
	_wputenv_s(L"JAVA_HOME", to_wide(java_home));
	CHECK(runtime_select(&c, app, &r, &report) == 1);
	CHECK_STR(r.home, java_home);

	char *bundled = plat_join(app, "runtime");
	CreateDirectoryW(to_wide(bundled), NULL);
	write_file(plat_join(bundled, "release"), "JAVA_VERSION=\"11.0.9\"\n");
	CHECK(runtime_select(&c, app, &r, &report) == 1);
	CHECK_STR(r.home, bundled);

	c.min_java = 17; // the bundled one is too old, so JAVA_HOME is next
	strlist tried = {0};
	CHECK(runtime_select(&c, app, &r, &tried) == 1);
	CHECK_STR(r.home, java_home);
	CHECK(tried.count == 2);
	CHECK(strstr(tried.items[0], "bundled") && strstr(tried.items[0], "older"));
}

static void test_classpath(void) {
	char *app = plat_join(root, "cpapp");
	char *lib = plat_join(app, "lib");
	CreateDirectoryW(to_wide(app), NULL);
	CreateDirectoryW(to_wide(lib), NULL);
	write_file(plat_join(lib, "b.jar"), "");
	write_file(plat_join(lib, "a.jar"), "");
	write_file(plat_join(lib, "notes.txt"), "");
	launcher_config c = {0};
	char *error = NULL;
	CHECK(classpath_build(&c, app, &error) == NULL);
	CHECK_STR(error, "classpath is required");

	strlist_add(&c.classpath, "lib/*;classes");
	strlist_add(&c.classpath, "C:\\abs\\x.jar");
	char *expected = str_fmt("%s\\a.jar;%s\\b.jar;%s\\classes;C:\\abs\\x.jar", lib, lib, app);
	CHECK_STR(classpath_build(&c, app, &error), expected);

	launcher_config missing = {0};
	strlist_add(&missing.classpath, "nothing/*");
	CHECK(classpath_build(&missing, app, &error) == NULL);
	CHECK(strstr(error, "no .jar files") != NULL);
}

int main(void) {
	wchar_t temp[MAX_PATH];
	GetTempPathW(MAX_PATH, temp);
	root = str_fmt("%sbsnative-test-%lu", to_utf8(temp), GetCurrentProcessId());
	CreateDirectoryW(to_wide(root), NULL);

	test_ini();
	test_versions();
	test_release();
	test_runtime_check();
	test_runtime_select();
	test_classpath();

	printf(failures ? "%d FAILED\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
