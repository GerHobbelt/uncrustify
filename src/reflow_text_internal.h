/**
 * @file reflow_text_internal.h
 *
 * A big honkin' text reflow engine, used to reformat comments in 'enhanced' mode CMT_REFLOW_MODE_DO_FULL_REFLOW.
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

#ifndef __COMMENT_REFLOW_ENGINE_INTERNAL_H__
#define __COMMENT_REFLOW_ENGINE_INTERNAL_H__

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include "args.h"
#include "reflow_text.h"
#include <cstring>
#include <cstdlib>
#include <float.h>


#if (defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(WIN64)) \
	&& defined(_DEBUG) && defined(_MSC_VER) && defined(_CRTDBG_MAP_ALLOC)

#define   new					new(_NORMAL_BLOCK, __FILE__, __LINE__)
//#define   delete				delete(_NORMAL_BLOCK, __FILE__, __LINE__)

#endif






#if defined(_MSC_VER)
#define STRING2(x) #x
#define STRING(x) STRING2(x)
#endif




#undef max
static_inline int max(int a, int b)
{
	return (a > b ? a : b);
}
#undef min
static_inline int min(int a, int b)
{
	return (a < b ? a : b);
}






/**
This is a bitfield representing the left/right-hand operator argument requirements:
	0: this is not a math operator
	1: requires left hand value
	2: requires right hand value
*/
typedef enum
{
	MO_NOT_AN_OP = 0,
	MO_UNARY_PREFIX_OP = 2, /**< e.g. '++a', '-5' */
	MO_UNARY_POSTFIX_OP = 1,  /**< e.g. 'b--' */
	MO_BINARY_OP = 3, /**< e.g. 'a + b', '2^^10' */

	/* the bitmasks for tests: */
	MO_TEST_LH_REQD = 1,
	MO_TEST_RH_REQD = 2,
} math_operator_t;

/*
Note: All variables in this struct are named/used such that zero(0) is a sensible default.
*/
struct reflow_box
{
public:
	const char *m_text;
	int m_word_length; /* number of characters occupied by non-breakable 'word' */

	int m_leading_whitespace_length; /* number of chars of whitespace */
	int m_trailing_whitespace_length; /* number of chars of whitespace */

	//int m_min_required_linebreak_before; /* minimum number of mandatory line breaks before this item */
	//int m_min_required_linebreak_after; /* minimum number of mandatory line breaks after this item */

	int m_left_priority; /* linebreak priority: 1000 = high (do not break), 1 = low (try to break here first) */
	int m_right_priority; /* linebreak priority: 1000 = high, 1 = low */

	int m_keep_with_next; /* number of subsequent items to keep with this one: widow-like control */
	int m_keep_with_prev; /* number of previous items to keep with this one: orphan-like control */

	bool m_is_non_reflowable; /* is this ASCII art or another type of non-reflowable content? */
	bool m_floodfill_non_reflow; /* this type of non-reflow must be expanded ('flood filled') across the entire paragraph */
	bool m_is_first_on_line; /* helper: is this the first non-whitespace token on this line? */
	bool m_is_punctuation;
	bool m_is_part_of_quoted_txt;
	bool m_suppress_quote_for_string_marking;
	bool m_is_quote;
    int m_orig_hpos; /* position of the word on the current line; this is used with re-aligning [multiline] XML elements, tables and multiline doxygen formulas */
	math_operator_t m_math_operator; /* whether the current operator requires either left- and/or right-hand values */
	bool m_is_math;	/* is part of a math expression */
	bool m_is_code;   /* is part of a programming expression */
	bool m_is_path;   /* is (part of) a directory */
	bool m_is_hyphenated; /* has a hyphen inside or at the end; in the latter case the remainder of the entire word is located on the next line of input text */
	bool m_is_bullet; /* this text identifies the bullet itself (simple bullets are 1 char wide, but others are possible) */
	bool m_is_doxygen_tag; /* is a doxygen/javadoc documentation tag */
	bool m_is_inline_javadoc_tag; /* is an in-line doxygen/javadoc documentation tag */
	bool m_is_escape_code; /* is a C escape code or regex marker. */
	bool m_is_xhtml_entity; /* is a [X]HTML entity (named or numeric), e.g. '&#160;' */
	bool m_is_emphasized; /* when the word/sequence is surrounded by '*' or '/' to emphasize the word(s) within */
	bool m_is_part_of_boxed_txt; /* this chunk is part of (probably non-reflowable) boxed text */
	bool m_is_part_of_graphical_txt; /* this chunk is part of (probably non-reflowable) graphical text (a.k.a. ASCII art) */
	bool m_is_xhtml_start_tag; /* is a xml/html tag: start tag; another box will identify the (optional) end tag */
	bool m_is_xhtml_end_tag; /* is a xml/html tag: (optional) end tag; another box will identify the start tag */
	bool m_is_unclosed_xhtml_start_tag; /* is a start tag without a proper end tag; the end was assumed to exist before the @ref xhtml_matching_end_tag box */
	bool m_is_unmatched_xhtml_end_tag; /* is an end tag without a related start tag in the text; this is an XML/XHTML error, but allowed for HTML content, where such tags are to be ignored :-( */
	bool m_is_xhtml_tag_part; /* is a part of a xml/html tag; the tag end box also show up later on. The tag itself can be found in box 'xhtml_tag_begin' while the tag ends with the 'xhtml_tag_end' indexed box */
	bool m_is_cdata_xml_chunk; /* is a <![CDATA[...]]> text */
	bool m_is_uri_or_email; /* is probably an URI, with or without the xyz:// OR it is an email address */
	int m_xhtml_matching_end_tag; /* -1 if not used; points at the matching end tag */
	int m_xhtml_matching_start_tag; /* -1 if not used; points at the matching start tag */
	int m_xhtml_tag_part_begin; /* -1 if not used; points to the start of the current tag; see 'is_xhtml_tag_part' above */
	int m_xhtml_tag_part_end; /* -1 if not used; points to the last box of the current tag; see 'is_xhtml_tag_part' above */

	bool m_do_not_print; /* special purpose: 'empty' boxes which identify old line breaks can be disabled in the output process this way */

	/* number of newlines immediately preceding this word */
	int m_line_count;

	/* only for 'boxed text' words: */
	const char *m_left_edge_text;   /* the box 'left edge' used here. 0 when none was used. */
	int m_left_edge_thickness;
	//int m_left_edge_trailing_whitespace;
	const char *m_right_edge_text;   /* the box 'right edge' used here. 0 when none was used. */
	int m_right_edge_thickness;
	//int m_right_edge_leading_whitespace;

public:
	bool box_is_a_usual_piece_of_text(bool count_basic_punctuation_as_unusual = false) const;
};


/*
Note: All variables in this struct are named/used such that zero(0) is a sensible default.
*/
struct paragraph_box
{
	int m_first_box; /* index to first reflow_box */
	int m_last_box; /* index to last reflow_box */

	paragraph_box *m_previous_sibling; /* 0: none */
	paragraph_box *m_next_sibling; /* 0: none */
	paragraph_box *m_first_child; /* 0: leaf node */
	paragraph_box *m_parent; /* 0: root node */

	int m_first_line_indent; /* number of indenting spaces for first line in paragraph. */
	int m_hanging_indent; /* number of indenting spaces for second and further lines */
	bool m_starts_on_new_line; /* helper: does this paragraph start on a new line? */

	int m_keep_with_next; /* number of subsequent items to keep with this one: widow-like control */
	int m_keep_with_prev; /* number of previous items to keep with this one: orphan-like control */

	bool m_is_non_reflowable; /* is this ASCII art or another type of non-reflowable content? */
	bool m_is_boxed_txt; /* this chunk is a (probably non-reflowable) boxed text */
	bool m_is_graphics;
	int m_graphics_trigger_box;
	int m_nonreflow_trigger_box;

	bool m_indent_as_previous; /* follows the same indentation rules as the previous paragraph */
	bool m_continue_from_previous; /* is a continuation from the previous paragraph, i.e. the previous 'hanging indent' setting is the indent setting for this entire paragraph, including its first line */

	bool m_is_bullet; /* is a bullet item within a bullet list */
	bool m_is_bulletlist; /* part of a bullet list */
	int m_bullet_box; /* index to the bullet itself */
	int m_bulletlist_level; /* list hierarchy level: 1..N */

	bool m_is_doxygen_par; /* is a doxygen/javadoc documentation paragraph, i.e. one that starts with a doc tag */
	int m_doxygen_tag_box; /* index pointing at the doxygen/javadoc tag which started this paragraph */

	bool m_is_xhtml; /* is a piece of xml/html formatted text: start tag + (optional) end tag */
	bool m_is_unclosed_html_tag; /* is a piece of html formatted text with a start tag but NO END TAG */
	bool m_is_dangling_xhtml_close_tag; /* is a unmatched xml/xhtml end tag. :-S */
	int m_xhtml_start_tag_box; /* index pointing at the starting xml/html tag, e.g. '<p>' or '<pre>' */
	int m_xhtml_end_tag_box; /* index pointing at the terminating xml/html tag, e.g. '</p>' or '</pre>'; points to the next closing 'parent' tag when 'is_unclosed_html_tag' */
	paragraph_box *m_xhtml_start_tag_container; /* point at para which contains the start tag box */
	paragraph_box *m_xhtml_end_tag_container; /* point at para which contains the end tag box */

	int m_leading_whitespace_length; /* number of chars of whitespace */
	int m_trailing_whitespace_length; /* number of chars of whitespace */

	int m_min_required_linebreak_before; /* minimum number of line breaks before this item */
	int m_min_required_linebreak_after; /* minimum number of line breaks after this item */

	//int m_forced_linebreak_before; /* number of mandatory line breaks before this item */
	//int m_forced_linebreak_after; /* number of mandatory line breaks after this item */

	bool m_is_math;	/* is part of a math expression */
	bool m_is_code;   /* is part of a programming expression */
	bool m_is_path;   /* is (part of) a directory */
	bool m_is_intermission; /* is an intermission chunk, i.e. a chunk within a 'real' paragraph with some particular formatting demands. */
	//bool m_is_reflow_par; /* is regular paragraph of text, i.e. is delimited by newlines, i.e. ** reflow break points **. Can be a bullet item or something else as well, just as long as this chunk is sharing it's lines with any explicit or implicit sibling paragraphs! */

	/* only for 'boxed text' words: */
	char *m_left_edge_text;   /* the box 'left edge' used here. 0 when none was used. */
	int m_left_edge_thickness;
	int m_left_edge_trailing_whitespace;
	char *m_right_edge_text;   /* the box 'right edge' used here. 0 when none was used. */
	int m_right_edge_thickness;
	int m_right_edge_leading_whitespace;

public:
	paragraph_box();
	~paragraph_box();

public:
	bool para_is_a_usual_piece_of_text(void) const;
};



template<class T> class items_collection
{
	//friend class cmt_reflow;

protected:
	T *m_items;
	size_t m_item_count;
	size_t m_item_allocsize;

private:
	/* explicitly generate a non-accessible default constructor to shut up some compilers */
	items_collection()
		: m_item_count(0), m_item_allocsize(0), m_items(NULL)
	{
		UNC_ASSERT(!"Should never get here!");
	}

protected:
	items_collection(size_t prealloc_count)
		: m_item_count(0)
	{
		m_item_allocsize = 1 + prealloc_count;
		m_items = (T *)calloc(m_item_allocsize, sizeof(m_items[0]));
		UNC_ASSERT(m_items);
	}
	items_collection(const items_collection &src)
		: m_item_count(src.m_item_count),
		  m_item_allocsize(src.m_item_allocsize)
	{
		if (&src != this)
		{
			UNC_ASSERT(m_item_allocsize > 0);
			m_items = (T *)calloc(m_item_allocsize, sizeof(m_items[0]));
			UNC_ASSERT(m_items);

			size_t i;
			for (i = 0; i < m_item_count; i++)
			{
				m_items[i] = src.m_items[i];
			}
		}
	}
public:
	items_collection &operator =(const items_collection &src)
	{
		if (&src != this)
		{
			m_item_count = src.m_item_count;
			m_item_allocsize = src.m_item_allocsize;

			UNC_ASSERT(m_item_allocsize > 0);
			m_items = (T *)calloc(m_item_allocsize, sizeof(m_items[0]));
			UNC_ASSERT(m_items);

			size_t i;
			for (i = 0; i < m_item_count; i++)
			{
				m_items[i] = src.m_items[i];
			}
		}
	}
public:
	virtual ~items_collection()
	{
		if (m_items)
		{
			free(m_items);
		}
	}

	/* make sure there space for at least N items in the collection */
	void reserve(size_t n)
	{
		if (n > m_item_allocsize)
		{
			/* grow at a steady pace; prevent reallocing for every single reserve(+1) */
			if (n < 256)
			{
				size_t m = n;
				for (n = m_item_allocsize; n < m; n *= 2)
					;
			}
			else
			{
				size_t m = n;
				for (n = m_item_allocsize; n < m; n = (n * 3)/2)
					;
			}
			m_items = (T *)realloc(m_items, n * sizeof(m_items[0]));
			UNC_ASSERT(m_items);
			memset(m_items + m_item_allocsize, 0, (n - m_item_allocsize) * sizeof(m_items[0]));
			m_item_allocsize = n;
		}
	}

	T *prep_next(int &item_idx)
	{
		UNC_ASSERT(item_idx >= -1);
		item_idx++;
		reserve(item_idx + 1);
		if (m_item_count <= (size_t)item_idx)
		{
			m_item_count = item_idx + 1;
		}

		return &m_items[item_idx]; // won't be part of the 'item_count' yet!
	}

	T *get_printable_prev(int &item_idx, int lowest_allowed_idx = 0)
	{
		UNC_ASSERT(lowest_allowed_idx >= 0);
		while (--item_idx >= lowest_allowed_idx)
		{
			UNC_ASSERT(item_idx >= 0);
			UNC_ASSERT(item_idx < (int)m_item_count);
			if (!m_items[item_idx].m_do_not_print)
				return &m_items[item_idx];
		}
		return NULL;
	}

	T *get_printable_next(int &item_idx, int highest_allowed_idx = INT_MAX)
	{
		UNC_ASSERT(highest_allowed_idx >= 0);
		if (highest_allowed_idx >= (int)m_item_count)
		{
			highest_allowed_idx = (int)m_item_count - 1;
		}
		while (++item_idx <= highest_allowed_idx)
		{
			UNC_ASSERT(item_idx >= 0);
			UNC_ASSERT(item_idx < (int)m_item_count);
			if (!m_items[item_idx].m_do_not_print)
				return &m_items[item_idx];
		}
		return NULL;
	}

	size_t count(void) const
	{
		return m_item_count;
	}
	T &operator[](size_t idx)
	{
		// UNC_ASSERT(idx >= 0); -- always true
		UNC_ASSERT(idx < m_item_count);
		return m_items[idx];
	}
	const T &operator[](size_t idx) const
	{
		// UNC_ASSERT(idx >= 0); -- always true
		UNC_ASSERT(idx < m_item_count);
		return m_items[idx];
	}
};





class words_collection : public items_collection<reflow_box>
{
	friend class paragraph_collection;

public:
	words_collection(cmt_reflow &cmt)
		: items_collection<reflow_box>(4 + cmt.m_comment_len / 4 /* heuristic (test cases average a ratio of 0.227) */)
	{
	}
	words_collection(const words_collection &src)
		: items_collection<reflow_box>(src)
	{
	}
};






chunk_t *get_next_function(chunk_t *pc);
chunk_t *get_next_class(chunk_t *pc);




/**
Find @a needle in @a haystack: the @a haystack is an array of strings, terminated by a NULL entry a la 'C' argv[],
which is matched against @a needle: the first match is returned or NULL when @a needle does not match any of the items
in @a haystack.

@return A zero-based index to the matching @a haystack item or -1 when no match could be found.
*/
int str_in_set(const char **haystack, const char *needle, size_t len);


/**
Find if @a needle occurs in @a haystack. The @a haystack is a string, which lists all the viable
characters that may match @a needle, with a few augmentations compared to @ref is_set(): when the
string contains a alphanumeric 'A' or 'a' or a digit '0', then this is assumed to match all alphanumerics
or digits, respectively.

Note that 'A' will only match capitals, while 'a' will only match lower case alphanumerics (i.e.: 'a' to 'z').
*/
bool in_RE_set(const char *haystack, int needle);




/**
Find first occurrence of any of the characters in @a set in the string @a str of length @len.

Return NULL if no match could be found.
*/
const char *strnchr_any(const char *src, const char *set, size_t len);


#if !defined(HAVE_STRNCHR)

/**
Find first occurrence of the character @a ch in the string @a str of length @len.

Return NULL if no match could be found.
*/
const char *strnchr(const char *src, const char ch, size_t len);

#endif




/*
return the number of occurrences of 'c' in 'str'.
*/
int strccnt(const char *str, int c);

/*
return the number of 'c' characters leading in the string 'str'.

This is is equivalent to strspn(str, c) but here c is a character argument instead of a string argument.
*/
int strleadlen(const char *str, int c);

/*
return the number of 'c' characters trailing at the end of the string 'str'.

'one_past_end_of_str' equals 'str + strlen(str)' but allows this routine to also work
when inspecting non-NUl-terminated strings.
*/
int strtaillen(const char *str, const char *one_past_end_of_str, int c);


/*
return the number of characters matching the set trailing at the end of the string 'str'.

'one_past_end_of_str' equals 'str + strlen(str)' but allows this routine to also work
when inspecting non-NUl-terminated strings.
*/
int strrspn(const char *str, const char *one_past_end_of_str, const char *set);


/*
return a pointer to the first occurrence of 'c' or, when 'c' wasn't found, a pointer
to the NUL sentinel (end of string).
*/
const char *strchrnn(const char *str, int c);

/*
return a pointer to the first occurrence of 'c' or, when 'c' wasn't found, a pointer
to the NUL sentinel (end of string).
*/
char *strchrnn(char *str, int c);


/**
Replace the leading series of characters @a old with the character @a replacement.

NOTE: replacement is done 'in line', i.e. the @a src string will be modified!
*/
char *strrepllead(char *str, int old, int replacement);


/*
Act like strdup() is 'src' is a valid string, otherwise 'strdup(default_str)'.
*/
char *strdupdflt(const char *src, const char *default_str);


#if !defined(HAVE_STRNDUP)

/*
Return a malloc()ed copy of the given text string/chunk.
*/
char *strndup(const char *src, size_t maxlen);

#endif



/**
Report the number of TABs in the input.
*/
int count_tabs(const char *text, size_t len);



/**
Inspect the comment block (sans start/end markers) and determine the number of whitespace characters to strip
from each line.

Do this by counting the minimum number of leading whitespace (including possible '*' leading edge) for
the comment starting at it's second line.

When the first line has whitespace and it's not a special header, it's whitespace will be marked as to-be-stripped
as well.

Return the 0-based(!) column position of the text which should remain after clipping off such leading whitespace.
*/
int calc_leading_whitespace4block(const char *text, int at_column);



/*
Check whether the given text starts with a HTML numeric entity
in either '&#[0-9]+;' or '&#x[0-9A-F]+;' standard format.

Return TRUE when so and set 'word_length' to the string length of this entity,
including the '&#' and ';' characters delineating it.

NOTE: when this function returns FALSE, the *word_length value is untouched.
*/
bool is_html_numeric_entity(const char *text, int *word_length);


/*
Check whether the given text starts with a HTML numeric entity
in '&[A-Za-z0-9]+;' standard format.

Return TRUE when so and set 'word_length' to the string length of this entity,
including the '&' and ';' characters delineating it.

WARNING: this function does NOT check whether the entity name is correct/known
         according to HTML/XHTML standards!

NOTE: when this function returns FALSE, the *word_length value is untouched.
*/
bool is_html_entity_name(const char *text, int *word_length);


#endif // __COMMENT_REFLOW_ENGINE_INTERNAL_H__



