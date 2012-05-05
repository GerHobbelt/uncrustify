/**
 * @file windows_compat.h
 * Hacks to work with different versions of windows.
 * This is only included if WIN32 is set.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef WINDOWS_COMPAT_H_INCLUDED
#define WINDOWS_COMPAT_H_INCLUDED


#if (defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(WIN64)) \
	&& defined(_DEBUG)

#ifndef _CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC 1
#endif

#include <crtdbg.h>

#endif

#include <windows.h>


/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if stdbool.h conforms to C99. */
#undef HAVE_STDBOOL_H

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#undef HAVE_STRCASECMP

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strnchr' function. */
/* #undef HAVE_STRNCHR */

/* Define to 1 if you have the `strndup' function. */
#undef HAVE_STRNDUP

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if `actime' is a member of `struct utimbuf'. */
#define HAVE_STRUCT_UTIMBUF_ACTIME 1  /* Win32 has 'struct _utimbuf' but in non-strict STDC mode, it also offers 'struct utimbuf' */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/utime.h> header file. */
#define HAVE_SYS_UTIME_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to 1 if you have the `utime' function. */
#define HAVE_UTIME 1

/* Define to 1 if you have the <utime.h> header file. */
#undef HAVE_UTIME_H

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#undef TIME_WITH_SYS_TIME


/* Define to appropriate substitute if compiler doesnt have __func__ */
/* #undef __func__ */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to rpl_malloc if the replacement function should be used. */
/* #undef malloc */

/* Define to rpl_realloc if the replacement function should be used. */
/* #undef realloc */





#define NO_MACRO_VARARG

typedef char               CHAR;

typedef signed char        INT8;
typedef short              INT16;
typedef int                INT32;

typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;

#if defined(_MSC_VER)

typedef unsigned __int64   UINT64;

#ifndef PRIx64
#define PRIx64             "I64x"
#endif

#else

typedef unsigned long long UINT64;

#ifndef PRIx64
#define PRIx64             "llx"
#endif

#endif


/* eliminate GNU's attribute */
#define __attribute__(x)

/* MSVC compilers before VC7 don't have __func__ at all; later ones call it
 * __FUNCTION__.
 */
#ifdef _MSC_VER
#if _MSC_VER < 1300
#define __func__    "???"
#else
#define __func__    __FUNCTION__
#endif
#else /* _MSC_VER */
#define __func__    "???"
#endif

#include <stdio.h>
#include <string.h>

#undef snprintf
#define snprintf      _snprintf

#undef vsnprintf
#define vsnprintf     _vsnprintf

#undef strcasecmp
#define strcasecmp    _strcmpi

#undef strncasecmp
#define strncasecmp   _strnicmp

#undef strdup
#define strdup        _strdup

#undef fileno
#define fileno        _fileno

/* includes for _setmode() */
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <errno.h>

#ifdef _MSC_VER
#define mkdir(x, y) _mkdir(x)
#endif
#define PATH_SEP  '\\'

#ifdef _MSC_VER
#define inline    __inline
#endif

static inline int unc_rename(const char *srcfname, const char *dstfname)
{
#if !defined(MOVEFILE_COPY_ALLOWED) || !defined(MOVEFILE_REPLACE_EXISTING)
    /* windows can't rename a file if the target exists, so delete it
     * first. This may cause data loss if the tmp file gets deleted
     * or can't be renamed.
     */
    (void)unlink(filename_out);
	return rename(srcfname, dstfname);
#else
	BOOL rv = MoveFileExA(srcfname, dstfname, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);

	if (!rv)
	{
		_set_errno(EACCES);
		return EACCES;
	}

	return 0;
#endif
}

#endif   /* WINDOWS_COMPAT_H_INCLUDED */

