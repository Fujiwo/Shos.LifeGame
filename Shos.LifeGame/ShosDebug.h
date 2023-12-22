#pragma once

#ifndef _DEBUG
#define DebugOutput(msg, ...)
#else	// ifndef _DEBUG else
#ifdef UNICODE
#define DebugOutput(msg, ...) _RPT_BASE_W(_CRT_WARN, NULL, 0, NULL, msg, __VA_ARGS__)
#else	// ifdef UNICODE else
#define DebugOutput(msg, ...) _RPT_BASE(_CRT_WARN, NULL, 0, NULL, msg, __VA_ARGS__)
#endif	// ifdef UNICODE
#endif	// ifndef _DEBUG
