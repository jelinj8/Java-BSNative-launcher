#ifndef PLATFORM_WIN_H
#define PLATFORM_WIN_H

#include <windows.h>

#include "../common/launcher.h"

wchar_t *to_wide(const char *text);
char *to_utf8(const wchar_t *text);

#endif
