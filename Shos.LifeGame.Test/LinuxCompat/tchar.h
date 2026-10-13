#pragma once

// Minimal substitute of <tchar.h> to build the regression tests with GCC/Clang on Linux.

#if defined(UNICODE) || defined(_UNICODE)
#error "LinuxCompat/tchar.h supports only the multi-byte character set."
#endif // UNICODE

#define _T(text) text

using TCHAR = char;
