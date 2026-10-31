// The launch sequence, shared by all platforms: finds <exe name>.ini next to the exe, and runs Java
// inside this process, so the OS and tools that identify apps by executable see this exe rather
// than java.
#include <jni.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "launcher.h"

#define EXIT_LAUNCHER_ERROR 2
#define MAIN_THREAD_STACK_BYTES (1024 * 1024)

typedef jint(JNICALL *create_vm_function)(JavaVM **, void **, void *);

typedef struct {
	create_vm_function create_vm;
	const launcher_config *config;
	const char *classpath;
	strlist args;
	int exit_code;
} launch_request;

static char *vm_messages = "";

static jint JNICALL capture_vm_output(FILE *stream, const char *format, va_list args) {
	(void)stream;
	char buffer[2048];
	int length = vsnprintf(buffer, sizeof(buffer), format, args);
	vm_messages = str_fmt("%s%s", vm_messages, buffer);
	plat_write_stderr(buffer);
	return length;
}

static char *exception_text(JNIEnv *env, jthrowable exception) {
	jclass type = (*env)->GetObjectClass(env, exception);
	jmethodID to_string = (*env)->GetMethodID(env, type, "toString", "()Ljava/lang/String;");
	jstring text = to_string ? (*env)->CallObjectMethod(env, exception, to_string) : NULL;
	const char *chars = text ? (*env)->GetStringUTFChars(env, text, NULL) : NULL;
	return str_dup(chars ? chars : "unknown exception");
}

/** JNI's own UTF-8 variant differs from real UTF-8 for characters beyond the BMP, so use UTF-16. */
static jstring new_java_string(JNIEnv *env, const char *text) {
	size_t length = strlen(text);
	jchar *units = malloc((length + 1) * sizeof(jchar));
	jsize count = 0;
	for (const unsigned char *p = (const unsigned char *)text; *p;) {
		unsigned long code;
		int extra;
		if (*p < 0x80) {
			code = *p;
			extra = 0;
		} else if (*p < 0xE0) {
			code = *p & 0x1Ful;
			extra = 1;
		} else if (*p < 0xF0) {
			code = *p & 0x0Ful;
			extra = 2;
		} else {
			code = *p & 0x07ul;
			extra = 3;
		}
		p++;
		for (int i = 0; i < extra && *p; i++, p++)
			code = (code << 6) | (*p & 0x3Ful);
		if (code >= 0x10000) {
			code -= 0x10000;
			units[count++] = (jchar)(0xD800 + (code >> 10));
			units[count++] = (jchar)(0xDC00 + (code & 0x3FF));
		} else {
			units[count++] = (jchar)code;
		}
	}
	return (*env)->NewString(env, units, count);
}

static void run_java(void *parameter) {
	launch_request *request = parameter;
	const launcher_config *config = request->config;
	request->exit_code = EXIT_LAUNCHER_ERROR;

	char *error = NULL;
	char *classpath = plat_encode_option(str_fmt("-Djava.class.path=%s", request->classpath), &error);
	char *command = classpath ? plat_encode_option(str_fmt("-Dsun.java.command=%s", config->main_class), &error) : NULL;
	if (!command) {
		plat_report_error(error);
		return;
	}
	JavaVMOption *options = calloc(config->jvm_options.count + 3, sizeof(JavaVMOption));
	size_t used = 0;
	options[used++].optionString = classpath;
	options[used].optionString = "vfprintf";
	options[used++].extraInfo = (void *)capture_vm_output;
	options[used++].optionString = command;
	for (size_t i = 0; i < config->jvm_options.count; i++) {
		char *option = plat_encode_option(config->jvm_options.items[i], &error);
		if (!option) {
			plat_report_error(error);
			return;
		}
		options[used++].optionString = option;
	}
	JavaVMInitArgs vm_args = {JNI_VERSION_1_8, (jint)used, options, JNI_FALSE};
	JavaVM *vm;
	JNIEnv *env;
	if (request->create_vm(&vm, (void **)&env, &vm_args) != JNI_OK) {
		plat_report_error(str_fmt("Java failed to start.\n\n%s", vm_messages));
		return;
	}

	char *class_path_name = str_dup(config->main_class);
	for (char *p = class_path_name; *p; p++) {
		if (*p == '.')
			*p = '/';
	}
	jclass main_class = (*env)->FindClass(env, class_path_name);
	if (!main_class) {
		char *text = exception_text(env, (*env)->ExceptionOccurred(env));
		(*env)->ExceptionClear(env);
		plat_report_error(str_fmt("Can't load main class %s:\n%s\n\nClasspath:\n%s", config->main_class, text, request->classpath));
		return;
	}
	jmethodID main_method = (*env)->GetStaticMethodID(env, main_class, "main", "([Ljava/lang/String;)V");
	if (!main_method) {
		(*env)->ExceptionClear(env);
		plat_report_error(str_fmt("%s has no public static void main(String[]) method.", config->main_class));
		return;
	}

	jclass string_class = (*env)->FindClass(env, "java/lang/String");
	jobjectArray args = (*env)->NewObjectArray(env, (jsize)request->args.count, string_class, NULL);
	for (size_t i = 0; i < request->args.count; i++)
		(*env)->SetObjectArrayElement(env, args, (jsize)i, new_java_string(env, request->args.items[i]));

	(*env)->CallStaticVoidMethod(env, main_class, main_method, args);

	request->exit_code = 0;
	jthrowable failure = (*env)->ExceptionOccurred(env);
	if (failure) {
		request->exit_code = 1;
		(*env)->ExceptionDescribe(env);
		if (!plat_has_stderr())
			plat_report_error(exception_text(env, failure));
	}
	(*vm)->DetachCurrentThread(vm);
	// waits for non-daemon threads, such as JavaFX's
	(*vm)->DestroyJavaVM(vm);
}

static char *resolve(const char *app_dir, const char *path) {
	return plat_is_absolute(path) ? str_dup(path) : plat_join(app_dir, path);
}

/** The ini's path: the exe's, with any .exe replaced. Other dots are part of the app's name. */
static char *ini_path_of(const char *exe) {
	char *base = str_dup(exe);
	size_t length = strlen(base);
	if (length > 4 && plat_same_extension(base + length - 4, ".exe"))
		base[length - 4] = '\0';
	return str_fmt("%s.ini", base);
}

int launcher_run(const char *exe, const char *app_dir, const strlist *command_line) {
	char *ini_path = ini_path_of(exe);
	char *text = plat_read_file(ini_path);
	if (!text) {
		plat_report_error(str_fmt("Configuration file not found:\n%s", ini_path));
		return EXIT_LAUNCHER_ERROR;
	}
	launcher_config config;
	char *error = NULL;
	if (config_parse(text, app_dir, &config, &error) != 0) {
		plat_report_error(str_fmt("%s\n%s", ini_path, error));
		return EXIT_LAUNCHER_ERROR;
	}
	if (config.log_file && *config.log_file)
		plat_open_log(resolve(app_dir, config.log_file));
	if (config.app_id)
		plat_set_app_id(config.app_id);

	// the app's folder by default, so relative paths work however it was started
	if (!config.working_dir || strcmp(config.working_dir, "inherit") != 0) {
		char *dir = config.working_dir && *config.working_dir ? resolve(app_dir, config.working_dir) : str_dup(app_dir);
		if (!plat_change_dir(dir)) {
			plat_report_error(str_fmt("Can't use working-dir %s", dir));
			return EXIT_LAUNCHER_ERROR;
		}
	}

	java_runtime runtime;
	strlist report = {0};
	if (!runtime_select(&config, app_dir, &runtime, &report)) {
		char *message = str_dup("No suitable Java was found.");
		if (config.min_java || config.max_java)
			message = str_fmt("%s\nmin-java: %d, max-java: %d (0 means no limit)", message, config.min_java, config.max_java);
		message = str_fmt("%s\n\nTried:", message);
		for (size_t i = 0; i < report.count; i++)
			message = str_fmt("%s\n%s", message, report.items[i]);
		if (report.count == 0)
			message = str_fmt("%s\nnothing: no bundled runtime, JAVA_HOME, installed Java or java on PATH", message);
		plat_report_error(message);
		return EXIT_LAUNCHER_ERROR;
	}
	char *classpath = classpath_build(&config, app_dir, &error);
	if (!classpath) {
		plat_report_error(error);
		return EXIT_LAUNCHER_ERROR;
	}

	char *dry_run = plat_getenv("BSNATIVE_LAUNCHER_DRY_RUN");
	if (dry_run && *dry_run) {
		plat_write_stdout(str_fmt("home=%s\nversion=%s\nclasspath=%s\n", runtime.home, runtime.version, classpath));
		return 0;
	}

	void *create_vm = plat_load_jvm(runtime.home, &error);
	if (!create_vm) {
		plat_report_error(error);
		return EXIT_LAUNCHER_ERROR;
	}

	launch_request request = {(create_vm_function)create_vm, &config, classpath, {0}, EXIT_LAUNCHER_ERROR};
	for (size_t i = 0; i < config.args.count; i++)
		strlist_add(&request.args, config.args.items[i]);
	for (size_t i = 0; i < command_line->count; i++)
		strlist_add(&request.args, command_line->items[i]);

	// like the java command: Java doesn't run on the process's first thread
	if (!plat_run_thread(run_java, &request, MAIN_THREAD_STACK_BYTES)) {
		plat_report_error("Can't start the Java main thread.");
		return EXIT_LAUNCHER_ERROR;
	}
	return request.exit_code;
}
