#pragma once


// Target Windows 2000 & higher
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500

// Set to use multibyte characters.
#define _MBCS 1

// Speed up build process with minimal headers.
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

// Eliminate some warnings.
#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_WARNINGS
#	define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef NO_COMPRESSION
#pragma comment(lib, "snappy.lib")
#include "snappy-c.h"
#endif

#define STUB_IN "res\\stub.bat"
#define STUB_OUT "res\\stub.bat.bin"

#define fatal(M,...) printf("Error: " M "\n",__VA_ARGS__); exit(EXIT_FAILURE)
#define error(M,...) printf("Error: " M "\n",__VA_ARGS__)
#define out(M,...) printf(M,__VA_ARGS__)
#define outl(M) printf("%s\n", M)

#ifndef bool
#	define bool BOOL
#	define true TRUE
#	define false FALSE
#endif

#ifdef _DEBUG
#	define debug(M,...) printf(M "\n",__VA_ARGS__)
#else
#	define debug(M,...) 
#endif
