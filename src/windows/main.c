#include "platform_win.h"

#include <shellapi.h>
#include <string.h>

/** Without this, output is lost when started from a terminal, as this is a GUI program. */
static void attach_parent_console(void) {
	if (plat_has_stderr() || !AttachConsole(ATTACH_PARENT_PROCESS))
		return;
	HANDLE out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (out != INVALID_HANDLE_VALUE) {
		SetStdHandle(STD_OUTPUT_HANDLE, out);
		SetStdHandle(STD_ERROR_HANDLE, out);
	}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show) {
	(void)instance;
	(void)previous;
	(void)command_line;
	(void)show;
	attach_parent_console();

	wchar_t exe_path[32768];
	GetModuleFileNameW(NULL, exe_path, 32768);
	char *exe = to_utf8(exe_path);
	char *app_dir = str_dup(exe);
	*strrchr(app_dir, '\\') = '\0';

	strlist args = {0};
	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 1; i < argc; i++)
		strlist_add(&args, to_utf8(argv[i]));

	return launcher_run(exe, app_dir, &args);
}
