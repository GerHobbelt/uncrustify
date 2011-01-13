/**
 * @file base_types.h
 *
 * Defines some base types, includes config.h
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#ifndef BASE_TYPES_H_INCLUDED
#define BASE_TYPES_H_INCLUDED

#ifdef WIN32

#include "../win32/windows_compat.h"

#else /* not WIN32 */

#include "config.h"

#define PATH_SEP    '/'

#define __STDC_FORMAT_MACROS


#if defined HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined HAVE_STDINT_H
#include <stdint.h>
#else
#error "Don't know where int8_t is defined"
#endif


/* some of my favorite aliases */

typedef char       CHAR;

typedef int8_t     INT8;
typedef int16_t    INT16;
typedef int32_t    INT32;

typedef uint8_t    UINT8;
typedef uint16_t   UINT16;
typedef uint32_t   UINT32;
typedef uint64_t   UINT64;


#ifndef PRIx64
#define PRIx64             "llx"
#endif

#define unc_rename(a, b)	rename(a, b)

#endif   /* ifdef WIN32 */

/* and the good old SUCCESS/FAILURE */

#define SUCCESS    0
#define FAILURE    -1


/* and a nice macro to keep SlickEdit happy */

#define static_inline    static inline

/* and the ever-so-important array size macro */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)    (sizeof(x) / sizeof((x)[0]))
#endif

#if defined(NDEBUG)

#define UNC_ASSERT(expr)				((void)0)

#else

#include <stdarg.h>

#define UNC_ASSERT(expr)													\
		(!(expr)	      													\
		? report_assertion_failed(#expr, __func__, __FILE__, __LINE__, 0)	\
			, 0																\
		: 1)

#define UNC_ASSERT_EX(expr, msgcombo)										\
	do																		\
	{																		\
		if (!(expr))														\
		{																	\
			assert_extended_reporter __m msgcombo ;							\
			report_assertion_failed(#expr, __func__, __FILE__, __LINE__,	\
					&__m);													\
		}																	\
	} while (0)

class assert_extended_reporter
{
public:
	assert_extended_reporter();
	assert_extended_reporter(long int val);
	assert_extended_reporter(unsigned long int val);
	assert_extended_reporter(const char *msg, ...);
	virtual ~assert_extended_reporter();

	const char *c_msg() const
	{
		return msgbuf;
	}

protected:
	char buf[1024];
	char *msgbuf;
	int buflen;

	int print(int suggested_buflen, const char *msg, ...);
	int vprint(int suggested_buflen, const char *msg, va_list args);
};

void report_assertion_failed(const char *expr, const char *function, const char *filepath, int lineno, assert_extended_reporter *rprtr);

#endif

#endif   /* BASE_TYPES_H_INCLUDED */
