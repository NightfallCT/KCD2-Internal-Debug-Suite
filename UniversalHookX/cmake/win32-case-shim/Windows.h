// MinGW's header is named "windows.h" (lowercase). The source uses the
// MSVC-style "Windows.h" spelling, which only resolves on a case-insensitive
// filesystem. This shim makes that spelling resolve when cross-compiling
// from a case-sensitive filesystem (e.g. Linux).
#pragma once
#include <windows.h>
