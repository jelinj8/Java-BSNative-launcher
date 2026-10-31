// Windows entry point: finds <exe name>.ini next to the exe, and runs Java inside this process, so
// the OS and tools that identify apps by executable see this exe rather than java.exe.
#include "platform_win.h"

#include <jni.h>
#include <shellapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void write_handle(DWORD which, const char *text) {
	HANDLE handle = GetStdHandle(which);
	if (handle && handle != INVALID_HANDLE_VALUE) {
		DWORD written;
		WriteFile(handle, text, (DWORD)strlen(text), &written, NULL);
	}
}

static int has_stderr(void) {
	HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
	return handle && handle != INVALID_HANDLE_VALUE;
}

/** Reports a failure the user has to see. A GUI app has no other way to say why it didn't start. */
static void report_error(const char *message) {
	write_handle(STD_ERROR_HANDLE, str_fmt("%s\n", message));
	char *no_dialog = plat_getenv("BSNATIVE_LAUNCHER_NO_DIALOG");
	if (!no_dialog || !*no_dialog)
		MessageBoxW(NULL, to_wide(message), L"Can't start the application", MB_OK | MB_ICONERROR);
}

/** Without this, output is lost when started from a terminal, as this is a GUI program. */
static void attach_parent_console(void) {
	if (has_stderr() || !AttachConsole(ATTACH_PARENT_PROCESS))
		return;
	HANDLE out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (out != INVALID_HANDLE_VALUE) {
		SetStdHandle(STD_OUTPUT_HANDLE, out);
		SetStdHandle(STD_ERROR_HANDLE, out);
	}
}

static void create_parent_dirs(const char *path) {
	char *copy = str_dup(path);
	for (char *p = copy + 3; *p; p++) {
		if (*p == '\\') {
			*p = '\0';
			CreateDirectoryW(to_wide(copy), NULL);
			*p = '\\';
		}
	}
}

/** Java's System.out and System.err follow these handles. */
static void open_log(const launcher_config *config, const char *app_dir) {
	if (!config->log_file || !*config->log_file)
		return;
	char *path;
	if (plat_is_absolute(config->log_file)) {
		path = plat_join(config->log_file, "");
		path[strlen(path) - 1] = '\0'; // plat_join normalized the slashes and added one
	} else {
		path = plat_join(app_dir, config->log_file);
	}
	create_parent_dirs(path);
	SECURITY_ATTRIBUTES inherit = {sizeof(inherit), NULL, TRUE};
	HANDLE log = CreateFileW(to_wide(path), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (log == INVALID_HANDLE_VALUE)
		return; // the app should still start without its log
	SetStdHandle(STD_OUTPUT_HANDLE, log);
	SetStdHandle(STD_ERROR_HANDLE, log);
}

/** JNI option strings use the system code page, so a path outside it can't be passed. */
static char *to_system_encoding(const char *text, char **error) {
	wchar_t *wide = to_wide(text);
	BOOL lossy = FALSE;
	int length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide, -1, NULL, 0, NULL, &lossy);
	if (lossy) {
		*error = str_fmt("This value contains characters the Windows system code page can't represent, so Java can't be given it:\n%s", text);
		return NULL;
	}
	char *result = malloc((size_t)length);
	WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide, -1, result, length, NULL, NULL);
	return result;
}

static jint JNICALL capture_vm_output(FILE *stream, const char *format, va_list args) {
	(void)stream;
	char buffer[2048];
	int length = vsnprintf(buffer, sizeof(buffer), format, args);
	vm_messages = str_fmt("%s%s", vm_messages, buffer);
	write_handle(STD_ERROR_HANDLE, buffer);
	return length;
}

static char *exception_text(JNIEnv *env, jthrowable exception) {
	jclass type = (*env)->GetObjectClass(env, exception);
	jmethodID to_string = (*env)->GetMethodID(env, type, "toString", "()Ljava/lang/String;");
	jstring text = to_string ? (*env)->CallObjectMethod(env, exception, to_string) : NULL;
	const char *chars = text ? (*env)->GetStringUTFChars(env, text, NULL) : NULL;
	return str_dup(chars ? chars : "unknown exception");
}

static jstring new_java_string(JNIEnv *env, const char *text) {
	wchar_t *wide = to_wide(text);
	return (*env)->NewString(env, (const jchar *)wide, (jsize)wcslen(wide));
}

static DWORD WINAPI run_java(void *parameter) {
	launch_request *request = parameter;
	const launcher_config *config = request->config;
	request->exit_code = EXIT_LAUNCHER_ERROR;

	char *error = NULL;
	char *classpath = to_system_encoding(str_fmt("-Djava.class.path=%s", request->classpath), &error);
	if (!classpath) {
		report_error(error);
		return 0;
	}
	size_t option_count = config->jvm_options.count + 3;
	JavaVMOption *options = calloc(option_count, sizeof(JavaVMOption));
	size_t used = 0;
	options[used++].optionString = classpath;
	options[used].optionString = "vfprintf";
	options[used++].extraInfo = (void *)capture_vm_output;
	options[used++].optionString = to_system_encoding(str_fmt("-Dsun.java.command=%s", config->main_class), &error);
	for (size_t i = 0; i < config->jvm_options.count; i++) {
		char *option = to_system_encoding(config->jvm_options.items[i], &error);
		if (!option) {
			report_error(error);
			return 0;
		}
		options[used++].optionString = option;
	}
	JavaVMInitArgs vm_args = {JNI_VERSION_1_8, (jint)used, options, JNI_FALSE};
	JavaVM *vm;
	JNIEnv *env;
	if (request->create_vm(&vm, (void **)&env, &vm_args) != JNI_OK) {
		report_error(str_fmt("Java failed to start.\n\n%s", vm_messages));
		return 0;
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
		report_error(str_fmt("Can't load main class %s:\n%s\n\nClasspath:\n%s", config->main_class, text, request->classpath));
		return 0;
	}
	jmethodID main_method = (*env)->GetStaticMethodID(env, main_class, "main", "([Ljava/lang/String;)V");
	if (!main_method) {
		(*env)->ExceptionClear(env);
		report_error(str_fmt("%s has no public static void main(String[]) method.", config->main_class));
		return 0;
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
		if (!has_stderr())
			report_error(exception_text(env, failure));
	}
	(*vm)->DetachCurrentThread(vm);
	// waits for non-daemon threads, such as JavaFX's
	(*vm)->DestroyJavaVM(vm);
	return 0;
}

/** Tells Windows to group this app's taskbar entries by its own ID. */
static void apply_app_id(const launcher_config *config) {
	if (!config->app_id)
		return;
	typedef HRESULT(WINAPI * set_id_function)(PCWSTR);
	HMODULE shell = LoadLibraryW(L"shell32.dll");
	set_id_function set_id = shell ? (set_id_function)(void *)GetProcAddress(shell, "SetCurrentProcessExplicitAppUserModelID") : NULL;
	if (set_id)
		set_id(to_wide(config->app_id));
}

static int launch(void) {
	wchar_t exe_path[32768];
	GetModuleFileNameW(NULL, exe_path, 32768);
	char *exe = to_utf8(exe_path);
	char *app_dir = str_dup(exe);
	char *last_slash = strrchr(app_dir, '\\');
	*last_slash = '\0';
	char *ini_path = str_dup(exe);
	char *dot = strrchr(ini_path, '.');
	if (dot > strrchr(ini_path, '\\'))
		*dot = '\0';
	ini_path = str_fmt("%s.ini", ini_path);

	char *text = plat_read_file(ini_path);
	if (!text) {
		report_error(str_fmt("Configuration file not found:\n%s", ini_path));
		return EXIT_LAUNCHER_ERROR;
	}
	launcher_config config;
	char *error = NULL;
	if (config_parse(text, app_dir, &config, &error) != 0) {
		report_error(str_fmt("%s\n%s", ini_path, error));
		return EXIT_LAUNCHER_ERROR;
	}
	open_log(&config, app_dir);
	apply_app_id(&config);

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
		report_error(message);
		return EXIT_LAUNCHER_ERROR;
	}
	char *classpath = classpath_build(&config, app_dir, &error);
	if (!classpath) {
		report_error(error);
		return EXIT_LAUNCHER_ERROR;
	}

	char *dry_run = plat_getenv("BSNATIVE_LAUNCHER_DRY_RUN");
	if (dry_run && *dry_run) {
		write_handle(STD_OUTPUT_HANDLE, str_fmt("home=%s\nversion=%s\nclasspath=%s\n", runtime.home, runtime.version, classpath));
		return 0;
	}

	// the JVM's libraries sit next to each other in bin, and java.dll is loaded from there
	SetDllDirectoryW(to_wide(plat_join(runtime.home, "bin")));
	char *jvm_path = plat_join(runtime.home, "bin\\server\\jvm.dll");
	HMODULE jvm = LoadLibraryW(to_wide(jvm_path));
	if (!jvm) {
		report_error(str_fmt("Can't load %s (Windows error %lu).", jvm_path, GetLastError()));
		return EXIT_LAUNCHER_ERROR;
	}
	create_vm_function create_vm = (create_vm_function)(void *)GetProcAddress(jvm, "JNI_CreateJavaVM");
	if (!create_vm) {
		report_error(str_fmt("%s has no JNI_CreateJavaVM.", jvm_path));
		return EXIT_LAUNCHER_ERROR;
	}

	launch_request request = {create_vm, &config, classpath, {0}, EXIT_LAUNCHER_ERROR};
	for (size_t i = 0; i < config.args.count; i++)
		strlist_add(&request.args, config.args.items[i]);
	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 1; i < argc; i++)
		strlist_add(&request.args, to_utf8(argv[i]));

	// like the java command: Java doesn't run on the process's first thread
	HANDLE thread = CreateThread(NULL, MAIN_THREAD_STACK_BYTES, run_java, &request, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
	if (!thread) {
		report_error("Can't start the Java main thread.");
		return EXIT_LAUNCHER_ERROR;
	}
	WaitForSingleObject(thread, INFINITE);
	return request.exit_code;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show) {
	(void)instance;
	(void)previous;
	(void)command_line;
	(void)show;
	attach_parent_console();
	return launch();
}
