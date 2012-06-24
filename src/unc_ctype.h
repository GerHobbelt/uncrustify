/**
 * @file unc_ctype.h
 * The ctype function are only required to handle values 0-255 and EOF.
 * A char is sign-extended when cast to an int.
 * With some C libraries, these values cause a crash.
 * These wrappers will properly handle all char values.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#ifndef UNC_CTYPE_H_INCLUDED
#define UNC_CTYPE_H_INCLUDED

#include "base_types.h"
#include <cctype>

static_inline bool in_set(const char *set, int chr)
{
    if (chr > 0)
    {
        return !!strchr(set, chr);
    }
    return false;
}

/**
 * Truncate anything except EOF (-1) to 0-255
 */
static_inline int unc_fix_ctype(int ch)
{
   return((ch == -1) ? -1 : (ch & 0xff));
}


static_inline int unc_isspace(int ch)
{
   return(isspace(unc_fix_ctype(ch)));
}


static_inline int unc_isprint(int ch)
{
   return(isprint(unc_fix_ctype(ch)));
}


static_inline int unc_isalpha(int ch)
{
   return(isalpha(unc_fix_ctype(ch)));
}


static_inline int unc_isalnum(int ch)
{
   return(isalnum(unc_fix_ctype(ch)));
}


/*
'is identifier?' Code identifiers can have alphanumerics and/or '_' and/or '$' (C! C++! PHP)
*/
static_inline int unc_isident(int ch)
{
    int c = unc_fix_ctype(ch);
    return isalnum(c) || in_set("$_", c);
}


static_inline int unc_toupper(int ch)
{
   return(toupper(unc_fix_ctype(ch)));
}


static_inline int unc_tolower(int ch)
{
   return(tolower(unc_fix_ctype(ch)));
}


static_inline int unc_isxdigit(int ch)
{
   return(isxdigit(unc_fix_ctype(ch)));
}


static_inline int unc_isdigit(int ch)
{
   return(isdigit(unc_fix_ctype(ch)));
}


static_inline int unc_isupper(int ch)
{
   return(isalpha(unc_fix_ctype(ch)) && (unc_toupper(unc_fix_ctype(ch)) == ch));
}


static_inline int unc_islower(int ch)
{
   return(isalpha(unc_fix_ctype(ch)) && (unc_tolower(unc_fix_ctype(ch)) == ch));
}


#endif /* UNC_CTYPE_H_INCLUDED */
