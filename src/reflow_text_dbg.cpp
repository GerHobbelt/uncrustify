/**
 * @file reflow_text_dbg.cpp
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

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include "args.h"
#include "reflow_text.h"

#include "reflow_text_internal.h"











void cmt_reflow_ex::show_diagnostics(paragraph_box *para, words_collection &words)
{
	char buf[256];
	double ppwr;
	double wptr;
	static size_t global_p_cnt = 0;
	static size_t global_w_cnt = 0;
	static size_t global_t_cnt = 0;

	int para_count = 0;

	while (para)
	{
		para_count++;

		if (para->m_first_child)
		{
			para = para->m_first_child;
			continue;
		}

		/* when no more sibling, then traverse up the tree and go to the next sibling there. */
		while (!para->m_next_sibling && para->m_parent)
		{
			para = para->m_parent;
		}
		para = para->m_next_sibling;
	}

	ppwr = para_count;
	ppwr /= FLT_EPSILON + words.count();

	wptr = words.count();
	wptr /= FLT_EPSILON + m_comment_len;

	global_p_cnt += para_count;
	global_w_cnt += words.count();
	global_t_cnt += m_comment_len;

	sprintf(buf, "\n/*--- para/word ratio: %5.3f, words/text ratio: %5.3f -- ", ppwr, wptr);
	write2output(buf);

	ppwr = global_p_cnt;
	ppwr /= FLT_EPSILON + global_w_cnt;

	wptr = global_w_cnt;
	wptr /= FLT_EPSILON + global_t_cnt;

	sprintf(buf, "GLOBAL p/w ratio: %5.3f, w/txt ratio: %5.3f ---*/\n", ppwr, wptr);
	write2output(buf);
}


void cmt_reflow_ex::dump2output(words_collection &words, int mode, int start_idx, int end_idx)
{
	int idx;

	if (start_idx < 0)
	{
		start_idx = 0;
	}
	end_idx++; // convert inclusive edge to exclusive edge
	if (end_idx > (int)words.count())
	{
		end_idx = (int)words.count();
	}
	for (idx = start_idx; idx < end_idx; idx++)
	{
		reflow_box *box = &words[idx];
		reflow_box *prev_box = NULL;
		if (idx > 0)
		{
			prev_box = &words[idx - 1];
		}
		reflow_box *next_box = NULL;
		if (idx + 1 < (int)words.count())
		{
			next_box = &words[idx + 1];
		}

		size_t j;

		char buf[256];

		switch (mode)
		{
		default:
		case 0:
			if (idx != start_idx)
			{
				write("\n");
			}

			write(box->m_text, box->m_word_length);

			for (j = (box->m_word_length < 10 ? 10 - box->m_word_length : 0) + 1; j > 0; j--)
			{
				write(" ");
			}

			snprintf(buf, sizeof(buf), "[WL=%d:", box->m_word_length);
			write(buf);

			if (box->m_do_not_print)
			{
				write("NOPRINT:");
			}
			else
			{
				if (box->m_leading_whitespace_length != 1)
				{
					snprintf(buf, sizeof(buf), "lead.WS=%d:",
						box->m_leading_whitespace_length);
					write(buf);
				}
				if (box->m_trailing_whitespace_length != 1)
				{
					snprintf(buf, sizeof(buf), "trail.WS=%d:",
						box->m_trailing_whitespace_length);
					write(buf);
				}

				if (box->m_left_priority != 0)
				{
					snprintf(buf, sizeof(buf), "<PRIO=%d:",
						box->m_left_priority);
					write(buf);
				}
				if (box->m_right_priority != 0)
				{
					snprintf(buf, sizeof(buf), ">PRIO=%d:",
						box->m_right_priority);
					write(buf);
				}

				if (box->m_keep_with_prev != 0)
				{
					snprintf(buf, sizeof(buf), "<KEEP=%d:",
						box->m_keep_with_prev);
					write(buf);
				}
				if (box->m_keep_with_next != 0)
				{
					snprintf(buf, sizeof(buf), ">KEEP=%d:",
						box->m_keep_with_next);
					write(buf);
				}

				if (box->m_is_non_reflowable)
				{
					snprintf(buf, sizeof(buf), "!REFLOW:");
					write(buf);
				}
				if (box->m_is_first_on_line)
				{
					snprintf(buf, sizeof(buf), "1ST:");
					write(buf);
				}
				if (box->m_is_punctuation)
				{
					snprintf(buf, sizeof(buf), "PUNCT:");
					write(buf);
				}
				if (box->m_is_math)
				{
					if (box->m_math_operator & MO_TEST_LH_REQD)
					{
						snprintf(buf, sizeof(buf), "LH<");
						write(buf);
					}
					if (box->m_math_operator & MO_TEST_RH_REQD)
					{
						snprintf(buf, sizeof(buf), "RH>");
						write(buf);
					}
					snprintf(buf, sizeof(buf), "MATH:");
					write(buf);
				}
				if (box->m_is_code)
				{
					snprintf(buf, sizeof(buf), "CODE:");
					write(buf);
				}
				if (box->m_is_path)
				{
					snprintf(buf, sizeof(buf), "PATH:");
					write(buf);
				}
				if (box->m_is_uri_or_email)
				{
					snprintf(buf, sizeof(buf), "URL:");
					write(buf);
				}
				if (box->m_is_bullet)
				{
					snprintf(buf, sizeof(buf), "BULLET:");
					write(buf);
				}
				if (box->m_is_inline_javadoc_tag)
				{
					snprintf(buf, sizeof(buf), "INLINE:");
					write(buf);
				}
				if (box->m_is_doxygen_tag)
				{
					snprintf(buf, sizeof(buf), "DOC:");
					write(buf);
				}
				if (box->m_is_part_of_boxed_txt)
				{
					snprintf(buf, sizeof(buf), "BOX('%*.*s',%d,%d,'%*.*s'):",
						box->m_left_edge_thickness,
						box->m_left_edge_thickness,
						box->m_left_edge_text,
						box->m_left_edge_thickness,
						box->m_right_edge_thickness,
						box->m_right_edge_thickness,
						box->m_right_edge_thickness,
						box->m_right_edge_text);
					write(buf);
				}
				if (box->m_is_part_of_graphical_txt)
				{
					snprintf(buf, sizeof(buf), "ART:");
					write(buf);
				}
				if (box->m_is_quote)
				{
					snprintf(buf, sizeof(buf), "QUOTE:");
					write(buf);
				}
				if (box->m_is_part_of_quoted_txt)
				{
					snprintf(buf, sizeof(buf), "STRING:");
					write(buf);
				}
				if (box->m_is_hyphenated)
				{
					snprintf(buf, sizeof(buf), "HYPHEN:");
					write(buf);
				}
				if (box->m_is_escape_code)
				{
					snprintf(buf, sizeof(buf), "ESC:");
					write(buf);
				}

				if (box->m_is_xhtml_start_tag)
				{
					snprintf(buf, sizeof(buf), "XML->%d:",
						box->m_xhtml_matching_end_tag);
					write(buf);
				}
				if (box->m_is_xhtml_end_tag)
				{
					snprintf(buf, sizeof(buf), "/XML<-%d:",
						box->m_xhtml_matching_start_tag);
					write(buf);
				}
				if (box->m_is_unclosed_xhtml_start_tag)
				{
					snprintf(buf, sizeof(buf), "XML!closed:");
					write(buf);
				}
				if (box->m_is_unmatched_xhtml_end_tag)
				{
					snprintf(buf, sizeof(buf), "/XML!match:");
					write(buf);
				}
				if (box->m_is_cdata_xml_chunk)
				{
					snprintf(buf, sizeof(buf), "/CDATA:");
					write(buf);
				}

				if (box->m_line_count != 0)
				{
					snprintf(buf, sizeof(buf), "NL=%d:",
						box->m_line_count);
					write(buf);
				}
			}
			write("]");
			break;

		case 1:
			if (box->m_do_not_print)
			{
				write(".X.");
			}
			else
			{
				bool place_brackets = false;

				if (box->m_is_non_reflowable)
				{
					write("!");
					place_brackets = true;
				}
				if (box->m_is_math)
				{
					if (box->m_math_operator & MO_TEST_LH_REQD)
					{
						write("<");
					}
					if (box->m_math_operator & MO_TEST_RH_REQD)
					{
						write(">");
					}
					write("M");
					place_brackets = true;
				}
				if (box->m_is_code)
				{
					write("C");
					place_brackets = true;
				}
				if (box->m_is_hyphenated)
				{
					write("-");
					place_brackets = true;
				}
				if (box->m_is_part_of_graphical_txt)
				{
					write("A");
					place_brackets = true;
				}
				if (box->m_is_part_of_quoted_txt)
				{
					if (!place_brackets
						|| !prev_box
						|| !prev_box->m_is_part_of_quoted_txt)
					{
						write("\"");
						place_brackets = true;
					}
				}
				else
				{
					if (place_brackets)
					{
						write("(");
					}
				}

				for (j = box->m_leading_whitespace_length; j > 0; j--)
				{
					write(" ");
				}
				write(box->m_text, box->m_word_length);
				for (j = box->m_trailing_whitespace_length; j > 0; j--)
				{
					write(" ");
				}

				if (box->m_line_count != 0)
				{
					for (int nl = box->m_line_count; nl > 0; nl--)
					{
						write("\n");
					}
				}

				if (box->m_is_part_of_quoted_txt)
				{
					if (!next_box || !next_box->m_is_part_of_quoted_txt)
					{
						write("\"");
						place_brackets = false;
					}
				}
				else
				{
					if (place_brackets)
					{
						write(")");
					}
				}
			}
		}
	}
	write("\n");
}



void cmt_reflow_ex::dump2output(paragraph_box *para, words_collection &words)
{
	UNC_ASSERT(para);

	char buf[256];

	while (para)
	{
		snprintf(buf, sizeof(buf), "\nPARA[%d-%d]:", para->m_first_box, para->m_last_box);
		write(buf);

		if (para->m_is_boxed_txt)
		{
			snprintf(buf, sizeof(buf), "BOXED:");
			write(buf);
		}
		if (para->m_is_graphics)
		{
			snprintf(buf, sizeof(buf), "GRX:");
			write(buf);
		}
		if (para->m_is_non_reflowable)
		{
			snprintf(buf, sizeof(buf), "NOREFLOW:");
			write(buf);
		}
		if (para->m_is_xhtml)
		{
			snprintf(buf, sizeof(buf), "XML:");
			write(buf);
		}
		if (para->m_is_math)
		{
			snprintf(buf, sizeof(buf), "MATH:");
			write(buf);
		}
		if (para->m_is_code)
		{
			snprintf(buf, sizeof(buf), "CODE:");
			write(buf);
		}
		if (para->m_is_path)
		{
			snprintf(buf, sizeof(buf), "PATH:");
			write(buf);
		}
		if (para->m_is_intermission)
		{
			snprintf(buf, sizeof(buf), "INTERMISSION:");
			write(buf);
		}
		if (para->m_is_bullet)
		{
			snprintf(buf, sizeof(buf), "BULLET(%d):", para->m_bulletlist_level);
			write(buf);
		}
		if (para->m_is_bulletlist)
		{
			snprintf(buf, sizeof(buf), "BULLET-LIST(%d):", para->m_bulletlist_level);
			write(buf);
		}
		if (para->m_is_doxygen_par)
		{
			snprintf(buf, sizeof(buf), "DOXY:");
			write(buf);
		}

		snprintf(buf, sizeof(buf), "INDENT[%d/%d]:", para->m_first_line_indent, para->m_hanging_indent);
		write(buf);

		if (para->m_indent_as_previous)
		{
			snprintf(buf, sizeof(buf), "LIKE_PREV:");
			write(buf);
		}
		if (para->m_continue_from_previous)
		{
			snprintf(buf, sizeof(buf), "CONT_FROM_PREV:");
			write(buf);
		}

		if (para->m_keep_with_prev != 0)
		{
			snprintf(buf, sizeof(buf), "<KEEP=%d:",
				para->m_keep_with_prev);
			write(buf);
		}
		if (para->m_keep_with_next != 0)
		{
			snprintf(buf, sizeof(buf), ">KEEP=%d:",
				para->m_keep_with_next);
			write(buf);
		}

		snprintf(buf, sizeof(buf), "WS(%d/%d):",
			para->m_leading_whitespace_length, para->m_trailing_whitespace_length);
		write(buf);

		if (para->m_min_required_linebreak_before != 0 || para->m_min_required_linebreak_after != 0)
		{
			snprintf(buf, sizeof(buf), "NL(%d/%d):", para->m_min_required_linebreak_before, para->m_min_required_linebreak_after);
			write(buf);
		}

#if 0
		/* only for 'boxed text' words: */
		char left_edge_char;   /* the box 'left edge' character used here. 0 when none was used. */
		int left_edge_thickness;
		int left_edge_trailing_whitespace;
		char right_edge_char;   /* the box 'right edge' character used here. 0 when none was used. */
		int right_edge_thickness;
		int right_edge_leading_whitespace;
#endif

		if (para->m_first_child)
		{
			snprintf(buf, sizeof(buf), "->CHILD:");
			write(buf);

			dump2output(para->m_first_child, words);
		}
		else
		{
			write("\n");
			dump2output(words, 1, para->m_first_box, para->m_last_box);
		}

		para = para->m_next_sibling;
	}
}





/*
dump text to output while escaping anything non-printable.

NOTE: this includes newlines as well!
*/
size_t cmt_reflow_ex::write_offender_text2output(const char *offender, size_t offender_len, size_t *marker_start, size_t *marker_end, bool do_print)
{
	size_t printed_len = 0;
	size_t i = 0;
	size_t mark_s = (marker_start ? *marker_start : 0);
	size_t mark_e = (marker_end ? *marker_end : 0);

	UNC_ASSERT(mark_s < offender_len);
	UNC_ASSERT(mark_e >= mark_s);
	UNC_ASSERT(mark_e <= offender_len);

	while (i < offender_len)
	{
		UNC_ASSERT(offender[i] != 0);

		if (mark_s == i && marker_start)
		{
			*marker_start = printed_len;
		}
		if (mark_e == i && marker_end)
		{
			*marker_end = printed_len;
		}

		/*
		print one 'character' per round so we can make sure the marker positions are spot-on at all times.
		*/
		if (unc_isprint(offender[i]))
		{
			if (do_print)
			{
				write2output(offender + i, 1);
			}
			printed_len += 1;

			i++;
			continue;
		}

		switch (offender[i])
		{
		case '\n':
			if (do_print)
			{
				write2output("\\n");
			}
			printed_len += 2;

			i++;
			continue;

		case '\r':
			if (do_print)
			{
				write2output("\\r");
			}
			printed_len += 2;

			i++;
			continue;

		case '\t':
			if (do_print)
			{
				write2output("[TAB]");
			}
			printed_len += 5;

			i++;
			continue;

		default:
			break;
		}

		// when we get here, it's UTF/hex-dumping time!
		char hexbuf[12];
		const unsigned int illegal_unicode_char = ~0u;
		unsigned int utfchar = illegal_unicode_char;
		unsigned int c;
		const unsigned char *b = (const unsigned char *)(offender + i);
		size_t charlen = 0;

		/*
		see if this is a UTF-8 or some such, i.e. decode non-ASCII as if UTF-8 and hexdump the remainder.

		http://en.wikipedia.org/wiki/UTF-8
		*/
		c = b[0];
		if ((c & 0x0080) == 0)
		{
			/* single byte UTF8 */
			utfchar = c;
			charlen = 1;
		}
		else if ((c & 0x00E0) == 0x00C0
			&& c >= 0x00C2 && c <= 0x00DF
			&& i + 1 < offender_len
			&& b[1] >= 0x0080 && b[1] <= 0x00BF)
		{
			/* two-byte UTF8 */
			utfchar = (((c & 0x001F) << 6) | (b[1] & 0x003F));
			charlen = 2;
		}
		else if ((c & 0x00F0) == 0x00E0
			&& c >= 0x00E0 && c <= 0x00EF
			&& i + 2 < offender_len
			&& b[1] >= 0x0080 && b[1] <= 0x00BF
			&& b[2] >= 0x0080 && b[2] <= 0x00BF)
		{
			/* three-byte UTF8 */
			utfchar = (((c & 0x000F) << 12) | ((b[1] & 0x003F) << 6) | (b[2] & 0x003F));
			charlen = 3;
		}
		else if ((c & 0x00F8) == 0x00F0
			&& c >= 0x00F0 && c <= 0x00F4
			&& i + 3 < offender_len
			&& b[1] >= 0x0080 && b[1] <= 0x00BF
			&& b[2] >= 0x0080 && b[2] <= 0x00BF
			&& b[3] >= 0x0080 && b[3] <= 0x00BF)
		{
			/* four-byte UTF8 */
			utfchar = (((c & 0x0007) << 18) | ((b[1] & 0x003F) << 12) | ((b[2] & 0x003F) << 6) | (b[3] & 0x003F));
			charlen = 4;
		}
		else
		{
			/* illegal UTF seq. */
			UNC_ASSERT(charlen == 0);
		}

		if (utfchar <= 0x0000 /* we do not accept U+0000 */
			|| utfchar > 0x10FFFF
			/*
			and we fail when the marker points right smack into the middle of a UTF8 char, as then
			it clearly isn't one.
			*/
			|| (mark_s > i && mark_s < i + charlen)
			|| (mark_e > i && mark_e < i + charlen)
			)
		{
			utfchar = illegal_unicode_char;
			charlen = 0;
		}

		if (utfchar != illegal_unicode_char)
		{
			sprintf(hexbuf, "U+%04X", utfchar);
			if (do_print)
			{
				write2output(hexbuf);
			}
			printed_len += strlen(hexbuf);

			UNC_ASSERT(charlen > 0);
			i += charlen;
			continue;
		}

		/*
		else:
		illegal unicode detect means nothing is unicode from here until we hit a ASCII-printable
		char. This is a design decision.
		*/
		UNC_ASSERT(!unc_isprint(b[0]));
		for ( ; i < offender_len && !unc_isprint(*b); i++)
		{
			if (mark_s == i && marker_start)
			{
				*marker_start = printed_len;
			}
			if (mark_e == i && marker_end)
			{
				*marker_end = printed_len;
			}

			sprintf(hexbuf, "\\x%02x", (unsigned)*b++);
			if (do_print)
			{
				write2output(hexbuf);
			}
			printed_len += strlen(hexbuf);
		}
		i--;
		UNC_ASSERT(!unc_isprint(offender[i]));
		UNC_ASSERT(unc_isprint(offender[i+1]));
	}

	UNC_ASSERT(i == offender_len);
	if (mark_e == i && marker_end)
	{
		*marker_end = printed_len;
	}

	return printed_len;
}


/*
write diagnostic to the output as a comment.
*/
void cmt_reflow_ex::pretty_print_diagnostic2output(const char *text, size_t text_len,
												const char *offender, size_t offender_len,
												const char *report_header,
												words_collection &words,
												paragraph_box *para)
{
	UNC_ASSERT(para);

	if (!report_header)
	{
		report_header = "**DIAG**";
	}

	size_t printed_len;
	write2output("    ");
	write2output(report_header);
	printed_len = 4 + strlen(report_header);

	if (offender && offender_len)
	{
		/*
		The offender is dumped to the output while special legibility formatting is applied. When this is done, the
		'lead in' and 'lead out' sizes are balanced, that is: these numbers can be considered a /ratio/ -- currently
		set at 1/3rd.
		*/
		const size_t dump_len = 40;
		const int dump_leadin = 1;
		const int dump_leadout = 3;

		UNC_ASSERT(text);
		UNC_ASSERT(offender_len <= text_len);
		UNC_ASSERT(offender >= text);
		UNC_ASSERT(offender < text + text_len);
		UNC_ASSERT(offender + offender_len >= text);
		UNC_ASSERT(offender + offender_len <= text + text_len);

		if (3 /* guestimate */ + printed_len + 4 + 2 + dump_len >= (size_t)max(78, m_line_wrap_column))
		{
			write2output(" at:");
			write2output("\n");
			printed_len = 0;
		}
		else
		{
			write2output(" @ \"");
			printed_len += 4;
		}

		/*
		dump the offender text to the output, while escaping newlines et al. Register the precise position of the
		offense while doing so and print '^' markers on the next line pointing out the bugger for y'all.

		But first calculate/guestimate the lead-in and lead-out, depending on offender size and the lead-in/lead-out ratio.
		*/
		size_t leadin = 0;
		size_t leadout = 0;
		size_t offender_print_len = write_offender_text2output(offender, offender_len, NULL, NULL, false);

		if (offender_print_len < dump_len)
		{
			size_t surplus = dump_len - offender_print_len;

			// assume the same encoding ratio for the 'context':
			surplus *= offender_len;
			surplus /= offender_print_len;

			leadin = surplus * dump_leadin;
			leadout = surplus * dump_leadout;
			leadin /= dump_leadin + dump_leadout;
			leadout /= dump_leadin + dump_leadout;

			if ((size_t)(offender - text) < leadin)
			{
				leadout += leadin - (size_t)(offender - text);
				leadin = (size_t)(offender - text);
			}
			if (text + text_len < offender + offender_len + leadout)
			{
				// leadin += ... - don't do this as we may fall below 'text' and this approximate anyway.
				UNC_ASSERT(text + text_len >= offender + offender_len);
				leadout = (size_t)(text + text_len - offender - offender_len);
			}
		}

		size_t start_column = cpd.column;
		size_t offender_start_pos = leadin;
		size_t offender_end_pos = leadin + offender_len; // EXclusive edge!
		offender_print_len = write_offender_text2output(offender - leadin, offender_len + leadin + leadout,
								&offender_start_pos, &offender_end_pos);
		if (printed_len > 0)
		{
			write2output("\"");
			//printed_len++;
		}
		write2output("\n");

		/*
		now advance to the offending start column again:
		*/
		start_column += offender_start_pos;
		size_t upper_loop_bound = start_column + 1;

		for ( ; upper_loop_bound > 0 && cpd.column < start_column; upper_loop_bound--)
		{
			write2output(" ");
		}
		// now we're at the start position of the offender.
		for (upper_loop_bound = offender_end_pos - offender_start_pos; upper_loop_bound > 0; upper_loop_bound--)
		{
			write2output("^");
		}
	}

	write2out_comment_end(0, 1);
	write2output("\n");
}


