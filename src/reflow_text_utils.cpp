/**
 * @file reflow_text_utils.cpp
 *
 * A big honkin' text reflow engine, used to reformat comments in 'enhanced' mode 2.
 *
 * This reflow engine works on a 'per-page' basis, where a 'page' here is one entire
 * comment. It does not work on a per-paragraph basis as that prevents the reflow
 * engine from making choices based on info spanning more than one paragraph in there,
 * such as when a bullet item spans multiple paragraphs and you like your text reflown
 * with spanning indent to properly identify the subsequent paragraphs as belonging
 * to the bullet item.
 *
 * Features:
 *
 * - recognizes (and applies) hanging indent
 * - widow and orphan control
 * - recognizes (nested) bullet lists
 * - recognizes (nested) numbered lists (numbering can be alphanumeric [configurable])
 * - allows enforced line breaks at end-of-sentence within a paragraph
 * - detects and keeps 'ASCII art' intact, allowing graphical documentation to survive
 * - recognizes boxed comments and can reflow these
 * - extremely flexible as almost all decision elements and parameters are fully
 *   configurable
 * - recognizes mixed 'leader' use and cleans up after you (e.g. when you're reflowing
 *   comments where only some lines are prefixed with a '*' comment lead character,
 *   a situation often happening when editing already formatted comments quickly in the
 *   heat of a deadline)
 * - supports a configurable set of 'directives', either as characters or tags, to hint
 *   the reflow engine (this is useful to keep a particular piece of formatted text
 *   exactly as-is, while the other parts are reflown)
 * - supports DoxyGen / JavaDoc / .NET documentation tags and adjusts formatting accordingly.
 *
 * This code resides in its own source file to help maintainance by keeping a very
 * specialized piece of output formatting functionality cordonned off.
 *
 * @author  Ger Hobbelt
   @maintainer Ger Hobbelt
 * @license GPL v2+
 */

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include "args.h"
#include "reflow_text.h"

#include "reflow_text_internal.h"






chunk_t *get_next_function(chunk_t *pc)
{
   while ((pc = chunk_get_next(pc)) != NULL)
   {
      if ((pc->type == CT_FUNC_DEF) ||
          (pc->type == CT_OC_MSG_DECL) ||
          (pc->type == CT_FUNC_PROTO))
      {
         return(pc);
      }
   }
   return(NULL);
}


chunk_t *get_next_class(chunk_t *pc)
{
   while ((pc = chunk_get_next(pc)) != NULL)
   {
      if (pc->type == CT_CLASS)
      {
         return(chunk_get_next(pc));
      }
   }
   return(NULL);
}










/**
Find @a needle in @a haystack: the @a haystack is an array of strings, terminated by a NULL entry a la 'C' argv[],
which is matched against @a needle: the first match is returned or NULL when @a needle does not match any of the items
in @a haystack.

@return A zero-based index to the matching @a haystack item or -1 when no match could be found.
*/
int str_in_set(const char **haystack, const char *needle, size_t len)
{
	int idx = 0;

	UNC_ASSERT(haystack);
	UNC_ASSERT(needle);

	while (*haystack)
	{
		size_t l = strlen(*haystack);
		if (len >= l)
		{
			if (!memcmp(*haystack, needle, l))
			{
				return idx; // *haystack;
			}
		}

		idx++;
		haystack++;
	}

	return -1; // NULL;
}



/**
Find if @a needle occurs in @a haystack. The @a haystack is a string, which lists all the viable
characters that may match @a needle, with a few augmentations compared to @ref is_set(): when the
string contains a alphanumeric 'A' or 'a' or a digit '0', then this is assumed to match all alphanumerics
or digits, respectively.

Note that 'A' will only match capitals, while 'a' will only match lower case alphanumerics (i.e.: 'a' to 'z').
*/
bool in_RE_set(const char *haystack, int needle)
{
	UNC_ASSERT(haystack);
	UNC_ASSERT(needle);

	if (in_set(haystack, needle))
	{
		return true;
	}
	for ( ; *haystack; haystack++)
	{
		if (unc_isupper(*haystack) && unc_isupper(needle))
		{
			return true;
		}
		if (unc_islower(*haystack) && unc_islower(needle))
		{
			return true;
		}
		if (unc_isdigit(*haystack) && unc_isdigit(needle))
		{
			return true;
		}
	}
	return false;
}





/**
Find first occurrence of any of the characters in @a set in the string @a str of length @len.

Return NULL if no match could be found.
*/
const char *strnchr_any(const char *src, const char *set, size_t len)
{
	for ( ; len > 0; len--)
	{
		UNC_ASSERT(*src);
		const char *f = strchr(set, *src++);
		if (f)
			return src - 1;
	}
	return NULL;
}


#if !defined(HAVE_STRNCHR)

/**
Find first occurrence of the character @a ch in the string @a str of length @len.

Return NULL if no match could be found.
*/
const char *strnchr(const char *src, const char ch, size_t len)
{
	UNC_ASSERT(src);
	for ( ; len > 0 && *src; len--)
	{
		if (ch == *src)
			return src;
	}
	return NULL;
}

#endif




/*
return the number of occurrences of 'c' in 'str'.
*/
int strccnt(const char *str, int c)
{
	int rv = 0;

	for ( ; *str; str++)
	{
		rv += (*str == c);
	}
	return rv;
}

/*
return the number of 'c' characters leading in the string 'str'.

This is is equivalent to strspn(str, c) but here c is a character argument instead of a string argument.
*/
int strleadlen(const char *str, int c)
{
	const char *s;

	for (s = str; *s == c; s++)
		;
	return (int)(s - str);
}

/*
return the number of 'c' characters trailing at the end of the string 'str'.

'one_past_end_of_str' equals 'str + strlen(str)' but allows this routine to also work
when inspecting non-NUL-terminated strings.
*/
int strtaillen(const char *str, const char *one_past_end_of_str, int c)
{
	const char *s;

	for (s = one_past_end_of_str; s > str && s[-1] == c; s--)
		;
	return (int)(one_past_end_of_str - s);
}


/*
return the number of characters matching the set trailing at the end of the string 'str'.

'one_past_end_of_str' equals 'str + strlen(str)' but allows this routine to also work
when inspecting non-NUl-terminated strings.
*/
int strrspn(const char *str, const char *one_past_end_of_str, const char *set)
{
	const char *s;

	for (s = one_past_end_of_str; s > str && strchr(set, s[-1]); s--)
		;
	return (int)(one_past_end_of_str - s);
}


/*
return a pointer to the first occurrence of 'c' or, when 'c' wasn't found, a pointer
to the NUL sentinel (end of string).
*/
const char *strchrnn(const char *str, int c)
{
	const char *s = strchr(str, c);

	return (s ? s: str + strlen(str));
}

/*
return a pointer to the first occurrence of 'c' or, when 'c' wasn't found, a pointer
to the NUL sentinel (end of string).
*/
char *strchrnn(char *str, int c)
{
	char *s = strchr(str, c);

	return (s ? s: str + strlen(str));
}


/**
Replace the leading series of characters @a old with the character @a replacement.

NOTE: replacement is done 'in line', i.e. the @a src string will be modified!
*/
char *strrepllead(char *str, int old, int replacement)
{
	char *s = str;

	UNC_ASSERT(s);
	while (*s == old)
	{
		*s++ = (char)replacement;
	}
	return str;
}



/*
Act like strdup() is 'src' is a valid string, otherwise 'strdup(default_str)'.
*/
char *strdupdflt(const char *src, const char *default_str)
{
	UNC_ASSERT(default_str);
	char *s = (src ? strdup(src) : strdup(default_str));
	UNC_ASSERT(s);
	return s;
}


#if !defined(HAVE_STRNDUP)

/*
Return a malloc()ed copy of the given text string/chunk.
*/
char *strndup(const char *src, size_t maxlen)
{
	char *s = (char *)malloc(maxlen + 1);
	UNC_ASSERT(s);
	strncpy(s, src, maxlen);
	s[maxlen] = 0;
	return s;
}

#endif



/**
Report the number of TABs in the input.
*/
int count_tabs(const char *text, size_t len)
{
	int count = 0;

	for ( ; len > 0; len--, text++)
	{
		if (*text == '\t')
			count++;
	}

	return count;
}



/**
Inspect the comment block (sans start/end markers) and determine the number of whitespace characters to strip
from each line.

Do this by counting the minimum number of leading whitespace (including possible '*' leading edge) for
the comment starting at it's second line.

When the first line has whitespace and it's not a special header, it's whitespace will be marked as to-be-stripped
as well.

Return the 0-based(!) column position of the text which should remain after clipping off such leading whitespace.
*/
int calc_leading_whitespace4block(const char *text, int at_column)
{
	/*
	find out how many leading spaces are shared among all lines.
	*/
	int theoretical_start_col = at_column;
	int cur_min_idx = INT_MAX;

	while (text)
	{
		text += strleadlen(text, '\n');
		int idx = strleadlen(text, ' ');
		if ((idx < cur_min_idx)
			&& unc_isprint(text[idx]))
		{
			cur_min_idx = idx;
		}
		text = strchr(text, '\n');
	}

	if (cur_min_idx < theoretical_start_col)
		return cur_min_idx;
	return theoretical_start_col;
}



/*
Check whether the given text starts with a HTML numeric entity
in either '&#[0-9]+;' or '&#x[0-9A-F]+;' standard format.

Return TRUE when so and set 'word_length' to the string length of this entity,
including the '&#' and ';' characters delineating it.

NOTE: when this function returns FALSE, the *word_length value is untouched.
*/
bool is_html_numeric_entity(const char *text, int *word_length)
{
#if 0
	if (word_length)
	{
		*word_length = 0;
	}
#endif
	if (!text || text[0] != '&')
		return false;
	if (text[1] != '#')
		return false;

	const char *accepted_set = "0123456789";
	int max_allowed_len = 10+2;
	int idx = 2;

	if (text[2] == 'x' || text[2] == 'X')
	{
		/* hex numeric constant */
		accepted_set = "0123456789ABCDEFabcdef";
		max_allowed_len = 8+3;
		idx = 3;
	}

	if (!strchr(accepted_set, text[idx++]))
		return false;

	for ( ; idx < max_allowed_len; idx++)
	{
		if (!strchr(accepted_set, text[idx]))
			break;
	}
	if (text[idx] != ';')
		return false;

	if (word_length)
	{
		*word_length = idx;
	}
	return true;
}


/*
Check whether the given text starts with a HTML numeric entity
in '&[A-Za-z0-9]+;' standard format.

Return TRUE when so and set 'word_length' to the string length of this entity,
including the '&' and ';' characters delineating it.

WARNING: this function does NOT check whether the entity name is correct/known
         according to HTML/XHTML standards!

NOTE: when this function returns FALSE, the *word_length value is untouched.
*/
bool is_html_entity_name(const char *text, int *word_length)
{
#if 0
	if (word_length)
	{
		*word_length = 0;
	}
#endif
	if (!text || text[0] != '&')
		return false;

	static const char *accepted_set1 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static const char *accepted_set2 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const int max_allowed_len = 10+2;
	int idx = 1;

	if (!strchr(accepted_set1, text[idx++]))
		return false;

	for ( ; idx < max_allowed_len; idx++)
	{
		if (!strchr(accepted_set2, text[idx]))
			break;
	}
	if (text[idx] != ';')
		return false;

	if (word_length)
	{
		*word_length = idx;
	}
	return true;
}








bool cmt_reflow::chunk_is_inline_comment(const chunk_t *pc)
{
	bool is_inline_comment = ((pc->flags & PCF_RIGHT_COMMENT) != 0);
	UNC_ASSERT(is_inline_comment ? pc->column > 1 : true);
	return is_inline_comment;
}

bool cmt_reflow::is_doxygen_tagmarker(const char *text, char doxygen_tag_marker)
{
	return (doxygen_tag_marker
					    ? *text == doxygen_tag_marker
					    : in_set("@\\", *text));
}

/*
A 'viable' bullet is a bullet which is either a non-alphanumeric character (or 2, or 3)
or a numeric or alphanumeric character followed by a terminating dot or other
non-alphanumeric character.

Nope, chapter numbering and that sort of stuff is not recognized as 'viable' bullets.
*/
bool cmt_reflow::is_viable_bullet_marker(const char *text, size_t len)
{
	const char *s = text;
	if (unc_isdigit(*s))
	{
		/* all numbers, optionally followed by zero or one alphanumeric */
		for (s++; unc_isdigit(*s); s++)
			;
		/* bullet order numbers larger than 99 are rediculous */
		if (s - text > 2)
			return false;

		/* plus one optional alphanumeric */
		if (unc_isalpha(*s))
			s++;

		/* must be followed by a non-aphanumeric printable */
		if (!unc_isprint(*s) || unc_isalnum(*s) || in_set("@$%^&*_-={[;\"'<>?/\\|~", *s))
		{
			return false;
		}
		s++;
	}
	else if (unc_isalpha(*s))
	{
		s++;

		/* single alphanumeric must be followed by a non-aphanumeric printable */
		if (!unc_isprint(*s) || unc_isalnum(*s) || in_set("@$%^&*_-={[;\"'<>?/\\|~", *s))
		{
			return false;
		}
		s++;
	}
	else
	{
		/* There may be at most 3 printable characters act together as a bullet. */
		for ( ; unc_isprint(*s) && !unc_isalnum(*s); s++)
			;

		if (s - text > 3)
			return false;
	}

	/*
	check against the specified bullet size: it's a FAIL when these don't match.
	*/
	if (len != (size_t)(s - text))
		return false;

	/* must be followed by at least one space */
	if (*s != ' ')
		return false;
	s += strleadlen(s, ' ');

	/* bullet must be followed on the same line by at least one more printable characters. */
	if (!unc_isprint(*s))
		return false;

	return true;
}





void cmt_reflow::resize_buffer(size_t extralen)
{
	size_t newlen = m_comment_len + extralen;
	if (newlen < m_comment_size && newlen > 0) /* makes sure the 'comment' pointer is always initialized, even for empty comments */
		return;

	size_t n = (m_comment_size < 128 ? 128 : m_comment_size);
	if (n < 4096)
	{
		for ( ; n < newlen + 1; n *= 2)
			;
	}
	else
	{
		for ( ; n < newlen + 1; n = (n * 3)/2)
			;
	}
	UNC_ASSERT(n > newlen);
	newlen = n;

	if (!m_comment)
	{
		m_comment = (char *)malloc(newlen);
		m_comment_len = 0;
		m_comment_size = newlen;
	}
	else
	{
		m_comment = (char *)realloc((void *)m_comment, newlen);
		m_comment_size = newlen;
	}

	if (!m_comment)
	{
      LOG_FMT(LERR, "%s: buffer allocation failed: out of memory\n", __func__);
      cpd.error_count++;
   }
}

/*
Helper functions: expands all TABs in the input text so we can be sure where
each input character was expected to be, at least visually.
*/

void cmt_reflow::push(const char *text)
{
	size_t len = strlen(text);

	push(text, len);
}

void cmt_reflow::push(const char *text, size_t len)
{
	resize_buffer(len);

	char *dst = m_comment + m_comment_len;
	while (len)
	{
		*dst++ = *text++;
		len--;
	}
	UNC_ASSERT((size_t)(dst - m_comment) < m_comment_size);
	*dst = 0;

	m_comment_len = dst - m_comment;
}

void cmt_reflow::push(char c, size_t len)
{
	resize_buffer(len);

	char *dst = m_comment + m_comment_len;
	while (len)
	{
		*dst++ = c;
		len--;
	}
	UNC_ASSERT((size_t)(dst - m_comment) < m_comment_size);
	*dst = 0;

	m_comment_len = dst - m_comment;
}



/**
 * Adds the javadoc-style @param and @return stuff, based on the params and
 * return value for pc.
 * If the arg list is '()' or '(void)', then no @params are added.
 * Likewise, if the return value is 'void', then no @return is added.
 */
void cmt_reflow::add_javaparam(chunk_t *pc)
{
   chunk_t *fpo;
   chunk_t *fpc;
   chunk_t *tmp;
   chunk_t *prev;
   bool    has_param = true;
   bool    need_nl   = false;

   fpo = chunk_get_next_type(pc, CT_FPAREN_OPEN, pc->level);
   if (fpo == NULL)
   {
      return;
   }
   fpc = chunk_get_next_type(fpo, CT_FPAREN_CLOSE, pc->level);
   if (fpc == NULL)
   {
      return;
   }

   /* Check for 'foo()' and 'foo(void)' */
   if (chunk_get_next_ncnl(fpo) == fpc)
   {
      has_param = false;
   }
   else
   {
      tmp = chunk_get_next_ncnl(fpo);
      if ((tmp == chunk_get_prev_ncnl(fpc)) &&
          chunk_is_str(tmp, "void", 4))
      {
         has_param = false;
      }
   }

   if (has_param)
   {
      tmp  = fpo;
      prev = NULL;
      while ((tmp = chunk_get_next(tmp)) != NULL)
      {
         if ((tmp->type == CT_COMMA) || (tmp == fpc))
         {
            if (need_nl)
            {
               push("\n");
            }
            need_nl = true;
            push("@param");
            if (prev != NULL)
            {
               push(" ");
               push(prev->str, prev->len);
               push(" TODO");
            }
            prev = NULL;
            if (tmp == fpc)
            {
               break;
            }
         }
         if (tmp->type == CT_WORD)
         {
            prev = tmp;
         }
      }
   }

   /* Do the return stuff */
   tmp = chunk_get_prev_ncnl(pc);
   if ((tmp != NULL) && !chunk_is_str(tmp, "void", 4))
   {
      if (need_nl)
      {
         push("\n");
      }
      push("@return TODO");
   }
}


/**
 * text starts with '$('. see if this matches a keyword and add text based
 * on that keyword.
 *
 * @return the number of characters eaten from the text
 */
int cmt_reflow::add_kw(const char *text) /* [i_a] */
{
	/* [i_a] strncmp vs. memcmp + len - now we don't need to scan to the end of the keyword in the caller! */
   if (strncmp(text, "$(filename)", 11) == 0)
   {
      push(path_basename(cpd.filename));
      return(11);
   }
   if (strncmp(text, "$(class)", 8) == 0)
   {
      chunk_t *tmp = get_next_class(m_first_pc);
      if (tmp != NULL)
      {
         push(tmp->str, tmp->len);
         return(8);
      }
   }

   /* If we can't find the function, we are done */
   chunk_t *fcn = get_next_function(m_first_pc);
   if (fcn == NULL)
   {
      return(0);
   }

   if (strncmp(text, "$(function)", 11) == 0)
   {
      if (fcn->parent_type == CT_OPERATOR)
      {
         push("operator ");
      }
      push(fcn->str, fcn->len);
      return(11);
   }
   if (strncmp(text, "$(javaparam)", 12) == 0)
   {
      add_javaparam(fcn);
      return(12);
   }
   if (strncmp(text, "$(fclass)", 9) == 0)
   {
      chunk_t *tmp = chunk_get_prev_ncnl(fcn);
      if ((tmp != NULL) && (tmp->type == CT_OPERATOR))
      {
         tmp = chunk_get_prev_ncnl(tmp);
      }
      if ((tmp != NULL) && ((tmp->type == CT_DC_MEMBER) ||
                            (tmp->type == CT_MEMBER)))
      {
         tmp = chunk_get_prev_ncnl(tmp);
         push(tmp->str, tmp->len);
         return(9);
      }
   }
   return(0);
}

bool cmt_reflow::detect_as_javadoc_chunk(chunk_t *pc, bool setup)
{
	size_t marker_len = 0;
	bool backref = false;

	if (pc
		&& ((pc->type == CT_COMMENT)
			|| (pc->type == CT_COMMENT_MULTI)
			|| (pc->type == CT_COMMENT_CPP)))
	{
		const char *text = pc->str + 2;
		int len = pc->len - 4;
		const char *eos = text + len;

		const char *eojd_marker = text;
		if (len > 0 && strchr("/*!<", *eojd_marker))
		{
			// if (pc->type == CT_COMMENT_MULTI || pc->type == CT_COMMENT)
			do
			{
				eojd_marker++;
			} while (strchr("/*!<", *eojd_marker));

			if (eojd_marker[-1] == '<')
			{
				backref = true;
			}

			bool has_content = false;
			const char *s;

			for (s = eojd_marker; s < eos && !has_content; s++)
			{
				has_content = !!unc_isalpha(*s);
			}

			if (pc->type == CT_COMMENT_CPP)
			{
				if (!has_content
					|| !strchr("/!<", *text))
				{
					/* not a real doxy/javadoc marker but something else */
					eojd_marker = text;
				}
			}
			else
			{
				if (pc->type == CT_COMMENT
					&& !has_content)
				{
					/* not a real doxy/javadoc marker but something else */

					eojd_marker = text;
				}

				if (!strchr("*!<", *text))
				{
					/* not a real doxy/javadoc marker but something else */
					eojd_marker = text;
				}
			}

			if (eojd_marker - text > 2)
			{
				/* a doxygen/javadoc marker which is also part of a boxed comment! */
				eojd_marker = text + 1 + backref;
			}
		}

		marker_len = eojd_marker - text;

		if (marker_len > 0)
		{
			if (setup)
			{
				m_is_doxygen_comment = true;
				m_is_backreferencing_doxygen_comment = backref;

				set_doxygen_marker(text, marker_len);
			}
			return true;
		}
	}

	return false;
}



/**
Expand the TABs in the input text; the output buffer will be dimensioned properly for this.

Also clean any trailing whitespace.

Be aware that 'first_column' is 1-based!
*/
size_t cmt_reflow::expand_tabs_and_clean(char **dst_ref, size_t *dstlen_ref, const char *src, size_t srclen, int first_column, bool part_of_preproc_continuation)
{
	int pos = 0;
	const int tabsize = m_tab_width;
	int t;
	int tab_count = count_tabs(src, srclen);
	size_t dstlen;
	UNC_ASSERT(dst_ref);
	UNC_ASSERT(dstlen_ref);
	*dstlen_ref = dstlen = srclen + tab_count * (tabsize - 1) + first_column + 2;
	char *dst;
	*dst_ref = dst = (char *)malloc(dstlen);
	char *last_nonwhite_idx = dst;

	/* a bit whacky assert, this one; just checkin' ... */
	UNC_ASSERT(part_of_preproc_continuation == ((cpd.in_preproc != CT_NONE) && (cpd.in_preproc != CT_PP_DEFINE)));

	for (; pos < first_column - 1; pos++)
	{
		UNC_ASSERT(dstlen > (size_t)(dst - *dst_ref));
		*dst++ = ' ';
		//dstlen--;
	}

	for ( ; srclen > 0; srclen--, src++)
	{
		switch (*src)
		{
		case '\t':
			/* expand to next input TAB position; 'pos' is 0-based... */
			t = pos + tabsize;
			t /= tabsize;
			t *= tabsize;
			for (; pos < t; pos++)
			{
				UNC_ASSERT(dstlen > (size_t)(dst - *dst_ref));
				*dst++ = ' ';
				//dstlen--;
			}
			break;

		case '\r':
			/* skip */
			break;

		case '\\':
			/* continuation or regular character/escape? */
			if (srclen > 1 && in_set("\r\n", src[1]) && part_of_preproc_continuation)
			{
				/* drop this one; it'll be regenerated on output anyway. */
				break;
			}
			if (0)
			{
		case '\n':
				/* trim trailing whitespace right now: */
				if (last_nonwhite_idx != dst)
					dst = last_nonwhite_idx;
				pos = -1;
				/* fall through */
			}
		default:
			UNC_ASSERT(dstlen > (size_t)(dst - *dst_ref));
			*dst++ = *src;
			//dstlen--;
			pos++;
			if (!in_set(" \t", *src))
			{
				last_nonwhite_idx = dst;
			}
			break;
		}
	}

	/* trim trailing whitespace right now: */
	if (last_nonwhite_idx != dst)
		dst = last_nonwhite_idx;

	UNC_ASSERT(dstlen > (size_t)(dst + 1 - *dst_ref));
	*dst = 0;

	return dst - *dst_ref;
}

/**
Remove the first and last NEWLINEs (empty lines, really) from the
comment text.
*/
void cmt_reflow::strip_first_and_last_nl_from_text(void)
{
	char * const text = m_comment;
	UNC_ASSERT_EX(strlen(text) <= m_comment_len, ("(%d != %d)", (int)strlen(text), (int)m_comment_len));

	if (*text)
	{
		char *s;
		char *last_nl = NULL;
		int nl_count = 0;

		// scan backward to strip trailing newlines + WS
		for (s = text + m_comment_len - 1; s >= text && unc_isspace(*s); s--)
		{
			if (*s == '\n')
			{
				last_nl = s;
				nl_count++;
			}
		}

		if (last_nl)
		{
			if (nl_count >= 1)
			{
				m_has_trailing_nl = true;
			}
			/* clip to before NL */
			*last_nl = 0;
		}

		// scan forward to strip leading newlines + WS
		last_nl = NULL;
		nl_count = 0;
		for (s = text; *s && unc_isspace(*s); s++)
		{
			if (*s == '\n')
			{
				last_nl = s;
				nl_count++;
			}
		}

		if (last_nl)
		{
			if (nl_count >= 1)
			{
				m_has_leading_nl = true;
			}
			/* strip leading NL */
			memmove(text, last_nl + 1, text - last_nl - 1 + m_comment_len + 1);
		}

		m_comment_len = strlen(text);
	}

	UNC_ASSERT(m_comment_len == strlen(text));
}


/**
Now strip leading '*' comment markers - count them so we know how many it took before (1 or 2?).

'*' will be considered a comment-leader character when it occurs as the first non-white character on
the second/third line of the multiline text.

As some comment formats like to dump a line of stars on the second line, we also have a look at the
subsequent lines when we hit such a condition.

NOTE: each block of (merged) comment does or does not have a leader character, but the ones that do
      have one MUST have the same leader character, because once determined, it stays that way for the
entire comment!
*/
int cmt_reflow::strip_nonboxed_lead_markers(char *text, int at_column)
{
	char *second_line = strchrnn(text, '\n');
	bool determine_leadin = (m_lead_marker == NULL);
	int min_cnt = 0;
	int horizontal_lead_index = 0;
	int pre_lead_ws_cnt = 0;
	int past_lead_ws_cnt = INT_MAX;
	char *last_nl = second_line;

	if (!*second_line)
		return 0;

	while (*second_line)
	{
		last_nl++;
		second_line++;
		second_line += strleadlen(second_line, ' ');

		int cnt = (int)strspn(second_line, m_defd_lead_markers);
		second_line += cnt;

		if (cnt > 0)
		{
			/* heuristic: cnt>2 == a line of stars. Count the minimum NON-ZERO number of leader chars per line. */
			if (min_cnt == 0 && cnt <= 2)
			{
				min_cnt = cnt;
				horizontal_lead_index = (int)(second_line - last_nl);
				pre_lead_ws_cnt = horizontal_lead_index - cnt;
				int past_cnt = strleadlen(second_line, ' ');
				if (past_lead_ws_cnt > past_cnt && unc_isprint(second_line[past_cnt]))
				{
					past_lead_ws_cnt = past_cnt;
				}
				if (determine_leadin)
				{
					m_lead_marker = strndup(second_line - 1, cnt);
				}
			}
			else if (min_cnt > cnt)
			{
				min_cnt = cnt;
				horizontal_lead_index = (int)(second_line - last_nl);
				int pre_cnt = horizontal_lead_index - cnt;
				if (pre_lead_ws_cnt > pre_cnt)
				{
					pre_lead_ws_cnt = pre_cnt;
				}
				int past_cnt = strleadlen(second_line, ' ');
				if (past_lead_ws_cnt > past_cnt && unc_isprint(second_line[past_cnt]))
				{
					past_lead_ws_cnt = past_cnt;
				}
				if (determine_leadin)
				{
					UNC_ASSERT(m_lead_marker);
					UNC_ASSERT((int)strlen(m_lead_marker) > cnt);
					memcpy(m_lead_marker, second_line - 1, cnt);
					m_lead_marker[cnt] = 0;
				}
			}
			else if (min_cnt == cnt && horizontal_lead_index > second_line - last_nl)
			{
				/* when a leadin char is discovered at an earlier horizontal position... */
				//min_cnt = cnt;
				horizontal_lead_index = (int)(second_line - last_nl);
				int pre_cnt = horizontal_lead_index - cnt;
				if (pre_lead_ws_cnt > pre_cnt)
				{
					pre_lead_ws_cnt = pre_cnt;
				}
				int past_cnt = strleadlen(second_line, ' ');
				if (past_lead_ws_cnt > past_cnt && unc_isprint(second_line[past_cnt]))
				{
					past_lead_ws_cnt = past_cnt;
				}
			}
			else if (min_cnt == cnt)
			{
				/* like the other lines; just make sure we collect the minimum post-lead whitespace count */
				int pre_cnt = horizontal_lead_index - cnt;
				if (pre_lead_ws_cnt > pre_cnt)
				{
					pre_lead_ws_cnt = pre_cnt;
				}
				int past_cnt = strleadlen(second_line, ' ');
				if (past_lead_ws_cnt > past_cnt && unc_isprint(second_line[past_cnt]))
				{
					past_lead_ws_cnt = past_cnt;
				}
			}
		}

		second_line = strchrnn(second_line, '\n');
		last_nl = second_line;
	}

	if (m_lead_cnt == 0)
		m_lead_cnt = min_cnt;
	UNC_ASSERT(m_lead_cnt == (m_lead_marker ? (int)strlen(m_lead_marker) : 0));

	if (min_cnt == 0)
		return 0;
	UNC_ASSERT(m_lead_marker != NULL);

	/*
	now we know how many leadin characters there are and which one is used:
	those are stripped from the text: more correctly, they are replaced with spaces.

	The left margin cutoff code later on will do the rest (left-adjusting the content).

	NOTE ABOUT BOXED COMMENTS: the characteristic of boxed comments is that lines
	which start with a character '*' also end with that same character '*'. This heuristic
	knowledge is applied here to ensure boxed layouts remain as they are...
	*/
	second_line = strchrnn(text, '\n');
	last_nl = second_line;
	char *previous_sol = text;
	bool previous_line_was_boxed = true;
	while (*second_line)
	{
		second_line++;
		second_line += strleadlen(second_line, ' ');

		/* see if we've got a 'boxed' line: */
		char *eol = strchrnn(second_line, '\n');
		eol -= strtaillen(second_line, eol, ' ');

		/*
		boxes are at least 3 chars wide and span entire 'paragraphs' (which are separated by
		empty lines).
		    */
		UNC_ASSERT(min_cnt > 0);
		bool maybe_boxed = (eol > second_line + max(min_cnt, m_lead_cnt) && 0 == strncmp(eol - m_lead_cnt, m_lead_marker, m_lead_cnt));

		if (maybe_boxed)
		{
			/* scan backwards to see whether the 'box' is a paragraph on its own. */
			int llwscnt = strleadlen(previous_sol, ' ');

			if (llwscnt < last_nl - previous_sol)
			{
				/* previous line contains more than just whitespace: this line is not a boxed comment section! */
				maybe_boxed = previous_line_was_boxed;
			}
		}
		if (maybe_boxed)
		{
			/* scan forward to see whether the 'box' is a paragraph on its own. */
			char *sl = strchrnn(second_line, '\n');

			while (*sl)
			{
				sl++;
				sl += strleadlen(sl, ' ');

				/* see if we've got a 'boxed' line: */
				char *el = strchrnn(sl, '\n');
				char *eol2 = el - strtaillen(sl, el, ' ');

				bool maybe_boxed2 = (eol2 > sl + max(min_cnt, m_lead_cnt) && 0 == strncmp(eol2 - m_lead_cnt, m_lead_marker, m_lead_cnt));

				if (eol2 == sl)
				{
					/* empty line or whitespace only: end of 'para' */
					break;
				}
				if (!maybe_boxed2)
				{
					/* all lines in the current 'paragraph' must be boxed like that! */
					maybe_boxed = false;
					break;
				}

				sl = el;
			}
		}

		if (maybe_boxed)
		{
			/* a boxed line */
		}
		else
		{
			/* not a boxed line; strip lead markers from the starting horizontal position onward. */
			int cnt = min_cnt;
			if (second_line - last_nl > horizontal_lead_index)
			{
				cnt -= (int)(second_line - last_nl) - horizontal_lead_index;
			}
			for ( ; cnt > 0 && in_set(m_lead_marker, *second_line); cnt--)
				*second_line++ = ' ';
		}

		previous_sol = last_nl;
		UNC_ASSERT(previous_sol != NULL);
		previous_sol++;
		previous_line_was_boxed = maybe_boxed;

		second_line = strchrnn(second_line, '\n');
		last_nl = second_line;
	}


	/*
	SIDE EFFECT: set up the current star-prefix related settings according to the results acquired above.
	*/
	if (m_extra_pre_star_indent < 0)
	{
		int diff = pre_lead_ws_cnt - (at_column - 1);
		m_extra_pre_star_indent = max(0, diff);
	}
	if (m_extra_post_star_indent < 0 && past_lead_ws_cnt != INT_MAX)
	{
		m_extra_post_star_indent = past_lead_ws_cnt;
	}

	return min_cnt;
}


void cmt_reflow::set_doxygen_marker(const char *marker, size_t len)
{
	/* only set the the marker once per comment */
	if (!m_doxygen_marker)
	{
		m_doxygen_marker = strndup(marker, len);
		UNC_ASSERT(m_doxygen_marker);
	}
}



















void cmt_reflow::push_chunk(chunk_t *pc)
{
	if (!m_first_pc)
	{
	   output_start(pc);
	   UNC_ASSERT(m_first_pc);
	}

   if (pc->type == CT_COMMENT_MULTI
		  || pc->type == CT_COMMENT)
   {
      push_text(pc->str + 2, pc->len - 4, false, 2, pc->orig_col, pc);
   }
   else
   {
	   UNC_ASSERT(pc->type == CT_COMMENT_CPP);
		push_text(pc->str + 2, pc->len - 2, true, 2, pc->orig_col, pc);
   }

	m_last_pc = pc;
}




/**
 Loads a comment. The initial C/C++ comment starter must be excluded from the text.

 Subsequent comment starters (if combining comments), should not be included.
 The comment closing marker (for C/D comments) should not be included either.

 This routine will expand keywords on the fly and will 'prerender' the comment at the
 specified input column, so as to produce a comment text which can be inspected by the
 generic reflowing engine.
 */
void cmt_reflow::push_text(const char *text, int len, bool esc_close, int first_extra_offset, int at_column, chunk_t *pc)
{
   bool was_star   = false;
   bool was_slash  = false;
   bool was_dollar = false;
   bool in_word    = false;

   if (len < 0)
	   len = (int)strlen(text);

   /* apply initial indent: */
   if (at_column < 0)
   {
	   at_column = m_orig_startcolumn;
   }
   UNC_ASSERT(at_column >= 1);
   //UNC_ASSERT(at_column >= m_orig_startcolumn);

   UNC_ASSERT(m_orig_startcolumn == at_column);

   /*
   before we go and 'position' the comment, we first check and remove any javadoc marker at the start as that
   is obstructing our operation.

   We could do this later on, but this is the most convenient place: the javadoc/doxygen marker starts right
   at 'text[0]'.
   */
   if (m_first_pc == pc && detect_as_javadoc_chunk(pc, true))
   {
	   /*
	   next: blow away the marker by replacing it with whitespace.

	   Since 'text' is non-modifiable, we do it a different way: we shift the 'first_extra_offset' N spaces
	   forward and so does 'text'; this will result in 'expand_tabs_and_clean()' dumping the
	   desired whitespace in there.
	   */
	   UNC_ASSERT(m_doxygen_marker);
	   int javadoc_marker_len = (int)strlen(m_doxygen_marker);
	   first_extra_offset += javadoc_marker_len;
	   len -= javadoc_marker_len;
	   text += javadoc_marker_len;
   }
   else if (m_first_pc != pc
			&& pc
			&& pc->type == CT_COMMENT_CPP
			&& m_doxygen_marker
			&& !strncmp(m_doxygen_marker, text, strlen(m_doxygen_marker)))
   {
	   /*
	   Extra: for merged C++ doxygen comments (i.e. '///' prefixed comments)
	   blow away the marker by replacing it with whitespace.

	   Since 'text' is non-modifiable, we do it a different way: we shift the 'first_extra_offset' N spaces
	   forward and so does 'text'; this will result in 'expand_tabs_and_clean()' dumping the
	   desired whitespace in there.
	   */
	   UNC_ASSERT(m_doxygen_marker);
	   int javadoc_marker_len = (int)strlen(m_doxygen_marker);
	   first_extra_offset += javadoc_marker_len;
	   len -= javadoc_marker_len;
	   text += javadoc_marker_len;
   }

   /*
   expand tabs in text now; this simplifies the remainder A LOT.

   Also trim trailing whitespace at the same time.
   */
   char *dst;
   size_t newlen;
   bool in_pp = ((cpd.in_preproc != CT_NONE) && (cpd.in_preproc != CT_PP_DEFINE));
   /*
   unfortunately, 'pc && (pc->flags & PCF_IN_PREPROC)' is also TRUE when inside a big #if 0 ... #endif chunk :-(
   */
   UNC_ASSERT((pc && (pc->flags & PCF_IN_PREPROC)) >= in_pp);
   len = (int)expand_tabs_and_clean(&dst, &newlen, text, len, first_extra_offset + at_column, in_pp);

   /*
   speed-up for heap manager: reserve [probably required] space for this comment up front.
   */
   resize_buffer(len);

   /*
   Now strip leading '*' comment markers - count them so we know how many it took before (1 or 2?).

   '*' will be considered a comment-leadin character when it occurs as the first non-white character on
   the second/third line of the multiline text.

   As some comment formats like to dump a line of stars on the second line, we also have a look at the
   subsequent lines when we hit such a condition.
   */
   int leader_len = strip_nonboxed_lead_markers(dst, at_column);

   /*
   when we arrive at this point, the text has been 'provisionally laid out', that is: all TABs
   have been discarded, preprocessor line continuation has been stripped off as well, and so
   has any trailing whitespace (these things can - and should - be added again in the output/render process anyhow).

   In short: from here on out, this chunk of text doesn't contain any 'comment' markers any longer,
   apart from the possible '*' leader from the second line onwards. These will be removed next.

   After that, this is 'just a piece of text' which can be reformatted/reflown in a generic way.

   Note:

   boxed comments which employ the comment start&end 2-char markers will have those markers removed,
   thus resulting in a possible 1..2 char whitespaced upper-left and bottom-right corner. This is
   a logical artifact of removing the comment markers, so keep this in mind when detecting boxed comments.
   */

   /* now scan the input text for:

   - keywords,
   - comment markers that must be escaped, and
   */
   UNC_ASSERT(first_extra_offset >= 0);
   //UNC_ASSERT(m_extra_pre_star_indent >= 0);
   //UNC_ASSERT(m_extra_post_star_indent >= 0);
   UNC_ASSERT(leader_len >= 0);
   int strip_column0 = calc_leading_whitespace4block(dst, at_column + max(3, leader_len + max(0, m_extra_pre_star_indent) + max(0, m_extra_post_star_indent)));

   /*
   now that we know the left WS edge, we can deduce whether the 'original comment' had whitespace trailing the possible 'star' prefix or C++ // comment marker.
   */
   if (pc->type == CT_COMMENT_CPP)
   {
	   int diff = strip_column0 - (at_column + 2 - 1);
	   if (m_extra_post_star_indent < 0 && diff >= 0)
	   {
		   m_extra_post_star_indent = diff;
	   }
   }
   else if (pc->type == CT_COMMENT_MULTI)
   {
	   int diff = strip_column0 - (at_column + 2 - 1);
	   if (m_extra_post_star_indent < 0 && diff >= 0)
	   {
		   m_extra_post_star_indent = diff;
	   }
   }
   else
   {
	   int diff = strip_column0 - (at_column + 2 - 1);
	   if (m_extra_post_star_indent < 0 && diff >= 0)
	   {
		   m_extra_post_star_indent = diff;
	   }
   }

   /*
   strip the 'first_extra_offset' extra WS for the first line only: those spaces were only in there
   as a 'stopgap' for the comment start marker and we don't want the text parser later on to
   'discover' this first line of comment text has N more leading WS than it actually had in
   the original source code.

   This stop gap was necessary to ensure the calc_leading_whitespace4block() code would be
   able to discover the global 'left edge' of the entire text being added here.
	*/
   const char *s;
   int strip_ws_cnt = strip_column0 + first_extra_offset;
   for (s = dst; *s; s++)
   {
	  UNC_ASSERT((int)strlen(dst) + 1 >= (int)(s - dst));
	  for (; strip_ws_cnt > 0 && *s == ' '; s++)
	  {
		  strip_ws_cnt--;
	  }
	  UNC_ASSERT(strip_ws_cnt != 0 ? (*s == '\n' || *s == 0 || s <= dst + strip_column0 + first_extra_offset) : 1);
	  strip_ws_cnt = 0;

      if (!was_dollar && m_kw_subst &&
          (*s == '$') && (s[1] == '('))
      {
         int kwlen = add_kw(s);
		 if (kwlen > 0)
		 {
			 s += kwlen - 1;
			UNC_ASSERT((int)strlen(dst) >= (int)(s - dst));
			continue;
		 }
      }

      /* Split the comment */
      if (*s == '\n')
      {
         in_word = false;
         push("\n");
		 strip_ws_cnt = strip_column0;
      }
	  else if (*s == 0)
	  {
         break;
	  }
	  else
      {
#if 0
		  /* Escape a C closure in a CPP comment */
         if (esc_close &&
             ((was_star && (*s == '/')) ||
              (was_slash && (*s == '*'))))
         {
            push("+"); // was: ' '
         }
#endif
		 if (!in_word && !unc_isspace(*s))
         {
            m_word_count++;
         }
         in_word = !unc_isspace(*s);
         push(*s, 1);
         was_star   = (*s == '*');
         was_slash  = (*s == '/');
         was_dollar = (*s == '$');
      }
   }

   free(dst);
}







void cmt_reflow::write2output(const char *text, size_t len)
{
	UNC_ASSERT(text);

	if (m_write_to_initial_column_pending && len > 0 && text[0] != '\n')
	{
		write_line_to_initial_column();
		m_write_to_initial_column_pending = false;
	}

	while (len-- > 0)
	{
		write(*text);
		if (*text == '\n')
		{
			/*
			Prevent trailing whitespace from appearing the output:

			Don't print leading whitespace block when this turns to be an empty line in the end. We will however only find out
			by the time we receive the next call to write2output() with a non-empty text.
			*/
			if (len > 0)
			{
				if (text[1] && text[1] != '\n')
				{
					write_line_to_initial_column();
				}
			}
			else
			{
				m_write_to_initial_column_pending = true;
			}
		}
		text++;
	}
}

void cmt_reflow::write2output(const char *text)
{
	write2output(text, strlen(text));
}

void cmt_reflow::write_line_to_initial_column(void)
{
	int left_col = m_left_global_output_column;
	int diff = left_col - get_global_block_left_column();
	UNC_ASSERT(diff >= 0);
	bool allow_tabs;
    chunk_t *prev = chunk_get_prev(m_first_pc);
	int max_tabbed_column = -1;
	bool first_thing_on_this_line;

    /* if not the first item on a line */
	if ((prev && prev->type != CT_NEWLINE) || chunk_is_inline_comment(m_first_pc))
	{
		first_thing_on_this_line = false;

		if (cpd.settings[UO_align_keep_tabs].b)
		{
		   allow_tabs = m_first_pc->after_tab;
		}
		else
		{
		   allow_tabs = (cpd.settings[UO_align_with_tabs].b &&
						 ((m_first_pc->flags & PCF_WAS_ALIGNED) != 0) &&
						 prev && ((prev->column + prev->len + 1) != m_first_pc->column));
		}
	}
	else
	{
		first_thing_on_this_line = true;

		allow_tabs = (cpd.settings[UO_indent_with_tabs].n != 0);
	}
	LOG_FMT(LOUTIND, " for comment: %d(%d)/%d -", m_first_pc->column, allow_tabs, m_first_pc->level);

	if (diff > 0)
	{
		switch (cpd.settings[UO_indent_with_tabs].n)
		{
		default:
		case 0:
			diff = 0;
			break;

		case 1:
#if 0
			diff = 1 + m_first_pc->brace_level * cpd.settings[UO_output_tab_size].n - get_global_block_left_column();
#else
			diff = 1 + m_first_pc->column_indent - get_global_block_left_column();
#endif
			break;

		case 2:
			if (!m_indent_cmt_with_tabs /* cpd.settings[UO_indent_cmt_with_tabs].b */)
			{
				diff = m_base_col - get_global_block_left_column();
			}
			break;
		}

		if (diff > 0)
		{
			max_tabbed_column = diff / cpd.settings[UO_output_tab_size].n;
		}
		else
		{
			max_tabbed_column = 0;
		}
	}
	else
	{
		allow_tabs = false;
	}

	output_to_column(m_left_global_output_column, allow_tabs, 1 + max_tabbed_column * cpd.settings[UO_output_tab_size].n);

	UNC_ASSERT(left_col >= get_global_block_left_column());
}




void cmt_reflow::output_start(chunk_t *pc)
{
	m_first_pc = pc;

	set_deferred_cmt_config_params_phase1();
}


/**
 * Checks to see if the current comment can be combined with the next comment.
 * The two can be combined if:
 *  1. They are the same type
 *  2. There is exactly one newline between them
 *  3. They are indented to the same level
 *  4. Neither is a doxygen/javadoc comment (when recognition for those is turned on)
 */
bool cmt_reflow::can_combine_comment(chunk_t *pc)
{
	/*
	Don't permit merging comment chunks when the first block is a
	multiline comment block or when the relevant comment merge config flag
	has been turned OFF.
	*/
	chunk_t *pc_1 = m_first_pc;
	if (!pc_1) pc_1 = pc;

	if (pc_1->type == CT_COMMENT_MULTI)
	{
		return false;
	}
	else if (pc_1->type == CT_COMMENT_CPP
		&& !cpd.settings[UO_cmt_cpp_group].b)
	{
		return false;
	}
	else if (pc_1->type == CT_COMMENT
		&& !cpd.settings[UO_cmt_c_group].b)
	{
		return false;
	}

   /* We can't combine if there is something other than a newline next */
   if (pc->parent_type == CT_COMMENT_START)
   {
      return false;
   }

   if (detect_as_javadoc_chunk(pc) || m_is_doxygen_comment)
   {
	   /*
	   one exception: when a series of C++ doxygen comment lines follow one another, those
	   are to be treated as a single comment!
	   */
	   if (pc && pc->type == CT_COMMENT_CPP)
	   {
		   /* next is a newline for sure, make sure it is a single newline */
		   chunk_t *next = chunk_get_next(pc);
		   if ((next != NULL) && (next->nl_count == 1))
		   {
			   /* Make sure the comment is the same type at the same column */
			   next = chunk_get_next(next);
			   if ((next != NULL) &&
				   (next->type == pc->type) &&
				   chunk_is_inline_comment(pc) == chunk_is_inline_comment(next) &&
				   detect_as_javadoc_chunk(next) /* && m_is_doxygen_comment */ &&
				   (((next->column == 1) && (pc->column == 1)) ||
				   ((next->column == m_brace_col) && (pc->column == m_brace_col)) ||
				   ((next->column > m_brace_col) && (pc->parent_type == CT_COMMENT_END))))
			   {
				   return true;
			   }
		   }
	   }
		return false;
   }

   /* next is a newline for sure, make sure it is a single newline */
   chunk_t *next = chunk_get_next(pc);
   if ((next != NULL) && (next->nl_count == 1))
   {
      /* Make sure the comment is the same type at the same column */
      next = chunk_get_next(next);
      if ((next != NULL) &&
          (next->type == pc->type) &&
		   chunk_is_inline_comment(pc) == chunk_is_inline_comment(next) &&
		  !detect_as_javadoc_chunk(pc) && !m_is_doxygen_comment &&
		   (((next->column == 1) && (pc->column == 1)) ||
           ((next->column == m_brace_col) && (pc->column == m_brace_col)) ||
           ((next->column > m_brace_col) && (pc->parent_type == CT_COMMENT_END))))
      {
         return true;
      }
   }
   return false;
}






int cmt_reflow::write2out_comment_start(paragraph_box *para, words_collection &words)
{
	UNC_ASSERT(para);

	m_write_to_initial_column_pending = false;
	write_line_to_initial_column();

	if (m_is_cpp_comment)
	{
		write2output("//");
		if (m_is_doxygen_comment)
		{
			UNC_ASSERT(m_doxygen_marker);
			strrepllead(m_doxygen_marker, '*', '/');
			write2output(m_doxygen_marker);
		}
	}
	else
	{
		write2output("/*");
		if (m_is_doxygen_comment)
		{
			UNC_ASSERT(m_doxygen_marker);
			strrepllead(m_doxygen_marker, '/', '*');
			write2output(m_doxygen_marker);
		}
	}

	return m_extra_post_star_indent;
}


int cmt_reflow::write2out_comment_next_line(void)
{
	write2output("\n");

	if (m_is_cpp_comment)
	{
		write2output("//");
		if (m_is_doxygen_comment)
		{
			UNC_ASSERT(m_doxygen_marker);
			strrepllead(m_doxygen_marker, '*', '/');
			write2output(m_doxygen_marker);
		}
	}
	else
	{
		int i;

		for (i = 0; i < m_extra_pre_star_indent; i++)
		{
			write2output(" ");
		}
		write2output(m_lead_marker);
	}

	return m_extra_post_star_indent;
}


void cmt_reflow::write2out_comment_end(int deferred_whitespace, int deferred_nl)
{
	int j;

	for (j = 1; j < deferred_nl; j++)
	{
		deferred_whitespace = write2out_comment_next_line();
	}
	if (deferred_nl > 0)
	{
		write2output("\n");

		deferred_whitespace = m_extra_post_star_indent;
	}

	/*
	when a comment has whitespace between per-line lead-in and the content itself, than it
	should also have that same whitespace between content and comment end marker when the
	end marker is printed on the same line as the last bit of content. */
	if (deferred_nl == 0 && deferred_whitespace == 0)
	{
		deferred_whitespace = m_extra_post_star_indent;
	}
	else if (deferred_nl > 0)
	{
		deferred_whitespace = m_extra_pre_star_indent;
	}

	if (!m_is_cpp_comment)
	{
		for (j = deferred_whitespace; j > 0; j -= min(16, j))
		{
			write2output("                ", min(16, j));
		}

		write2output("*/");
	}
}


