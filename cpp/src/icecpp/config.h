// **********************************************************************
//
// Copyright (c) 2001
// ZeroC, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

//
// This configuration file is based on the one generated by the
// configure script from the gcc-2.8.1 distribution.
//

#if defined(__linux__)
#   define HAVE_INTTYPES_H 1
#   define TIME_WITH_SYS_TIME 1
#   define HAVE_BCMP 1
#   define HAVE_BCOPY 1
#   define HAVE_BZERO 1
#   define HAVE_GETRLIMIT 1
#   define HAVE_INDEX 1
#   define HAVE_RINDEX 1
#   define HAVE_UNISTD_H 1
#elif defined(_WIN32)
#   include <malloc.h>
#   include <io.h>
#   pragma warning( disable : 4018 )
#   pragma warning( disable : 4244 )
#   define alloca _alloca
#endif

#define HAVE_SETRLIMIT 1
#define HAVE_STRERROR 1
#define HAVE_VPRINTF 1

#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1

#define CHAR_TYPE_SIZE 1
#define INT_TYPE_SIZE 4
#define LONG_TYPE_SIZE 4
#if defined(_WIN32)
#   define WCHAR_TYPE_SIZE 2
#elif defined(__linux__) && defined(i386)
#   define WCHAR_TYPE_SIZE 4
#else
#   error "unsupported operating system or platform"
#endif

#define TARGET_BELL 007
#define TARGET_BS 010
#define TARGET_TAB 011
#define TARGET_NEWLINE 012
#define TARGET_VT 013
#define TARGET_FF 014
#define TARGET_CR 015

static char *version_string = "2.8.1";

#define INCLUDE_DEFAULTS { { 0, 0, 0 } }
#define GCC_INCLUDE_DIR ""
#define FATAL_EXIT_CODE 33
#define SUCCESS_EXIT_CODE 0
#define PREFIX ""
#define OBJECT_SUFFIX ".cpp" // For -M option.
