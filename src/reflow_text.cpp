/**
 * @file reflowtxt.cpp
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
   @maintainer Ben Gardner
 * @license GPL v2+
 *
 * $Id: reflowtxt.cpp 1599 2009-08-08 19:58:52Z bengardner $
 */

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include "args.h"
#include "reflow_text.h"

#include "reflow_text_internal.h"






cmt_reflow::cmt_reflow()
   : m_first_pc(NULL), m_last_pc(NULL),
   m_left_global_output_column(0),
   m_brace_col(0),
   m_base_col(0),
   m_word_count(0),
   m_kw_subst(false),
   //m_xtra_indent(0),
   //m_cont_text(""),
   m_reflow_mode(0),
   m_is_cpp_comment(false),
   m_is_merged_comment(false),
   m_is_single_line_comment(false),
   m_extra_pre_star_indent(-1),
   m_extra_post_star_indent(-1),
   m_has_leading_and_trailing_nl(false),
	m_indent_cmt_with_tabs(false),
	m_cmt_reflow_graphics_threshold(0),
	m_cmt_reflow_box_threshold(0),
	m_cmt_reflow_box_markers(NULL),
	m_cmt_reflow_box(false),
	m_cmt_reflow_graphics_markers(NULL),
	m_cmt_reflow_no_line_reflow_markers_at_SOL(NULL),
	m_cmt_reflow_no_par_reflow_markers_at_SOL(NULL),
	m_cmt_reflow_no_cmt_reflow_markers_at_SOL(NULL),
	m_cmt_reflow_bullets(NULL),
	m_cmt_reflow_bullet_terminators(NULL),
	m_cmt_reflow_SOL_markers(NULL),
	m_string_escape_char(0),
	m_comment_is_part_of_preproc_macro(false),
	m_cmt_reflow_overshoot(0),
	m_cmt_reflow_minimum_words_per_line(0),
	m_cmt_reflow_intermission_indent_threshold(0),
	m_xml_text_has_stray_lt_gt(-1), m_xml_offender(0),
   m_comment(NULL), m_comment_len(0), m_comment_size(0),
   m_orig_startcolumn(1),
   m_lead_cnt(0),
   m_lead_marker(NULL),
   m_is_doxygen_comment(false),
   m_is_backreferencing_doxygen_comment(false),
   m_doxygen_marker(NULL),
	m_no_reflow_marker_start(NULL),
	m_no_reflow_marker_end(NULL),
	m_line_wrap_column(0),
	m_tab_width(8),
	m_defd_lead_markers(NULL)
{
   set_cmt_config_params();
}

cmt_reflow::~cmt_reflow()
{
   if (m_comment)
	   free((void *)m_comment);
   if (m_doxygen_marker)
	   free((void *)m_doxygen_marker);
   if (m_lead_marker)
	   free((void *)m_lead_marker);

   free((void *)m_no_reflow_marker_start);
	free((void *)m_no_reflow_marker_end);
	free((void *)m_cmt_reflow_box_markers);
	free((void *)m_cmt_reflow_graphics_markers);
	free((void *)m_cmt_reflow_no_line_reflow_markers_at_SOL);
	free((void *)m_cmt_reflow_no_par_reflow_markers_at_SOL);
	free((void *)m_cmt_reflow_no_cmt_reflow_markers_at_SOL);
	free((void *)m_cmt_reflow_bullets);
	free((void *)m_cmt_reflow_bullet_terminators);
	free((void *)m_cmt_reflow_SOL_markers);

	free((void *)m_defd_lead_markers);
}





int cmt_reflow::get_global_block_left_column(void)
{
	UNC_ASSERT(m_first_pc);
	return cpd.column; // m_first_pc->column;
}


bool cmt_reflow::comment_is_part_of_preproc_macro(void) const
{
	return m_comment_is_part_of_preproc_macro;
}


/*
Estimate the width consumed by this bit of text.

Take into account any keep-with-prev/next and other reflow limitations, such as localized 'non-reflow' series of boxes;
this box is assumed to be the first one in such a series.
*/
int cmt_reflow::estimate_box_print_width(paragraph_box *para, words_collection &words, int box_idx, int *last_box_for_this_bit)
{
	reflow_box *box;

	box = &words[box_idx];

	int print_len = box->m_word_length;
	if (box->m_is_part_of_boxed_txt
		// && is_first_line_of_para
		// && box->m_is_first_on_line
		)
	{
		/*
		TODO: properly handle semi-boxed and fully boxed comments by rendering them without
		      the top/bottom/left/right borders and only once done, wrap those borders around
			  the paragraph / series-of-paragraphs.
		*/
		print_len += box->m_left_edge_thickness + box->m_right_edge_thickness;
	}

	bool stuck_together;
	do
	{
		/*
		signal flag: set when multiple boxes were merged to stick on a single line for whatever reason
		*/
		stuck_together = false;

		if ((box->m_is_non_reflowable || para->m_is_non_reflowable)
			&& box_idx + 1 <= para->m_last_box)
		{
			/*
			if this box has been marked as 'non-reflowable', consume subsequent boxes with the same flag also!
			*/
			UNC_ASSERT(box_idx + 1 < (int)words.count());
			for (box_idx++; box_idx <= para->m_last_box; box_idx++)
			{
				reflow_box *next = &words[box_idx];

				if (next->m_do_not_print)
					continue;

				if (next->m_line_count > 0)
				{
					break;
				}
				if (!next->m_is_non_reflowable && !para->m_is_non_reflowable)
				{
					break;
				}
				print_len += box->m_trailing_whitespace_length
						+ next->m_leading_whitespace_length
						+ next->m_word_length;
				box = next;
				stuck_together = true;
			}
			box_idx--;
		}
		else if ((box->m_is_math || para->m_is_math)
			&& box_idx + 1 <= para->m_last_box)
		{
			/*
			if box is math, keep it together with the other 'math' bits...
			*/
			UNC_ASSERT(box_idx + 1 < (int)words.count());
			for (box_idx++; box_idx <= para->m_last_box; box_idx++)
			{
				reflow_box *next = &words[box_idx];

				if (next->m_line_count > 0)
				{
					break;
				}
				if (!next->m_is_math && !para->m_is_math)
				{
					break;
				}
				print_len += box->m_trailing_whitespace_length
					+ next->m_leading_whitespace_length
					+ next->m_word_length;
				box = next;
				stuck_together = true;
			}
			box_idx--;
		}
		else if ((box->m_is_code || para->m_is_code)
			&& box_idx + 1 <= para->m_last_box)
		{
			/*
			if box is code, keep it together with the other 'code' bits...
			*/
			UNC_ASSERT(box_idx + 1 < (int)words.count());
			for (box_idx++; box_idx <= para->m_last_box; box_idx++)
			{
				reflow_box *next = &words[box_idx];

				if (next->m_do_not_print)
					continue;

				if (next->m_line_count > 0)
				{
					break;
				}
				if (!next->m_is_code && !para->m_is_code)
				{
					break;
				}
				print_len += box->m_trailing_whitespace_length
					+ next->m_leading_whitespace_length
					+ next->m_word_length;
				box = next;
				stuck_together = true;
			}
			box_idx--;
		}


		/*
		if box is punctuation, which comes with ZERO leading whitespace, e.g. a sentence-terminating dot,
		keep it together with the previous word.
		*/
		if (box->m_trailing_whitespace_length == 0
			&& box_idx + 1 <= para->m_last_box)
		{
			UNC_ASSERT(box_idx + 1 < (int)words.count());

			for (box_idx++; box_idx <= para->m_last_box; box_idx++)
			{
				reflow_box *next = &words[box_idx];

				if (next->m_do_not_print)
					continue;

				if (next->m_is_punctuation
					&& next->m_leading_whitespace_length == 0
					&& next->m_line_count == 0
					)
				{
					print_len += box->m_trailing_whitespace_length
							+ next->m_leading_whitespace_length
							+ next->m_word_length;
					box = next;
					// stuck_together = true;

					if (next->m_trailing_whitespace_length > 0)
					{
						box_idx++;
						break;
					}
				}
				/*
				else: no 'keep-with-next'-ish punctuation: end this.
				*/
				break;
			}
			box_idx--;
		}
	} while (stuck_together);

	if (last_box_for_this_bit)
	{
		*last_box_for_this_bit = box_idx;
	}
	return print_len;
}



paragraph_box *cmt_reflow::get_last_sibling(paragraph_box *para)
{
	if (!para)
		return NULL;

	while (para->m_next_sibling)
	{
		para = para->m_next_sibling;
	}
	return para;
}




/**
Calculate the render width occupied by the given box sequence and leading deferred whitepace.

Also calculates the box index and render width for the last 'preferred break' position within
that box sequence and the first subsequent 'preferred break' position and render width BEYOND
the given box range, yet within the current paragraph.

The latter two datums can be employed to decide intelligently where the next line break
should occur.

When @a last_box_idx == 0, this routine assumes it needs to render until the first 'preferred break'
position (and it will still also determine the next preferred break position & width after that).
*/
void cmt_reflow::estimate_render_width(paragraph_box *para, words_collection &words, int start_box_idx, int last_box_idx, int deferred_whitespace, render_estimates_t &info)
{
	int i;
	int render_width = 0;
	bool is_first = true;
	bool keep_with_next = false;
	bool until_first_preferred_break = (last_box_idx == 0);

	info.next_preferred_break_box_idx = 0;
	info.previous_preferred_break_box_idx = 0;
	info.next_preferred_break_width = 0;
	info.previous_preferred_break_width = 0;

	if (until_first_preferred_break)
	{
		last_box_idx = para->m_last_box;
	}

	UNC_ASSERT(para->m_first_box <= start_box_idx);
	UNC_ASSERT(para->m_last_box >= last_box_idx);
	for (i = start_box_idx; i <= last_box_idx; i++)
	{
		UNC_ASSERT(i >= 0);
		UNC_ASSERT(i < (int)words.count());

		reflow_box *box = &words[i];

		if (box->m_do_not_print)
			continue;

		if (is_first)
		{
			is_first = false;
		}
		else
		{
			if (box->m_line_count > 0)
			{
				deferred_whitespace = 1;
			}
			else if (!box->m_is_first_on_line)
			{
				deferred_whitespace += box->m_leading_whitespace_length;
			}
			else
			{
				deferred_whitespace = 1;
			}

			if ((deferred_whitespace > 0 && !keep_with_next)
				|| box->m_line_count > 0)
			{
				info.previous_preferred_break_box_idx = i;
				info.previous_preferred_break_width = render_width;

				if (until_first_preferred_break)
				{
					is_first = true;
					break;
				}
			}
		}

		render_width += box->m_word_length + deferred_whitespace;
		deferred_whitespace = box->m_trailing_whitespace_length;

		keep_with_next = !box->box_is_a_usual_piece_of_text();
	}

	info.render_width = render_width;

	/*
	render a bit PAST the indicated end to find out what the width is for the next
	'preferred break'; this is used in overflow-allowing situations: if the next
	break is within the overflow bound and 'better' (i.e. significantly closer
	to the 'line_width' mark) than the 'previous preferred break', we might want
	to use that break point instead!

	As we ASSUME that paragraphs are always line spanning (i.e. no partial line para's
	here), we can try to render until the end of the paragraph.

	N.B.: is_first==true to prevent counting the whitespace at the
	      break/edge twice.
	*/
	for ( ; i <= para->m_last_box; i++)
	{
		UNC_ASSERT(i >= 0);
		UNC_ASSERT(i < (int)words.count());

		reflow_box *box = &words[i];

		if (box->m_do_not_print)
			continue;

		if (is_first)
		{
			is_first = false;
		}
		else
		{
			if (box->m_line_count > 0)
			{
				deferred_whitespace = 1;
			}
			else if (!box->m_is_first_on_line)
			{
				deferred_whitespace += box->m_leading_whitespace_length;
			}
			else
			{
				deferred_whitespace = 1;
			}

			if ((deferred_whitespace > 0 && !keep_with_next)
				|| box->m_line_count > 0)
			{
				info.next_preferred_break_box_idx = i;
				info.next_preferred_break_width = render_width;
				/* found a break; we're done now! */
				break;
			}
		}

		render_width += box->m_word_length + deferred_whitespace;
		deferred_whitespace = box->m_trailing_whitespace_length;

		keep_with_next = !box->box_is_a_usual_piece_of_text();
	}
}






/**
Determine the index of the box which is the last word of the series of 'widow/orphan' words for this paragraph.

Also estimate the 'rendered' width consumed by the widows/orphans.
*/
void cmt_reflow::calculate_widow_and_orphan_aspects(paragraph_box *para, words_collection &words, int line_width, window_orphan_info_t &info)
{
	int i;
	int orphan_count = (!para->para_is_a_usual_piece_of_text() ? 0 : cpd.settings[UO_cmt_reflow_orphans].n);

	info.orphan_last_box_idx = 0;
	info.widow_first_box_idx = INT_MAX;
	info.orphan_render_width = 0;
	info.widow_render_width = 0;

	/*
	scan through the paragraph to detect the box index which represents
	the last word of the orphans [at the start of the paragraph] and
	first box of the widows [at the end of the paragraph].
	*/
	for (i = para->m_first_box; i <= para->m_last_box && orphan_count > 0; i++)
	{
		UNC_ASSERT(i >= 0);
		UNC_ASSERT(i < (int)words.count());

		reflow_box *box = &words[i];

		if (box->m_do_not_print)
			continue;

		/*
		There's no orphan control when math, code or other specials are situated near the
		start of the paragraph!
		*/
		if (!box->box_is_a_usual_piece_of_text())
		{
			/* disable orphan treatment */
			UNC_ASSERT(orphan_count > 0);
			break;
		}
		/*
		skip punctuation and don't count it as a 'word' for the widow/orphan check;
		since punctuation shouldn't end up at the start-of-line if it isn't already
		there, we make sure the widow/orpahn check ensures it's contained within
		the widow/orpahn box series.
		*/
		if (box->m_is_punctuation
			&& !box->m_is_first_on_line)
		{
			continue;
		}

		info.orphan_last_box_idx = i;

		/* only count 'real words' here! */
		if (!box->m_is_punctuation && !box->m_is_quote)
		{
			orphan_count--;
		}
	}

	/*
	There's no orphan control when the number of words in the paragraph doesn't surpass the orphan condition count.
	*/
	if (orphan_count != 0)
	{
		/* disable orphan treatment */
		info.orphan_last_box_idx = 0;
	}


	/*
	scan backwards through the paragraph to detect the box index which represents
	the first word of the widows [at the end of the paragraph].
	*/
	int widow_count = (!para->para_is_a_usual_piece_of_text() ? 0 : cpd.settings[UO_cmt_reflow_widows].n);
	for (i = para->m_last_box; i >= para->m_first_box && widow_count > 0; i--)
	{
		UNC_ASSERT(i >= 0);
		UNC_ASSERT(i < (int)words.count());

		reflow_box *box = &words[i];

		if (box->m_do_not_print)
			continue;

		/*
		There's no widow control when math, code or other specials are situated near the
		end of the paragraph!
		*/
		if (!box->box_is_a_usual_piece_of_text())
		{
			/* disable widow treatment */
			UNC_ASSERT(widow_count != 0);
			break;
		}
		/*
		skip punctuation and don't count it as a 'word' for the widow/orphan check;
		since punctuation shouldn't end up at the start-of-line if it isn't already
		there, we make sure the widow/orpahn check ensures it's contained within
		the widow/orpahn box series.
		*/
		if (box->m_is_punctuation
			&& !box->m_is_first_on_line)
		{
			continue;
		}

		info.widow_first_box_idx = i;

		/* only count 'real words' here! */
		if (!box->m_is_punctuation && !box->m_is_quote)
		{
			widow_count--;
		}
	}

	/*
	There's no widow control when the number of words in the paragraph doesn't surpass the widow condition count.
	*/
	if (widow_count != 0)
	{
		/* disable widow treatment */
		info.widow_first_box_idx = INT_MAX;
	}

	/*
	There's no use for widows nor orphans if there isn't at least one full width line between them:
	*/
	int orphan_edge = max(para->m_first_box, info.orphan_last_box_idx);
	int widow_edge = min(para->m_last_box, info.widow_first_box_idx);

	if (widow_edge <= orphan_edge)
	{
		/* disable widow/orphan treatment */
		info.widow_first_box_idx = INT_MAX;
		info.orphan_last_box_idx = 0;
	}
	else
	{
		render_estimates_t render_info;

		estimate_render_width(para, words, orphan_edge, widow_edge, 0 /* para->m_hanging_indent */, render_info);
		if (render_info.render_width < line_width - para->m_hanging_indent)
		{
			/*
			not a full line between orphaned and widowed line: forget about it all!

			disable widow/orphan treatment */
			info.widow_first_box_idx = INT_MAX;
			info.orphan_last_box_idx = 0;
		}
	}

	/*
	calculate the rendered width estimate anyhow, as we use it ourselves to check whether the
	widow/orphan condition makes any sense at all, e.g. when the widows/orphans each are longer then the allowed
	line width, upholding these restrictions is certainly ludicrous.
	*/
	if (para->m_first_box <= info.orphan_last_box_idx)
	{
		render_estimates_t render_info;

		estimate_render_width(para, words, para->m_first_box, info.orphan_last_box_idx, 0 /* para->m_first_line_indent */, render_info);
		if (render_info.render_width > line_width - para->m_first_line_indent)
		{
			/* disable orphan treatment */
			info.orphan_last_box_idx = 0;
		}
		else
		{
			info.orphan_render_width = render_info.render_width;
		}
	}

	if (info.widow_first_box_idx <= para->m_last_box)
	{
		render_estimates_t render_info;

		estimate_render_width(para, words, info.widow_first_box_idx, para->m_last_box, 0 /* para->m_hanging_indent */, render_info);
		if (render_info.render_width > line_width - para->m_hanging_indent)
		{
			/* disable widow treatment */
			info.widow_first_box_idx = INT_MAX;
		}
		else
		{
			info.widow_render_width = render_info.render_width;
		}
	}
}







void cmt_reflow::push_tag_piece_and_possible_newlines(words_collection &words, const char *&s, int &word_idx, reflow_box *&current_word, const char *&last_nl)
{
	// UNC_ASSERT(unc_isspace(*s) || in_set("/>", *s));

	const char *text;

	/* push the tag box; maybe push a newline box as well... */
	UNC_ASSERT(current_word->m_text);
	text = s;
	current_word->m_word_length = (int)(text - current_word->m_text);
	current_word->m_orig_hpos = (int)(current_word->m_text - last_nl);

	int spc = strleadlen(text, ' ');
	current_word->m_trailing_whitespace_length = spc;
	text += spc;
	s = text;

	current_word = words.prep_next(word_idx);
	current_word->m_text = text;
	current_word->m_orig_hpos = (int)(text - last_nl);

	int newline_count = 0;
	while (unc_isspace(*s))
	{
		if (*s == '\n')
		{
			newline_count++;
			last_nl = s;
		}
		s++;
	}
	text = s;

	if (newline_count)
	{
		current_word->m_word_length = 0;
		current_word->m_line_count = newline_count;
#if 0
		if(current_word->m_line_count > 1)
			UNC_ASSERT(0);
#endif
		current_word = words.prep_next(word_idx);
		current_word->m_is_first_on_line = true;

		text = last_nl + 1;
	}
	current_word->m_leading_whitespace_length = (int)(s - text);
	current_word->m_text = s;
	current_word->m_orig_hpos = (int)(s - last_nl);
}


void cmt_reflow::count_graphics_nonreflow_and_printable_chars(const char *text, int len, int *graph_countref, int *nonreflow_countref, int *print_countref)
{
	int graph_count = 0;
	int nonreflow_count = 0;
	int print_count = 0;

	for (int i = 0; i < len; i++)
	{
		graph_count += in_set(m_cmt_reflow_graphics_markers, text[i]);
		nonreflow_count += in_set(m_cmt_reflow_box_markers, text[i]);
		print_count += !!unc_isprint(text[i]);
	}

	if (graph_countref)
		*graph_countref = graph_count;
	if (nonreflow_countref)
		*nonreflow_countref = nonreflow_count;
	if (print_countref)
		*print_countref = print_count;
}




/*
convert the specified tag set as a configuration string to arrays of tags, ready to be used.
*/
void cmt_reflow::set_no_reflow_markers(const char *start_tags, const char *end_tags)
{
	UNC_ASSERT(start_tags);
	UNC_ASSERT(end_tags);

	/*
	we are playing nasty here so we can free() the allocated memory in one go:

	the array-of-strings and the text itself are allocated in one go; we can provide
	accurate worst-case estimates for the array size so we're good in that regard:

	We estimate the number of items by counting the number of spaces in the input.
	Plus 2 for the first entry and the NULL sentinel.
	*/
	int arrsize;
	char *s;

	arrsize = 2 + strccnt(start_tags, ' ');
	m_no_reflow_marker_start = (const char **)malloc(arrsize * sizeof(char *) + strlen(start_tags) + 1);
	s = (char *)(m_no_reflow_marker_start + arrsize);
	m_no_reflow_marker_start[Args::SplitLine(strcpy(s, start_tags), m_no_reflow_marker_start, arrsize - 1)] = NULL;

	arrsize = 2 + strccnt(end_tags, ' ');
	m_no_reflow_marker_end = (const char **)malloc(arrsize * sizeof(char *) + strlen(end_tags) + 1);
	s = (char *)(m_no_reflow_marker_end + arrsize);
	m_no_reflow_marker_end[Args::SplitLine(strcpy(s, end_tags), m_no_reflow_marker_end, arrsize - 1)] = NULL;
}






/*
set up the configuration parameters for this particular comment (block of text).

TODO: the interface is ugly as it assumes the existence of global 'cpd'; this (and several other
      chunks of code) make this comment/text reflow engine highly dependent on uncrustify,
	  which is not all that desirable, both from an interface and re-use point of view...
*/
void cmt_reflow::set_cmt_config_params(void)
{
	m_tab_width = cpd.settings[UO_input_tab_size].n;

	m_defd_lead_markers = strdupdflt(cpd.settings[UO_cmt_lead_markers].str, "*#\\|+");

	UNC_ASSERT(cpd.settings[UO_cmt_reflow_no_reflow_start_tag].str);
	UNC_ASSERT(cpd.settings[UO_cmt_reflow_no_reflow_end_tag].str);
	set_no_reflow_markers(cpd.settings[UO_cmt_reflow_no_reflow_start_tag].str,
						cpd.settings[UO_cmt_reflow_no_reflow_end_tag].str);

	m_indent_cmt_with_tabs = cpd.settings[UO_indent_cmt_with_tabs].b;
	m_cmt_reflow_graphics_threshold = cpd.settings[UO_cmt_reflow_graphics_threshold].n;
	m_cmt_reflow_box_threshold = cpd.settings[UO_cmt_reflow_box_threshold].n;
	m_cmt_reflow_box_markers = strdupdflt(cpd.settings[UO_cmt_reflow_box_markers].str, "'\"*#+',`.|-=_!/\\");
	m_cmt_reflow_box = cpd.settings[UO_cmt_reflow_box].b;
	m_cmt_reflow_graphics_markers = strdupdflt(cpd.settings[UO_cmt_reflow_graphics_markers].str, "+-_!|/,.=");
	m_cmt_reflow_no_line_reflow_markers_at_SOL = strdupdflt(cpd.settings[UO_cmt_reflow_no_line_reflow_markers_at_SOL].str, "!");
	m_cmt_reflow_no_par_reflow_markers_at_SOL = strdupdflt(cpd.settings[UO_cmt_reflow_no_par_reflow_markers_at_SOL].str, "`");
	m_cmt_reflow_no_cmt_reflow_markers_at_SOL = strdupdflt(cpd.settings[UO_cmt_reflow_no_cmt_reflow_markers_at_SOL].str, "'");
	m_cmt_reflow_bullets = strdupdflt(cpd.settings[UO_cmt_reflow_bullets].str, "*#-+;0");
	m_cmt_reflow_bullet_terminators = strdupdflt(cpd.settings[UO_cmt_reflow_bullet_terminators].str, " )].:");
	m_cmt_reflow_SOL_markers = strdupdflt(cpd.settings[UO_cmt_reflow_SOL_markers].str, "A\\@");

	m_string_escape_char = cpd.settings[UO_string_escape_char].n;
	m_cmt_reflow_overshoot = cpd.settings[UO_cmt_reflow_overshoot].n;
	m_cmt_reflow_minimum_words_per_line = cpd.settings[UO_cmt_reflow_minimum_words_per_line].n;
	m_cmt_reflow_intermission_indent_threshold = cpd.settings[UO_cmt_reflow_intermission_indent_threshold].n;

	m_comment_is_part_of_preproc_macro = ((cpd.in_preproc != CT_NONE) && (cpd.in_preproc != CT_PP_DEFINE));
}


/*
Set the parameters which depend on the initial chunk of text being known.
*/
void cmt_reflow::set_deferred_cmt_config_params_phase1(void)
{
	chunk_t *pc = m_first_pc;

#if defined(_MSC_VER)
#pragma message(__FILE__ "(" STRING(__LINE__) ") : TODO: check why this assert and assignment are here and when the assert fires.")
#endif
	//UNC_ASSERT(m_comment_is_part_of_preproc_macro == ((pc->flags & PCF_IN_PREPROC) != 0));
	m_comment_is_part_of_preproc_macro = ((pc->flags & PCF_IN_PREPROC) != 0);

	//m_left_global_output_column = cpd.column;

      if (pc->type == CT_COMMENT_MULTI)
      {
         if (!cpd.settings[UO_cmt_indent_multi].b)
         {
			 m_reflow_mode = 1;
			 // output_comment_multi_simple(pc);
         }
		 m_is_cpp_comment = false;
      }
      else if (pc->type == CT_COMMENT_CPP)
      {
		 m_is_cpp_comment = true;
      }
      else if (pc->type == CT_COMMENT)
      {
		 m_is_cpp_comment = false;
      }


   m_brace_col = 1 + (pc->brace_level * cpd.settings[UO_output_tab_size].n);

int cmt_col;
int col_diff;

   if (chunk_is_newline(chunk_get_prev(pc)))
   {
      /* The comment should be indented correctly */
      cmt_col  = pc->column;
      col_diff = pc->orig_col - pc->column;
   }
   else
   {
      /* The comment starts after something else */
      cmt_col  = pc->orig_col;
      col_diff = 0;
   }

   UNC_ASSERT(cmt_col >= 0);
   m_left_global_output_column = pc->column;
   //m_brace_col   = pc->column_indent;
   m_base_col    = pc->column_indent;
   m_orig_startcolumn = pc->orig_col;

   if ((pc->parent_type == CT_COMMENT_START) ||
       (pc->parent_type == CT_COMMENT_WHOLE))
   {
      if (!cpd.settings[UO_indent_col1_comment].b &&
          (pc->orig_col == 1) &&
          !(pc->flags & PCF_INSERTED))
      {
         m_left_global_output_column = 1;
         m_base_col  = 1;
         m_brace_col = 1;
      }
   }
   else if (pc->parent_type == CT_COMMENT_END)
   {
      /* Make sure we have at least one space past the last token */
      chunk_t *prev = chunk_get_prev(pc);
      if (prev != NULL)
      {
         int col_min = prev->column + prev->len + 1;
         if (m_left_global_output_column < col_min)
         {
            m_left_global_output_column = col_min;
         }
      }
   }

   /* tab aligning code */
   if (m_indent_cmt_with_tabs /* cpd.settings[UO_indent_cmt_with_tabs].b */ &&
       ((pc->parent_type == CT_COMMENT_END) ||
        (pc->parent_type == CT_COMMENT_WHOLE)))
   {
      m_left_global_output_column = next_tab_column(m_left_global_output_column - 1);
      //LOG_FMT(LSYS, "%s: line %d, orig:%d new:%d\n",
      //        __func__, pc->orig_line, pc->column, m_column);
      pc->column = m_left_global_output_column;
	  m_base_col = m_left_global_output_column;
   }

   //LOG_FMT(LSYS, "%s: -- brace=%d base=%d outputcol=%d\n", __func__, m_brace_col, m_base_col, m_left_global_output_column);

   /* Bump out to the column */
   //cmt_output_indent(m_brace_col, m_base_col, m_column);

   m_kw_subst = ((pc->flags & PCF_INSERTED) != 0);






   /*
	defer CORRECTING setting 'line_width' until after we've collected and cleaned up the text to reflow:

	this parameter (and a few others) are only needed by the time we invoke render() after all.
	*/

	bool is_inline_comment = chunk_is_inline_comment(m_first_pc);

	int lw = (!is_inline_comment
		? cpd.settings[UO_cmt_width].n
		: (cpd.settings[UO_cmt_inline_width].n < 0
			? cpd.settings[UO_cmt_width].n
			: cpd.settings[UO_cmt_inline_width].n));

	if (lw < 0)
	{
		/*
		'autodetect' the line width by scanning the comment text.

		WARNING: this ASSUMES both that the 'comment' has been filled before this call
		         AND that the comment text is written in such a way that it starts at
				 column==1 i.e. screen's left edge.
			*/

		// DEFERRED!
		lw = -1;
	}
	m_line_wrap_column = lw;
}


/*
Set the parameters which depend on the entire input text being known.
*/
void cmt_reflow::set_deferred_cmt_config_params_phase2(void)
{
	m_is_single_line_comment = !strchr(m_comment, '\n');

	int lw = m_line_wrap_column;

	UNC_ASSERT(m_first_pc);
	if (lw < 0)
	{
		/*
		'autodetect' the line width by scanning the comment text.

		WARNING: this ASSUMES both that the 'comment' has been filled before this call
		         AND that the comment text is written in such a way that it starts at
				 column==1 i.e. screen's left edge.
			*/
		const char *text = m_comment;

		while (*text)
		{
			const char *eol = strchrnn(text, '\n');
			int spc = strtaillen(text, eol, ' ');
			int width = (int)(eol - spc - text);

			if (width > lw)
			{
				lw = width;
			}
			text = eol + strleadlen(eol, '\n');
		}

		/*
		account for the global indentation of the comment block:
		*/
		//lw += m_first_pc->column - m_first_pc->orig_col;

		// convert width to column value:
		lw += m_first_pc->column;
	}

	UNC_ASSERT(lw > m_left_global_output_column);
	UNC_ASSERT(m_left_global_output_column > 0);
	UNC_ASSERT(m_left_global_output_column >= m_first_pc->column);
	const int heuristic_minimum_width = 16; // minimum allowed width
	if (lw <= m_left_global_output_column + heuristic_minimum_width)
	{
		lw = m_left_global_output_column + heuristic_minimum_width;
	}
	const int heuristic_minimum_column = 78;
	if (lw <= heuristic_minimum_column)
	{
		lw = heuristic_minimum_column;
	}

	m_line_wrap_column = lw;




	if (!m_lead_marker)
	{
		m_lead_marker = strdup("");
	}

	if (m_extra_pre_star_indent < 0)
	{
		m_extra_pre_star_indent = (m_is_cpp_comment
			? 0
			: (*m_lead_marker)
				? 1
				: 0);
	}
	if (m_extra_post_star_indent < 0)
	{
		m_extra_post_star_indent = (m_is_cpp_comment
			? 1
			: (*m_lead_marker)
				? 1
				: 0);
	}

	if (m_is_cpp_comment)
	{
		m_reflow_mode = cpd.settings[UO_cmt_reflow_mode_cpp].n;
		UNC_ASSERT(m_is_cpp_comment == true);
		UNC_ASSERT(!*m_lead_marker);

		if (cpd.settings[UO_cmt_cpp_to_c].b)
		{
			/* We are going to convert the CPP comments to C comments */
			switch (cpd.settings[UO_cmt_star_cont].t)
			{
			case TB_TRUE:
				free((void *)m_lead_marker);
				m_lead_marker = strdup("*");
				m_extra_pre_star_indent = cpd.settings[UO_cmt_sp_before_star_cont].n;
				m_extra_post_star_indent = cpd.settings[UO_cmt_sp_after_star_cont].n;
				break;

			case TB_FALSE:
				*m_lead_marker = 0;
				m_extra_pre_star_indent = 0;
				m_extra_post_star_indent = 0;
				break;

			case TB_NOCHANGE:
				break;
			}
			m_is_cpp_comment = false;
		}
		else
		{
			/* Abuse^H^H^H^H^HRe-use the settings for the CPP comments: guestimate some sensible conversion here */
			switch (cpd.settings[UO_cmt_star_cont].t)
			{
			case TB_TRUE:
				*m_lead_marker = 0;
				m_extra_pre_star_indent = 0;
				m_extra_post_star_indent = cpd.settings[UO_cmt_sp_after_star_cont].n;
				break;

			case TB_FALSE:
				*m_lead_marker = 0;
				m_extra_pre_star_indent = 0;
				m_extra_post_star_indent = 0;
				break;

			case TB_NOCHANGE:
				break;
			}
		}
	}
	else if (m_is_single_line_comment)
	{
		m_reflow_mode = cpd.settings[UO_cmt_reflow_mode].n;
		UNC_ASSERT(m_is_cpp_comment == false);
		switch (cpd.settings[UO_cmt_star_cont].t)
		{
		case TB_TRUE:
			free((void *)m_lead_marker);
			m_lead_marker = strdup("*");
			m_extra_pre_star_indent = cpd.settings[UO_cmt_sp_before_star_cont].n;
			m_extra_post_star_indent = cpd.settings[UO_cmt_sp_after_star_cont].n;
			break;

		case TB_FALSE:
			*m_lead_marker = 0;
			m_extra_pre_star_indent = 0;
			m_extra_post_star_indent = 0;
			break;

		case TB_NOCHANGE:
			break;
		}

		//calculate_comment_body_indent(m_comment, (int)m_comment_len);
	}
	else
	{
		/* multiline comment: */
		m_reflow_mode = cpd.settings[UO_cmt_reflow_mode].n;
		UNC_ASSERT(m_is_cpp_comment == false);
		tristate_t tb = cpd.settings[UO_cmt_star_cont].t;
		if (!cpd.settings[UO_cmt_indent_multi].b)
		{
			tb = TB_NOCHANGE;
		}
		switch (tb)
		{
		case TB_TRUE:
			free((void *)m_lead_marker);
			m_lead_marker = strdup("*");
			m_extra_pre_star_indent = cpd.settings[UO_cmt_sp_before_star_cont].n;
			m_extra_post_star_indent = cpd.settings[UO_cmt_sp_after_star_cont].n;
			break;

		case TB_FALSE:
			*m_lead_marker = 0;
			m_extra_pre_star_indent = 0;
			m_extra_post_star_indent = 0;
			break;

		case TB_NOCHANGE:
			break;
		}

		//calculate_comment_body_indent(m_comment, (int)m_comment_len);
	}
}


/*
Set the parameters which depend on the text being chopped into words and the initial parse having finished.
*/
void cmt_reflow::set_deferred_cmt_config_params_phase3(void)
{
}








/**
This one simply chops the text up in reflow boxes, one for every 'word'.

The only particular thing here is the detection of non-reflowable *lines*, that is: detection of
non-reflowable boxed texts. This is done here as this is the last time in the comment reflow process
that we look at text and lines, instead of text/reflow boxes, and some non-reflow heuristics are
line oriented, so this is the 'optimal' place for those to end up.

Alas, it complicates this otherwise simple function a tad, but you can't have it all.

All other box-based paragraph-extraction/reflow/non-reflow/layout logic is done in the subsequent stages (functions),
this is just the beginning. Chop chop. :-)
*/
void cmt_reflow::chop_text_into_reflow_boxes(words_collection &words)
{
	const char *text = m_comment;
	UNC_ASSERT(m_comment);
	UNC_ASSERT(m_comment[m_comment_len] == 0);

	bool in_probable_boxed_cmt = false;
	bool in_probable_ascii_art_cmt = false;

	UNC_ASSERT(m_xml_text_has_stray_lt_gt == -1); // 0: surely NO; +1/+2: surely YES; -1: don't know yet.
	UNC_ASSERT(m_xml_offender == NULL);

	UNC_ASSERT(words.count() == 0);
	int word_idx = -1;
	reflow_box *current_word = words.prep_next(word_idx);
	UNC_ASSERT(word_idx == 0);
	int line_count = 0;
	int newline_count = 0;
	char doxygen_tag_marker = 0; // the identified doxygen/javadoc marker char; see comments further below
	int nrfl_start_marker = -1;

	enum
	{
		REGULAR_PARSE_MODE,
		IN_NONREFLOW_LINE,
		IN_NONREFLOW_PARAGRAPH,
		IN_NONREFLOW_COMMENT,
		IN_NONREFLOW_SECTION,
	} parse_mode = (m_reflow_mode != 2 ? IN_NONREFLOW_COMMENT : REGULAR_PARSE_MODE);

	/*
	push a single non-printing box to ensure the box set is always at least count >= 1
	*/
	current_word->m_line_count = 0;
	current_word->m_text = text;
	current_word->m_do_not_print = true;
	current_word->m_is_first_on_line = true;
	current_word->m_orig_hpos = 0;
	current_word->m_word_length = 0;
	current_word = words.prep_next(word_idx);

	while (*text)
	{
		/*
		count the number of consecutive newlines.

		Hint: as we already have the input text stripped of all trailing whitespace, we can do this
		in a very simple way.
		*/
		const char *s;
		int nlc = strleadlen(text, '\n');

		s = text + nlc;
		newline_count += nlc;
		line_count += nlc;

		/* a 'whitespace only' word. */
		UNC_ASSERT(!current_word->m_is_non_reflowable);
		UNC_ASSERT(current_word->m_word_length == 0);
		if (newline_count > 0)
		{
			/*
			merge multiple newline chunks when intermediate lines only carry whitespace

			WARNING: we reverse scan to find a suitable 'previous' box, but we MAY not find
			         any (this happens, for example, when we parse a line-continued comment
					 like this when it occurs /outside/ a preprocessor macro; an otherwise
					 valid situation):

					 `(*                                    \
					 `  A buggersome comment.				\
					 `*)
			*/
			if (word_idx > 0)
			{
				int prev_idx = word_idx;
				reflow_box *prev = words.get_printable_prev(prev_idx);

				if (prev)
				{
					UNC_ASSERT(prev != current_word);
					//UNC_ASSERT(!prev->m_do_not_print);
					UNC_ASSERT(current_word->m_line_count == 0);

					if (newline_count > 0
						&& prev->m_line_count > 0
						&& prev->m_word_length == 0
						// && prev->m_left_edge_thickness == 0
						// && prev->m_right_edge_thickness == 0
						)
					{
						/* 'prev' only lists pure whitespace: contract with 'current' */
						newline_count += prev->m_line_count;
						//UNC_ASSERT(prev->m_is_first_on_line);

						/* clear the now unused-again box */
						memset(current_word, 0, sizeof(*current_word));

						current_word = prev;
						UNC_ASSERT(prev->m_text);
						//text = prev->m_text;
						word_idx = prev_idx;
					}
				}
			}

			/* words always point *past* the leading whitespace! Here, there's no leading whitespace... */
			current_word->m_text = text;
			current_word->m_orig_hpos = 0;
			current_word->m_word_length = 0; /* ! */
			current_word->m_leading_whitespace_length = 0;
			//current_word->m_left_priority = -100; /* newline is high priority linebreak option */
			current_word->m_right_priority = -100; /* newline is high priority linebreak option */
			current_word->m_line_count = newline_count;
#if 0
			if(current_word->m_line_count > 1)
				fprintf(stderr, "!%d\n", words.count());
#endif

			current_word = words.prep_next(word_idx);

			/*
			RESET the ASCII ART and/or BOXED markers when we hit a double newline
			*/
			if (newline_count >= 2)
			{
				in_probable_ascii_art_cmt = false;
				in_probable_boxed_cmt = false;

				if (parse_mode == IN_NONREFLOW_PARAGRAPH)
				{
					/* empty line: end of paragraph */
					parse_mode = REGULAR_PARSE_MODE;
				}
			}

			newline_count = 0;
		}

		UNC_ASSERT(newline_count == 0);
		text = s;

		/* mark the current box as the first to occur on a new line */
		current_word->m_is_first_on_line = true;

		const char *sol = text;
		const char *eol = strchrnn(text, '\n');
		const char *next_line = eol;

		int spc = strleadlen(text, ' ');
		s = text + spc;
		current_word->m_leading_whitespace_length = spc;

		const char *e;
		spc = strtaillen(s, eol, ' ');
		e = eol - spc;
		UNC_ASSERT(e >= s);

		//if (newline_count > 0)
		//{
		//	current_word->m_left_priority = -1000; /* newline is high priority linebreak option */
		//}

		/*
		should we reset the parse mode to regular now?

		This depends on whether or not we've hit an empty line (for paragraphs) or related conditions.
		*/
		const char *end_of_non_reflow_chunk = m_comment;

		switch (parse_mode)
		{
		case REGULAR_PARSE_MODE:
			if (s < e)
			{
				/*
				only recognize the 'no reflow' marker when it sits on a line of its own!

				Yes, we /do/ recognize it when it's just a part of a line, but that case is handled
				further down below.
				*/
				nrfl_start_marker = str_in_set(m_no_reflow_marker_start, s, e - s);

				if (nrfl_start_marker >= 0)
				{
					parse_mode = IN_NONREFLOW_SECTION;
				}
			}
			break;

		case IN_NONREFLOW_LINE:
			/* we parse the next line now, so we should reset the mode unconditionally */
			parse_mode = REGULAR_PARSE_MODE;
			break;

		case IN_NONREFLOW_PARAGRAPH:
			if (s == e)
			{
				/* empty line: end of paragraph */
				parse_mode = REGULAR_PARSE_MODE;
			}
			break;

		case IN_NONREFLOW_COMMENT:
			/* stay in this mode until we've reached the very end... */
			break;

		case IN_NONREFLOW_SECTION:
			/*
			stay in this mode until we've /passed beyond/ the end marker.

			Also make sure the end marker matches the start marker: this is done by
			matching up their indexes in the marker array, as we ASSUME both start and end
			tag sets are matched pairs all the way.
			*/
			if (s < e)
			{
				int nrfl_end_marker = str_in_set(m_no_reflow_marker_end, s, e - s);

				UNC_ASSERT(nrfl_start_marker >= 0);
				if (nrfl_end_marker >= 0 && nrfl_end_marker == nrfl_start_marker)
				{
					/*
					treat the line with the end marker as non-reflowable.

					This of course assumes the end marker sits on the line on its own, as was the matching start marker.
					*/
					parse_mode = IN_NONREFLOW_LINE;
				}
				else
				{
					/*
					see if the end marker is part of this line: if it is, note the point /beyond/ the marker as
					that is the spot where the 'no reflow' condition ends.
					*/
					for (const char *p = s; p < e; p++)
					{
						nrfl_end_marker = str_in_set(m_no_reflow_marker_end, p, e - p);

						if (nrfl_end_marker >= 0 && nrfl_end_marker == nrfl_start_marker)
						{
							end_of_non_reflow_chunk = p + strlen(m_no_reflow_marker_end[nrfl_end_marker]);
							parse_mode = REGULAR_PARSE_MODE;
							break;
						}
					}
				}
			}
			break;
		}

		int ascii_art_count = 0;
		int marker_count = 0;
		int print_count = 0;
		count_graphics_nonreflow_and_printable_chars(s, (int)(e - s),
												  &ascii_art_count,
												  &marker_count,
												  &print_count);

		/*
		ASCII art heuristics:
		*/
		if (ascii_art_count >= (1 + print_count) / 2
			&& ascii_art_count >= m_cmt_reflow_graphics_threshold)
		{
			/* use ascii_art_count > 0 as the marker */
		}
		else
		{
			ascii_art_count = 0;
		}
		/*
		boxed comment heuristics: box start and end lines will have 50% or more printable chars on their lines, at least
		the part between the leading and trailing whitespace.

		Then there's the bit about the number of marker characters surpassing the configured threshold value too.
		*/
		if (marker_count >= (1 + print_count) / 2
			&& marker_count >= m_cmt_reflow_box_threshold)
		{
			/* use marker_count > 0 as the marker */
		}
		else
		{
			marker_count = 0;
		}

		/*
		hunt down the end of another word; before we do so,
		we should check whether this line is part of a 'graphic element', i.e.
		a non-reflowable chunk.

		Such non-reflowable chunks always span entire lines and MAY span multiple
		lines.

		If it is, mark it down as a single non-reflowable 'word'.
		*/
		if (s < e
			&& in_set(m_cmt_reflow_box_markers, e[-1])
			&& in_set(m_cmt_reflow_box_markers, *s)
			)
		{
			/*
			probably a boxed comment!

			Check the heuristic statistics for this line to make sure it is truly
			part of a boxed text: the first and last lines of such boxes consist
			of a lot of marker characters and little text.

			ASCII art has precedence over boxed comment, when both types are 'triggered' on this line.

			Also, boxed comment cannot occur within an ASCII art chunk as that would be an inherent
			part of the ASCII art!
			*/
			if (!in_probable_boxed_cmt && !in_probable_ascii_art_cmt)
			{
				in_probable_boxed_cmt = ((marker_count > 0) && (ascii_art_count == 0));
			}
		}
		else if (s < e
			&& !in_set(m_cmt_reflow_box_markers, *s)
			&& !in_set(m_cmt_reflow_box_markers, e[-1]))
		{
			/*
			turn OFF the 'boxed text' signal when the new line doesn't start nor end with a 'box' character...
			*/
			in_probable_boxed_cmt = false;
		}
		else if (s == e)
		{
			/*
			we've run into an empty line. Do we keep the 'boxed text' signal turned ON
			when the new line is entirely empty???

			Nope, we don't.
			*/
			in_probable_boxed_cmt = false;
		}

		/*
		for 'boxed text' we strip away the leading and trailing boxed marker characters to better
		process what's within.

		However, this process is ONLY done when a 'boxed text' is allowed to reflow, as for very fancy
		'boxed text' art there's a chance this action will partly remove the 'fancy' box art by replacing
		it with a basic box at render time.
		*/
		if (in_probable_boxed_cmt)
		{
			/* push a special 'word' token on the stack */
			current_word->m_text = s;
			current_word->m_orig_hpos = (int)(s - sol);
			current_word->m_word_length = 0; /* special! */

			UNC_ASSERT(s < e);

			current_word->m_is_part_of_boxed_txt = true;
			UNC_ASSERT(current_word->m_is_first_on_line);

			/* should boxed text be allowed to reflow? */
			if (!m_cmt_reflow_box
				|| parse_mode != REGULAR_PARSE_MODE
				|| s < end_of_non_reflow_chunk)
			{
				current_word->m_word_length = (int)(e - s);
				UNC_ASSERT(current_word->m_text);
				UNC_ASSERT(newline_count == 0);
				current_word->m_trailing_whitespace_length = (int)(eol - e);
				current_word->m_is_non_reflowable = true;
				current_word->m_floodfill_non_reflow = true;
				//current_word->m_line_count = newline_count;

				text = s = next_line;
				newline_count = 0;

				UNC_ASSERT(s >= e);

				current_word = words.prep_next(word_idx);
				continue;
			}
			else
			{
				/*
				this is a bit nasty: we also register various end-of-line border specifics with the first 'word'
				on this line.

				But this is okay, as this is a special word anyhow: it will not store any text.
				*/
				current_word->m_trailing_whitespace_length = (int)(eol - e);
				//current_word->m_line_count = newline_count;

				if (in_set(m_cmt_reflow_box_markers, *s))
				{
					current_word->m_left_edge_text = s;
					int i = (int)strspn(s, m_cmt_reflow_box_markers);
					UNC_ASSERT(i >= 1);
					current_word->m_left_edge_thickness = i;
					s += i;
					text = s;
					s += strleadlen(s, ' ');
				}
				UNC_ASSERT(current_word->m_right_edge_thickness == 0);
				if (s < e && in_set(m_cmt_reflow_box_markers, e[-1]))
				{
					int i = strrspn(s, e, m_cmt_reflow_box_markers);
					current_word->m_right_edge_text = e - i;
					current_word->m_right_edge_thickness = i;
					e -= i;
					eol = e;
					e -= strtaillen(s, e, ' ');
				}
			}

			current_word = words.prep_next(word_idx);

			current_word->m_leading_whitespace_length = (int)(s - text);

			/*
			now also recalculate the grax 'n print counts for ASCII art detection, etc. down below.
			*/
			ascii_art_count = 0;
			marker_count = 0;
			print_count = 0;
			count_graphics_nonreflow_and_printable_chars(s, (int)(e - s),
												  &ascii_art_count,
												  &marker_count,
												  &print_count);

			/*
			redo the ASCII art heuristics:
			*/
			if (ascii_art_count >= (1 + print_count) / 2
				&& ascii_art_count >= m_cmt_reflow_graphics_threshold)
			{
				/* use ascii_art_count > 0 as the marker */
			}
			else
			{
				ascii_art_count = 0;
			}
		}

		/*
		test for ASCII art chunks:

		these always exist on lines of their own, just like boxed texts. However, the only heuristic
		here is the number of graphical characters versus the number of printable characters, i.e. in a way the
		graphics characters versus alphabetical and numerical chars ratio.

		Warning:
		there's a very large chance that 'boxed texts' are detected through a character set quite similar to
		'ASCII art' content, so we should make sure a line which has just been detected as being a 'boxed text'
		delimiting line, isn't also rated as ASCII art content.

		Other than this, there's no reason why a piece of ASCII art couldn't exist within a 'boxed text'.
		*/
		if (!in_probable_ascii_art_cmt)
		{
			/*
			heuristics:
			*/
			in_probable_ascii_art_cmt = (ascii_art_count > 0);
		}
		else if (in_probable_ascii_art_cmt && s == e)
		{
			/*
			otherwise, when an empty(!) line is hit, then, apparently, the 'ASCII art' section has ended.
			*/
			in_probable_ascii_art_cmt = false;
		}

		if (in_probable_ascii_art_cmt)
		{
			UNC_ASSERT(text < eol);
			UNC_ASSERT(s < e);

			current_word->m_is_part_of_graphical_txt = true;

			UNC_ASSERT(!current_word->m_text);
			UNC_ASSERT(current_word->m_word_length == 0);
			UNC_ASSERT(newline_count == 0);
			current_word->m_text = s;
			UNC_ASSERT(s >= sol);
			current_word->m_orig_hpos = (int)(s - sol);
			current_word->m_word_length = (int)(e - s);
			current_word->m_trailing_whitespace_length = (int)(eol - e);
			current_word->m_is_non_reflowable = true;
			current_word->m_floodfill_non_reflow = true;
			//current_word->m_line_count = newline_count;

			text = s = next_line;
			newline_count = 0;

			UNC_ASSERT(s >= e);

			current_word = words.prep_next(word_idx);
			continue;
		}


		/*
		After we've checked whether the line is surrounded by box marks (or not), we should check
		whether the current line/paragraph/comment should be reflown at all, due to 'hints' at the start of this
		or previous lines.
		*/
		if (parse_mode == REGULAR_PARSE_MODE && s < e && s >= end_of_non_reflow_chunk)
		{
			if (in_set(m_cmt_reflow_no_line_reflow_markers_at_SOL, *s))
			{
				/* the current text line is non-reflowable; mode reverts at end-of-line. */
				parse_mode = IN_NONREFLOW_LINE;
			}
			else if (in_set(m_cmt_reflow_no_par_reflow_markers_at_SOL, *s))
			{
				/* reparse the current text chunk in para-non-reflow mode; mode reverts at double newline == end-of-para. */
				parse_mode = IN_NONREFLOW_PARAGRAPH;
			}
			else if (in_set(m_cmt_reflow_no_cmt_reflow_markers_at_SOL, *s))
			{
				/* reparse the current+remaining text in comment-nonreflow mode */
				parse_mode = IN_NONREFLOW_COMMENT;
			}
		}

		if (parse_mode != REGULAR_PARSE_MODE)
		{
			/* the current line may NOT be reflown: treat as a single word and mark as non-reflowable. */
			if (text < eol)
			{
				UNC_ASSERT(s < e);

				UNC_ASSERT(!current_word->m_text);
				UNC_ASSERT(newline_count == 0);
				current_word->m_text = s;
				UNC_ASSERT(s >= sol);
				current_word->m_orig_hpos = (int)(s - sol);
				current_word->m_word_length = (int)(e - s);
				current_word->m_trailing_whitespace_length = (int)(eol - e);
				current_word->m_is_non_reflowable = true;
				//current_word->m_floodfill_non_reflow = true;
				//current_word->m_line_count = newline_count;

				text = next_line;
				newline_count = 0;

				current_word = words.prep_next(word_idx);
				continue;
			}
		}

		UNC_ASSERT(!current_word->m_is_non_reflowable);
		if (s < e)
		{
			UNC_ASSERT(parse_mode == REGULAR_PARSE_MODE);

			/*
			as we now know we are in (probable) reflowable county, we chop the current line into words.

			Right now we're at start of word always:
			*/
			UNC_ASSERT(*text != '\n');
			UNC_ASSERT(*s != '\n');

			/*
			we're at a certified 'word' edge.

			Chop up the words on this line...
			*/
			UNC_ASSERT(!current_word->m_is_non_reflowable);
			/* words always point *past* the leading whitespace! */
			//current_word->m_text = s;
			//current_word->m_orig_hpos = (int)(s - last_nl);
			//current_word->m_word_length = 0; /* ! */
			current_word->m_leading_whitespace_length = (int)(s - text);
			//current_word->m_left_priority = -1000; /* newline is high priority linebreak option */
#if defined(_MSC_VER)
#pragma message(__FILE__ "(" STRING(__LINE__) ") : TODO: check why this assert and assignment are here and when the assert fires.")
#endif
			UNC_ASSERT(current_word->m_is_first_on_line);

			UNC_ASSERT(newline_count == 0);
			//newline_count = 0;

			text = s;
			//current_word = prep_next_word_box(words, word_idx);

			/*
			Now for the interesting bit. What IS a 'word', really?

			A 'word' here is either a:

			- punctuation mark (e.g. ',') -- yes, we register them seperately as they constitute
			  linebreak suggestions.

			  Also note that 'formulas' are chopped up alongside, e.g. '1 + 2' will split into
			  3 'words', as will the not-so-helpful-for-chopping, yet 'identical', character sequence '1+2'.

			  And did I mention quotes? Quotes are hard to disambiguate, as they can exist in
			  pairs and single occurrences in English texts, e.g.

			  "It's hard to tell" (paired ["], single ['])

			  'The masters' key' (paired ['], single ['] for possessive plural: masters+s --> masters')

			  Should we support 'longest match' or 'shortest match'? Since we work in a line-oriented
			  fashion, there's not much matching to do, really -- this will need to be performed at a
			  higher parsing level -- so any non-obvious use of quotes at the start or end of a
			  word will produce one extra 'word' for the quote alone.
			  Since we have the ability to 'discard' ('do_not_print') individual boxes, we can merge
			  such boxes later on, when we have decided the quotes are part of the preceding or
			  succeeding word. Alternatively (and currently preferrably) we can tweak the linebreak
			  priorities for these, even 'keep-wih-*' marking them so the linebreaks will be positioned
			  elsewhere in the box sequence.

			  And how about punctuation character SEQUENCES, e.g. '...' or '((abc)def)'?
			  Given the ways we wish to see reflowing occur, it makes sense to split such sequences
			  into their individual parts, which is not to deny that the ellipsis example above ('...')
			  is just one, 3-char-wide, punctuation mark. How about splitting the '('...')'
			  character sequence just then?
			  Suggested:
			      [(] + ['] + [...] + ['] + [)]
			  and then the interspace in this sequence is hinted as very low priority for line breaks.

		    - a 'tag' in either XML,HTML or doxygen/javadoc format -- mind character sequences
			  such as '<xyz<abc>' which are definitely not a tag but MAY be a bit of cruft
			  plus possibly-maybe tag '<abc>': the text parser will have an impossible time
			  disambiguating that one, unless we introduce the enforced rule that such XML/HTML
			  tags cannot exist in 'paragraphs' where stray ',' or '>' roam as well.

			  And on the subject of 'paragraphs' there: we don't know yet where our paragraph
			  'edges' are so we dial down to the next best thing: rule enforced for this entire
			  text.

			  NOTE that XML/XHTML tags therefore are the odd one out as these may span multiple lines
			  and we wish to detect them, so we add a bit of additional logic to cope with multi-line
			  spanning XML/XHTML tags.

			  To ensure we don't assume odd items such as a couple of stray '<' and '>' at just the right
			  places in your text, multi-line XML tags are parsed rigorously according to the official format:

			    <tag attrib=value | attrib='value' | attrib="value [/]>

			  Anything out of the ordinary there will mark the text as NOT-XML-compliant and disable the
			  XML treatment altogether. (Lucky for us those stray '<' and '>' characters will be marked as
			  'math' items and most probably will result in keeping their surroundings together so that
			  crufty semi-XML text will reflow like we knew what was going in anyway! :-) (See a debug dump
			  of the comment reflower to see this happen.)

		    - a 'word' in the English sense: an unbroken isalnum() character sequence.

			  A word about a 'word' here: since the text-to-be-reflown most probably contains
			  technical datums such as variables, how about these examples then? Do they constitute
			  'one word' each, or is it okay to chop them up and apply the 'dont-break-here'
			  keep_with_* magic to the box sequences afterwards, as suggested for the quotes handling
			  above?

			  - namespace::class::member
			  - struct.item
			  - pointer->member
			  - __func__
			  - $m_member   // which is legal in C, C++ and PHP at least...
			*/
			while (text < eol)
			{
				current_word->m_text = text;
				UNC_ASSERT(s >= sol);
				current_word->m_orig_hpos = (int)(text - sol);

				/*
				mark tail of a 'non reflow' section as such:
				*/
				current_word->m_is_non_reflowable = (text < end_of_non_reflow_chunk);

				bool in_xml_tag = false;

				/*
				see if the 'no reflow' start marker is part of this line: if it is,
				we've a 'no reflow' section after all.
				*/
				nrfl_start_marker = str_in_set(m_no_reflow_marker_start, text, eol - text);

				if (nrfl_start_marker >= 0)
				{
					parse_mode = IN_NONREFLOW_SECTION;
					end_of_non_reflow_chunk = eol + m_comment_len; // fake the pointer to point to 'infinity'

					/*
					see if the end marker is part of this line: if it is, note the point /beyond/ the marker as
					that is the spot where the 'no reflow' condition ends.
					*/
					for (const char *p = text + strlen(m_no_reflow_marker_start[nrfl_start_marker]); p < eol; p++)
					{
						int nrfl_end_marker = str_in_set(m_no_reflow_marker_end, p, eol - p);

						if (nrfl_end_marker >= 0)
						{
							end_of_non_reflow_chunk = p + strlen(m_no_reflow_marker_end[nrfl_end_marker]);
							parse_mode = REGULAR_PARSE_MODE;
							break;
						}
					}

					UNC_ASSERT(current_word->m_text == text);
					current_word->m_is_non_reflowable = true;
					//current_word->m_floodfill_non_reflow = true;
					if (end_of_non_reflow_chunk < eol)
					{
						current_word->m_word_length = (int)(end_of_non_reflow_chunk - text);
					}
					else
					{
						current_word->m_word_length = (int)(eol - text);
					}

					text += current_word->m_word_length;

					spc = strleadlen(text, ' ');
					current_word->m_trailing_whitespace_length = spc;
					text += spc;

					current_word = words.prep_next(word_idx);
					continue;
				}

				if (m_xml_text_has_stray_lt_gt <= 0 && *text == '<')
				{
					/*
					XML/HTML tag start? Or is this a stray one? Or are there any
					stray '<' or '>' on this line?

					Only scan for another XML tag when we don't yet know or /do/ know /for sure/ that
					the entire text (from now on) is at least rudimentary XML/HTML compliant: no
					stray '<' or '>' or other illegalities within perceived 'tags' anywhere!

					XML/XHTML tags are a bugger as these can span multiple lines; they may and may not
					include attributes and stuff and it's a nuisance in general.

					We tackle them here by scanning the ENTIRE TAG all at once, newlines and all.
					When we hit whitespace between tag id and attributes and in between attributes
					we push additional boxes so we can reflow large multi-attribute tags later on when
					we want / need to.

					The drawback of this approach is, however, that we now have have two zones in our code
					where newlines are processed, but that's a minor issues, really.

					The second issue to watch out for is that, while we are happily pushing attribute boxes
					'n all, we don't know yet whether the perceived tag actually IS a valid one, since we
					do all this in a single scan. To counter any illegalities found out later on, we keep
					a reference to the starting box around, so we can always pop back to that one when the
					dung hits the AC.

					Note: as a XML-validity scan traverses the entire text, this 'box pushing' for the tag
					      itself will only be done until we hit the end of the tag: (tag_end != NULL)
					*/
					const int start_of_tag_boxidx = word_idx;
					const char *text_orig_ptr = text;
					UNC_ASSERT(current_word->m_text == text);
					const char *last_nl = sol;

					int lt_gt_count = 1;
					bool is_end_tag = (text[1] == '/');
					bool is_closed_tag = false; /* tag of type '<tag/>' instead of '<tag>' */
					bool is_cdata_chunk = false;
					bool is_legal_tag_set = true; /* be an optimist */

					enum
					{
						NODE_NAME_START,
						NODE_NAME,
						ATTRIBUTE_NAME_START,
						ATTRIBUTE_NAME,
						ATTRIBUTE_VALUE,
						OUTSIDE_ANY_TAG,
						IN_CDATA_CHUNK,
					} xmldec_mode = NODE_NAME_START;
					int attr_quote = 0;
					const char *tag_end = NULL;

					/*
					XML parsing should be able to handle newlines in the tags or it's useless.

					WARNING: this scan MAY cross newline boundaries as XML/[X]HTML tags
					         MAY span multiple lines.
					*/
					if (text[1] == '!' && !strncmp("[CDATA[", text + 2, 7))
					{
						UNC_ASSERT(!is_end_tag);
						xmldec_mode = IN_CDATA_CHUNK;
						is_cdata_chunk = true;
						text += 7;
					}

					for (s = text + 1 + is_end_tag; *s; s++)
					{
						UNC_ASSERT(lt_gt_count >= 0 && lt_gt_count <= 1);

						if (xmldec_mode == NODE_NAME_START)
						{
							if (unc_isalpha(*s))
							{
								xmldec_mode = NODE_NAME;
								continue;
							}
							/* XML instructions, DTD lines */
							if (in_set("?!", *s) && unc_isalpha(s[1]))
							{
								xmldec_mode = NODE_NAME;
								s++;
								continue;
							}

							/*
							else: bad tag!
							*/
							is_legal_tag_set = false;
							if (!m_xml_offender)
							{
								m_xml_offender = s;
							}
							break;
						}

						if (xmldec_mode == NODE_NAME)
						{
							if (unc_isalnum(*s) || in_set("-_", *s))
							{
								continue;
							}
							if (unc_isspace(*s))
							{
								if (!tag_end)
								{
									push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
								}

								s--; /* compensate for s++ in loop; s already points past the WS */
								xmldec_mode = ATTRIBUTE_NAME_START;
								continue;
							}

							if (!in_set("/>", *s))
							{
								/*
								else: bad tag!
								*/
								is_legal_tag_set = false;
								if (!m_xml_offender)
								{
									m_xml_offender = s;
								}
								break;
							}
						}

						if (xmldec_mode == ATTRIBUTE_NAME_START)
						{
							if (unc_isspace(*s))
							{
								continue;
							}

							if (unc_isalpha(*s))
							{
								xmldec_mode = ATTRIBUTE_NAME;
								continue;
							}

							if (!in_set("/>", *s))
							{
								/*
								else: bad attrib tag!
								*/
								is_legal_tag_set = false;
								if (!m_xml_offender)
								{
									m_xml_offender = s;
								}
								break;
							}
						}

						if (xmldec_mode == ATTRIBUTE_NAME)
						{
							if (unc_isalnum(*s) || in_set("-_", *s))
							{
								continue;
							}
							if (unc_isspace(*s))
							{
								/* an attribute without a value; probably HTML code, e.g. '<th nowrap>' */
								if (!tag_end)
								{
									push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
								}

								s--; /* compensate for s++ in loop; s already points past the WS */
								xmldec_mode = ATTRIBUTE_NAME_START;
								continue;
							}
							if (*s == '=')
							{
								attr_quote = 0;
								if (in_set("'\"", s[1]))
								{
									attr_quote = s[1];
									s++;
								}
								xmldec_mode = ATTRIBUTE_VALUE;
								continue;
							}

							if (!in_set("/>", *s))
							{
								/*
								else: bad attrib tag!
								*/
								is_legal_tag_set = false;
								if (!m_xml_offender)
								{
									m_xml_offender = s;
								}
								break;
							}
						}

						if (xmldec_mode == ATTRIBUTE_VALUE)
						{
							if (*s && *s == attr_quote)
							{
								s++;
								if (unc_isspace(*s))
								{
									/* end of value string */
									if (!tag_end)
									{
										push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
									}

									s--; /* compensate for s++ in loop; s already points past the WS */
									attr_quote = 0; /* reset quote character! */
									xmldec_mode = ATTRIBUTE_NAME_START;
									continue;
								}

								if (!in_set("/>", *s))
								{
									/*
									else: bad attrib value!
									*/
									is_legal_tag_set = false;
									if (!m_xml_offender)
									{
										m_xml_offender = s;
									}
									break;
								}

								/* no newlines or WS here, but this is end of attribute nevertheless, keep in its own box for optimum reflow */
								if (!tag_end)
								{
									push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
								}

								attr_quote = 0; /* reset quote character! */
							}
							else if (!attr_quote)
							{
								if (unc_isspace(*s))
								{
									/* end of unquoted attribute value */
									if (!tag_end)
									{
										push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
									}

									s--; /* compensate for s++ in loop; s already points past the WS */
									attr_quote = 0; /* reset quote character! */
									xmldec_mode = ATTRIBUTE_NAME_START;
									continue;
								}
								if (!in_set("/><='\"", *s))
								{
									continue;
								}
								if (!in_set("/>", *s))
								{
									/*
									else: bad attribute value!
									*/
									is_legal_tag_set = false;
									if (!m_xml_offender)
									{
										m_xml_offender = s;
									}
									break;
								}
								/* end of non-quoted attribute value reached: hit [probable] end of tag */
								UNC_ASSERT(in_set("/>", *s));
								if (!tag_end)
								{
									push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
								}
							}
							else if (attr_quote)
							{
								/* within quotes, anything goes. Break boxes on WS though */
								if (unc_isspace(*s))
								{
									if (!tag_end)
									{
										push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
									}

									s--; /* compensate for s++ in loop; s already points past the WS */
								}
								continue;
							}
						}

						if (xmldec_mode == IN_CDATA_CHUNK)
						{
							/* scan for ']]>' */
							if (*s == ']' && !strncmp("]>", s + 1, 2))
							{
								xmldec_mode = OUTSIDE_ANY_TAG;
								s += 2;
								lt_gt_count--;
								UNC_ASSERT(lt_gt_count == 0);

								if (!tag_end)
								{
									tag_end = s;
									if (m_xml_text_has_stray_lt_gt == 0)
									{
										/*
										a previous run on this line already led to the conclusion that
										it is XML only, no stray lt/gt, so we can stop scanning right here!
										*/
										break;
									}
								}
								continue;
							}
							else
							{
								/* anything goes within a CDATA block. But break boxes on WS for better reflow later. */
								if (unc_isspace(*s))
								{
									if (!tag_end)
									{
										push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
									}

									s--; /* compensate for s++ in loop; s already points past the WS */
								}
								continue;
							}
						}

						if (xmldec_mode != OUTSIDE_ANY_TAG)
						{
							/* by the way: can't have </xyz/> tags; they're illegal! */
							if (*s == '/' && s[1] == '>' && !is_end_tag)
							{
								lt_gt_count--;
								UNC_ASSERT(lt_gt_count == 0);
								s++;

								xmldec_mode = OUTSIDE_ANY_TAG;

								/*
								and mark this one as the end of the tag!
								*/
								if (!tag_end)
								{
									is_closed_tag = true;
									tag_end = s;
									if (m_xml_text_has_stray_lt_gt == 0)
									{
										/*
										a previous run on this line already led to the conclusion that
										it is XML only, no stray lt/gt, so we can stop scanning right here!
										*/
										break;
									}
								}
								continue;
							}

							if (*s == '>')
							{
								lt_gt_count--;
								UNC_ASSERT(lt_gt_count == 0);

								xmldec_mode = OUTSIDE_ANY_TAG;

								/*
								and mark this one as the end of the tag!
								*/
								if (!tag_end)
								{
									UNC_ASSERT(!is_closed_tag);
									tag_end = s;
									if (m_xml_text_has_stray_lt_gt == 0)
									{
										/*
										a previous run on this line already led to the conclusion that
										it is XML only, no stray lt/gt, so we can stop scanning right here!
										*/
										break;
									}
								}
								continue;
							}

							/*
							anything else is an illegal character inside the xml tag


						WARNING: this comment will fail here (and we don't care right now...):

						(**

						The <see> tag is used within the text of other comment tags to specify a hyperlink. It is used inline as part of the text and usually just includes one attribute, cref:

						/// One of the associated member functions (<see
						/// cref="GiveTypeListHTMLHelp"/>,
						/// <see cref="GiveMemberListHTMLHelp"/>, <see
						/// cref="GiveMemberHTMLHelp"/>)
						/// is called to initiate and then return the transformation.

						*)

							*/
							is_legal_tag_set = false;
							if (!m_xml_offender)
							{
								m_xml_offender = s;
							}
							break;
						}
						else
						{
							/*
							outside XML tag: scan for the next tag:
							*/
							if (*s == '<')
							{
								lt_gt_count++;
								UNC_ASSERT(lt_gt_count == 1);
								is_end_tag = (s[1] == '/');
								is_closed_tag = false; /* tag of type '<tag/>' instead of '<tag>'? */
								is_cdata_chunk = false;

								xmldec_mode = NODE_NAME_START;
								UNC_ASSERT(attr_quote == 0);
								UNC_ASSERT(tag_end != NULL);

								if (is_end_tag)
								{
									s++;
								}
								else if (*s == '!' && !strncmp("[CDATA[", s + 1, 7))
								{
									xmldec_mode = IN_CDATA_CHUNK;
									is_cdata_chunk = true;
									s += 7;
								}
								continue;
							}

							if (*s == '>')
							{
								/*
								'dangling' [>] is illegal in an XML/HTML context
								*/
								m_xml_text_has_stray_lt_gt = 1;
								if (!m_xml_offender)
								{
									m_xml_offender = s;
								}
								break;
							}
						}
					}

					/*
					reset the text and the box array: assume it's not a [valid] XML tag by default.
					*/
					text = text_orig_ptr;
					int last_tag_box = word_idx;
					word_idx = start_of_tag_boxidx;
					current_word = &words[word_idx];

					/*
					When we're done, the 'is_end_tag' and 'is_closed_tag' flags MAY be damaged as the
					entire text nay have been scanned (happens on the first round).
					*/
					if (tag_end)
					{
						is_end_tag = (text[1] == '/');
						is_closed_tag = (tag_end[-1] == '/');
					}

					if (!is_legal_tag_set)
					{
						m_xml_text_has_stray_lt_gt = 2; // broken XML line detected.
					}

					/* when we arrive at EOT, the tag must be 'closed': */
					if (m_xml_text_has_stray_lt_gt < 0)
					{
						if (lt_gt_count == 0)
						{
							if (tag_end)
							{
								UNC_ASSERT(tag_end);
								m_xml_text_has_stray_lt_gt = 0;
							}
							else
							{
								m_xml_text_has_stray_lt_gt = 1; // broken XML line detected.
							}
						}
						else
						{
							m_xml_text_has_stray_lt_gt = 1; // broken XML line detected.
						}
					}

					if (m_xml_text_has_stray_lt_gt == 0)
					{
						in_xml_tag = true;
						UNC_ASSERT(text[0] == '<');
						if (text[1] == '!' && !strncmp("[CDATA[", text + 2, 7))
						{
							current_word->m_is_cdata_xml_chunk = true;
						}
						else
						{
							if (is_end_tag || is_closed_tag)
							{
								current_word->m_is_xhtml_end_tag = true;
							}
							if (!is_end_tag)
							{
								current_word->m_is_xhtml_start_tag = true;
							}
						}
						UNC_ASSERT(current_word->m_xhtml_matching_end_tag == 0);
						UNC_ASSERT(current_word->m_xhtml_matching_start_tag == 0);
						current_word->m_xhtml_matching_end_tag = -1;
						current_word->m_xhtml_matching_start_tag = -1;
						current_word->m_xhtml_tag_part_begin = start_of_tag_boxidx;
						current_word->m_xhtml_tag_part_end = last_tag_box;

						UNC_ASSERT(tag_end);
						tag_end++; /* point past the '>' */

						UNC_ASSERT(current_word->m_text);

						/*
						as an XML/HTML tag may span multiple lines, it's the odd one out: we
						need to:

						- correct the number of lines
						- adjust the EOL to point past the tag end again
						- ...
						*/
						if (tag_end > eol)
						{
							//current_word->m_contains_embedded_newlines = true;
							//current_word->m_hpos = (int)(current_word->m_text - sol);

							const char *nlp = eol;
							sol = eol + 1;

							while (nlp < tag_end)
							{
								line_count++;
								//newline_count++; <-- don't! These remain embedded.

								nlp = strchrnn(nlp + 1, '\n');
								sol = nlp;
							}
							eol = nlp;
							next_line = nlp;
						}

						/* update last box: word length + trailing WS */
						word_idx = last_tag_box;
						current_word = &words[word_idx];

						current_word->m_word_length = (int)(tag_end - current_word->m_text);

						text = tag_end;
						spc = strleadlen(text, ' ');
						current_word->m_trailing_whitespace_length = spc;
						text += spc;

						/*
						now update the entire series of boxes to bind them together into being a single XML/XHTML tag
						*/
						if (start_of_tag_boxidx != last_tag_box)
						{
							int w;
							reflow_box *dst = &words[start_of_tag_boxidx];
							reflow_box *src;

							dst->m_xhtml_tag_part_begin = start_of_tag_boxidx;
							dst->m_xhtml_tag_part_end = last_tag_box;
							dst->m_is_xhtml_tag_part = true;

							for (w = start_of_tag_boxidx + 1; w <= last_tag_box; w++)
							{
								src = dst;
								dst = &words[w];

								dst->m_is_cdata_xml_chunk = src->m_is_cdata_xml_chunk;
								dst->m_is_xhtml_start_tag = src->m_is_xhtml_start_tag;
								dst->m_is_xhtml_end_tag = src->m_is_xhtml_end_tag;

								dst->m_xhtml_tag_part_begin = start_of_tag_boxidx;
								dst->m_xhtml_tag_part_end = last_tag_box;
								dst->m_is_xhtml_tag_part = true;
							}
						}
						UNC_ASSERT(word_idx == last_tag_box);
						UNC_ASSERT(in_xml_tag);
						UNC_ASSERT(current_word->m_is_xhtml_start_tag || current_word->m_is_xhtml_end_tag || current_word->m_is_cdata_xml_chunk);

						current_word = words.prep_next(word_idx);
						continue;
					}
					else
					{
						/*
						NOT an XML tag; the word_idx et al have already been rewound back to the '<' position.

						Make sure the boxes used are zeroed or at least don't have their bits propagate illegally
						into their next use.
						*/
						if (last_tag_box > word_idx)
						{
							memset(&words[word_idx + 1], 0, sizeof(reflow_box) * (last_tag_box - word_idx));
						}
					}
				}
				if (m_xml_text_has_stray_lt_gt <= 0 && *text == '>')
				{
					/*
					Now this is definitely a stray '>' so no XML for us in here!
					*/
					UNC_ASSERT(m_xml_text_has_stray_lt_gt < 0);
					m_xml_text_has_stray_lt_gt = 1;
					if (!m_xml_offender)
					{
						m_xml_offender = text;
					}
				}

				if (current_word->m_is_first_on_line
					&& in_RE_set(m_cmt_reflow_bullets, *text))
				{
					/*
					Check if this is a viable bullet item:

					- must consist of one or bullet characters
					- must be terminated by a bullet terminator character
					- must be followed by at least one 'word' (in the broadest sense: anything printable!)
					*/
					UNC_ASSERT(text == current_word->m_text);
					s = text + 1;

					for (s = text + 1; *s && in_RE_set(m_cmt_reflow_bullets, *s); s++)
						;
					if (*s && in_RE_set(m_cmt_reflow_bullet_terminators, *s))
					{
						/*
						must be followed by one 'word' at least; simply check whether the current
						line has any further printable characters.
						*/
						if (*s != ' ')
						{
							/*
							   SPACE can be bullet sentinel! Do NOT count it as part of the bullet marker though!
						    */
							s++;
						}
						spc = strleadlen(s, ' ');

						UNC_ASSERT(eol >= s + spc);
						if (s + spc != eol
							&& is_viable_bullet_marker(text, s - text))
						{
							// bingo!
							current_word->m_word_length = (int)(s - text);

							current_word->m_trailing_whitespace_length = spc;
							text = s + spc;

							current_word->m_is_bullet = true;

							current_word = words.prep_next(word_idx);
							continue;
						}
					}
				}

				if (/* m_is_doxygen_comment
					&& */ (is_doxygen_tagmarker(text, doxygen_tag_marker)
					|| (*text == '{' && text[1] == '@')))
				{
					/*
					ignore the possibility of \xAB and \123 hex/octal escape sequences? No, can't have that.

					To help disambiguate, if possible, we need more context than this single line:

					we simply determine which 'tag' marker occurred first and stick with that one; in other words:
					we do not allow mixing \tag and @tag formats within a single text.
					*/
					UNC_ASSERT(text == current_word->m_text);
					bool is_doxygen_tag = true;
					bool is_inline_javadoc_tag = false;

					if (*text == '{')
					{
						/*
						javadoc internal tag?

						http://download.oracle.com/javase/1.5.0/docs/tooldocs/windows/javadoc.html#javadoctags

						Do a rough guestimate here...
						*/
						const char *sentinel = strchr(text + 1, '}');
						if (sentinel != NULL)
						{
							is_inline_javadoc_tag = true;
							text++;
						}
					}

					text++;

					/*
					collect entire tag:
					allow doxygen/javadoc words and special doxygen/javadoc markers
					*/
					s = text;
					if (*s == 'f'
						&& !is_inline_javadoc_tag
						&& in_set("{[$", s[1]))
					{
						/*
						one of the doxygen formula markers: \f$ \f[ \f] \f{ \f} \f{}{
						*/
						s++;

						char endmarker[4];

						endmarker[0] = text[-1];
						endmarker[1] = 'f';
						switch (*s)
						{
						case '[':
							endmarker[2] = ']';
							break;

						case '{':
							endmarker[2] = ']';
							break;

						default:
							UNC_ASSERT(*s == '$');
							endmarker[2] = *s;
							break;
						}
						endmarker[3] = 0;
						s++;

						const char *em = strstr(s, endmarker);
						if (!em)
						{
							/* not a valid doxygen formula! */
							is_doxygen_tag = false;
						}
						else
						{
							/*
							we grabbed the entire formula, which may span multiple lines.

							These buggers are non-reflowable!
							*/
							current_word->m_is_doxygen_tag = true;
							UNC_ASSERT(!current_word->m_is_inline_javadoc_tag);
							UNC_ASSERT(!is_inline_javadoc_tag);

							const char *last_nl = sol;
							push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);

							/*
							now chop the remainder until 'em' into non-reflowable boxes: one per line.
							*/
							while (s < em)
							{
								current_word->m_is_math = true;
								current_word->m_is_non_reflowable = true;
								current_word->m_is_doxygen_tag = true;

								const char *next_nl = strnchr(s, '\n', em - s);
								if (!next_nl)
								{
									next_nl = em;
								}
								else
								{
									line_count++;
								}
								spc = strtaillen(s, next_nl, ' ');
								next_nl -= spc;

								push_tag_piece_and_possible_newlines(words, s, word_idx, current_word, last_nl);
							}

							UNC_ASSERT(s == em);
							UNC_ASSERT(current_word->m_text == em);
							s += 3;
						}
					}
					else if (*s == '~'
						&& unc_isalpha(s[1])
						&& !is_inline_javadoc_tag)
					{
						/*
						doxygen \~language tag
						*/
						for (s += 2; unc_isalpha(*s); s++)
							;

						is_doxygen_tag = (!unc_isspace(*s));
					}
					else if (unc_isalpha(*s))
					{
						/*
						regular tag / inline javadoc tag
						*/
						for (s++; unc_isalpha(*s); s++)
							;

						/* inline javadoc tags must be followed by SPACE */
						if (is_inline_javadoc_tag && *s != ' ')
						{
							is_doxygen_tag = false;
						}
						else
						{
							UNC_ASSERT(!unc_isalpha(*s));
							is_doxygen_tag = (!unc_isdigit(*s) && !in_set("$_-", *s));
						}
					}
					else
					{
						/* not a doxygen/javadoc tag! */
						is_doxygen_tag = false;
					}

					if (is_doxygen_tag)
					{
						if (0 == doxygen_tag_marker)
							doxygen_tag_marker = text[-1];

						current_word->m_is_doxygen_tag = true;
						current_word->m_is_inline_javadoc_tag = is_inline_javadoc_tag;
						UNC_ASSERT(current_word->m_text);
						current_word->m_word_length = (int)(s - current_word->m_text);
						spc = strleadlen(s, ' ');
						current_word->m_trailing_whitespace_length = spc;
						text = s + spc;

						current_word = words.prep_next(word_idx);
						continue;
					}
				}

				if (*text == m_string_escape_char)
				{
					/*
					escape sequence?

					Note that this char can also occur as line continuation marker at EOL in the
					text, when the text has those but is not itself part of a multiline preprocessor macro, but
					if it did, we'd already removed the bugger in the initial text cleanup!
					*/
#if defined(_MSC_VER)
#pragma message(__FILE__ "(" STRING(__LINE__) ") : TODO: check why this assert fires.")
#endif
					//UNC_ASSERT(text == current_word->m_text);

					/*
					accept any C escape sequence or escaped regex bit. Don't be too picky...
					*/
					bool is_esc_code = true;

					UNC_ASSERT(*next_line == 0 ? text <= next_line : 1);
					if (unc_isdigit(text[1]))
					{
						int j;

						text += 2;

						// octal/decimal digit: possibly two more digits to go (octal).
						for (j = 3 - 2; j >= 0; j--)
						{
							if (!unc_isdigit(*text))
								break;
						}
					}
					else if (in_set("abnrv", text[1]))
					{
						text += 2;
					}
					else if (in_set("cdefghklpstuwDEHKLNPQRSUVW", text[1]))
					{
						/* regex escapes probably. It's safest to 'extend' the 'escape' to the first whitespace. */
						text += 2;

						while (!unc_isspace(*text))
						{
							text++;
						}
					}
					else if (unc_tolower(text[1]) == 'x'
							&& unc_isxdigit(text[2]))
					{
						// hex escape sequence. May be Unicode or Byte, i.e. up to 4 hex digits.
						text += 3;

						int j;

						for (j = 4 - 2; j >= 0; j--)
						{
							if (!unc_isxdigit(*text))
								break;
						}
					}
					else if (unc_isprint(text[1]) && !unc_isspace(text[1]) && !unc_isalnum(text[1]))
					{
						// things like '\$', '\'', etc.
						text += 2;
					}
					else
					{
						/*
						this '\' should not be considered an 'escape sequence'.

						It /might/ be a line continuation character /outside/ preprocessor macro bounds: when it is,
						kill it!
						*/
						if (text[1] == '\n' || text[1] == 0)
						{
							UNC_ASSERT(current_word->m_text);
							text++;
							UNC_ASSERT(text - current_word->m_text == 1);
							current_word->m_word_length = (int)(text - current_word->m_text);
							current_word->m_do_not_print = true;

							current_word = words.prep_next(word_idx);
							continue;
						}
						else
						{
							is_esc_code = false;
						}
					}

					if (is_esc_code)
					{
						UNC_ASSERT(current_word->m_text);
						current_word->m_word_length = (int)(text - current_word->m_text);
						current_word->m_is_escape_code = true;

						spc = strleadlen(text, ' ');
						current_word->m_trailing_whitespace_length = spc;
						text += spc;

						current_word = words.prep_next(word_idx);
						continue;
					}

					/*
					otherwise: reset 'text' for the next parse attempt below.
					*/
					text = current_word->m_text;
				}

				/* collect entire word/esc seq/tag */
				if (unc_isident(*text))
				{
					/*
					permit words like "it's" instead of chopping it into "it"+"'"+"s" ?
					*/
					bool allow_contractions = (!in_xml_tag && unc_isalpha(*text));
					bool is_uri = false;
					bool is_email = false;
					bool is_code = false;
					bool is_hyphenated = false;
					bool is_path = false;
					bool is_end_of_xml_tag = false;

					for (s = text + 1; *s; )
					{
						/* scan a basic word (or variable) */
						for (;
							unc_isalnum(*s)
							|| (allow_contractions && *s == '\'' && unc_isalpha(s[1]) && unc_isalpha(s[-1]))
							|| (is_code && in_set("_$", *s))
							|| (is_uri && in_set(":/\\.@%~!#$&()_-+={}[]|?", *s))
							|| (is_email && in_set(":.@!-", *s))
							|| (is_path && in_set(":/\\_-.~!&()+{}[],", *s))
							/* || (is_hyphenated && *s == '-' && unc_isalpha(s[-1]) && unc_isalpha(s[1])) */ ;
							s++)
							;

						switch (*s)
						{
						case '_':
							/* code? (variable, e.g. _T or a_var; the shortest var is '_' itself) */
							is_code = true;
							s++;
							continue;

						case '$':
							/* PHP/Perl code? Or C ('$' is allowed in var names - VMS inheritance) */
							if (unc_isident(s[1]))
							{
								is_code = true;
								s++;
								continue;
							}
							break;

						case '@':
						case '!':
							/* probably email address;
							can also be a user/pass sep in an URI.
							The '!' is for old bang-addresses a la lab!mit!edu */
							if (unc_isalpha(s[1]) && !is_code)
							{
								is_email = true;
								s += 2;
								continue;
							}
							break;

						case '.':
							/* punctuation or a dot in a FQDN? Can also be code, e.g. 'struct.member' ... but we'll leave the 'code' detection to other parts. */
							if (unc_isident(s[1]))
							{
								if (!is_code && unc_isalnum(s[1]))
								{
									is_uri = true;
									s += 2;
									continue;
								}
								else if (is_code && !unc_isdigit(s[1]))
								{
									s += 2;
									continue;
								}
							}
							break;

						case ':':
							/* URI: '://' or user:pass@uri or C++ class/namespace c::m */
							if (s[1] == ':')
							{
								s += 2;
								is_code = true;
								continue;
							}
							else if (s[1] == '/' && s[2] == '/')
							{
								is_uri = true;
								s += 3;
								continue;
							}
							else if (s[1] == '/' && (unc_isalnum(s[2]) || in_set(":/\\_-.~!&()+{}[],", s[2])))
							{
								is_path = true; /* Windows path, Cygwin/unix format */
								s += 2;
								continue;
							}
							else if (unc_isalnum(s[1]))
							{
								is_uri = true;
								s += 2;
								continue;
							}
							/* punctuation, apparently */
							break;

						case '-':
							/* hyphenation, code or uri? Assume the least */
							if (in_set(">", s[1]))
							{
								s += 2;
								is_code = true;
								continue;
							}
							else if (unc_isalpha(s[1]) && unc_isalpha(s[-1]) && !is_path && !is_code && !is_uri)
							{
								/* break hyphenated words at the hyphen!

								However, we only allow hyphenation for 'real' words, so NOT for 'code', e.g. 'var_hyph-anated' is 3 words, no hyphenation!

								Also, we do NOT consider the hyphen a hyphenation when we're right smack in the middle of a FQDN, e.g. 'www.hyph-en.at'
								*/
								is_hyphenated = true;
								s++;
								break;
							}
							else if (unc_isalnum(s[1]) && (is_uri || is_path))
							{
								s++;
								continue;
							}
							else if (is_code)
							{
								/* this is a MINUS, not a hyphen */
								break;
							}
							else if (!is_path && !is_code && !is_uri)
							{
								/* we're at end-of-line, so only whitespace follows and there's another 'word' on the next line */
								int nl_count = 0;
								const char *ws;

								for (ws = s + 1; *ws && unc_isspace(*ws); ws++)
								{
									nl_count += (*ws == '\n');
								}

								if (nl_count == 1 && unc_isalpha(*ws))
								{
									is_hyphenated = true;
									s++;
									break;
								}
							}
							break;

						case '%':
							/* urlencoded char in uri? e.g. %20 ? */
							if (unc_isxdigit(s[1]))
							{
								is_uri = true;
								s += 2;
								continue;
							}
							break;

						case '/':
						case '\\':
							/* path? */
							if (s[1] == s[0] || unc_isalnum(s[1]) || (s > text && (s[-1] == '.' || unc_isalnum(s[-1]))))
							{
								is_path = true;
								s++;
								UNC_ASSERT(*next_line == 0 ? s <= next_line : 1);
								continue;
							}
							else if (s[1] == '>' && in_xml_tag)
							{
								UNC_ASSERT(0);

								UNC_ASSERT(current_word->m_is_xhtml_start_tag);
								s += 2;
								is_end_of_xml_tag = true;
								UNC_ASSERT(*next_line == 0 ? s <= next_line : 1);
								break;
							}
							break;

						case '>':
							if (in_xml_tag)
							{
								UNC_ASSERT(0);

								UNC_ASSERT(current_word->m_is_xhtml_start_tag || current_word->m_is_xhtml_end_tag);
								s++;
								is_end_of_xml_tag = true;
								UNC_ASSERT(*next_line == 0 ? s <= next_line : 1);
								break;
							}
							break;

						default:
							break;
						}
						/* terminate word scan */
						break;
					}

					current_word->m_is_hyphenated = is_hyphenated;
					current_word->m_is_path = is_path;
					current_word->m_is_code = is_code;
					current_word->m_is_uri_or_email = (is_uri || is_email);

					current_word->m_word_length = (int)(s - current_word->m_text);
					UNC_ASSERT(current_word->m_text);
					spc = strleadlen(s, ' ');
					current_word->m_trailing_whitespace_length = spc;
					text = s + spc;

					if (in_xml_tag)
					{
						UNC_ASSERT(current_word->m_is_xhtml_start_tag || current_word->m_is_xhtml_end_tag);
					}

					current_word = words.prep_next(word_idx);

					if (in_xml_tag && !is_end_of_xml_tag)
					{
						reflow_box *prev_box = &words[word_idx - 1];

						current_word->m_is_xhtml_start_tag = prev_box->m_is_xhtml_start_tag;
						current_word->m_is_xhtml_end_tag = prev_box->m_is_xhtml_end_tag;
						UNC_ASSERT(current_word->m_xhtml_matching_end_tag == 0);
						UNC_ASSERT(current_word->m_xhtml_matching_start_tag == 0);
						current_word->m_xhtml_matching_end_tag = -1;
						current_word->m_xhtml_matching_start_tag = -1;
					}
					continue;
				}

				/* detect math/programming/punctuation marks */
				current_word->m_word_length = 1;
				UNC_ASSERT(current_word->m_text);
				current_word->m_is_punctuation = true;
				text++;

				bool identified_token = false; // TRUE when we have detected a combined token which should be treated as a single word.

				/*
				support both '..' (e.g. '1..2') and '...' ellipsis; while we're at it, heck,
				support any '.' sequence as a single ellipsis-like character.

				Heck, we'll even support arbitrary lengths of one character a single tokens,
				e.g. '???', '*******', etc.

				That's why we have the 'identified_token' flag: when TRUE, we've already detected
				a complete token and should not 'cluster more of same' further below.
				*/
				switch (text[-1])
				{
				case '\'':
				case '\"':
					// quotes: in a separate box now; bundle later.
					identified_token = true;
					current_word->m_is_quote = true;
					break;

				case '-':
					// hyphen, comment marker, but also part of '->' programming expression
					if (*text == '>')
					{
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						//current_word->m_is_code = true;
						text++;
					}
					else if (*text == '=')
					{
						// C/C++ shorthand: '-='
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_is_math = true;
						current_word->m_math_operator = MO_BINARY_OP;
						text++;
					}
					else if (*text == '-' && text[1] != '-')
					{
						// C/C++ shorthand: '--'
						if (text[1] == 0 || unc_isspace(text[1]))
						{
							if (text - 2 < m_comment
								|| unc_isspace(text[-2]))
							{
								UNC_ASSERT(text[-2] != '\n');
								break;
							}
							current_word->m_math_operator = MO_UNARY_POSTFIX_OP;
							current_word->m_keep_with_prev = true;
						}
						else
						{
							UNC_ASSERT(text[1] != '\n');
							current_word->m_math_operator = MO_UNARY_PREFIX_OP;
							current_word->m_keep_with_next = true;
						}
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_is_math = true;
						text++;
					}
					else if (*text != '-')
					{
						// C/C++ shorthand: '-' MAY BE a UNARY MINUS
						if (text[0] != 0
							&& !unc_isspace(text[0])
							&& (current_word->m_is_first_on_line
								|| text -2 < m_comment
								|| unc_isspace(text[-2])))
						{
							current_word->m_math_operator = MO_UNARY_PREFIX_OP;
							current_word->m_keep_with_next = true;
						}
						else
						{
							current_word->m_math_operator = MO_BINARY_OP;
						}
						identified_token = true;
						current_word->m_word_length = 1;
						UNC_ASSERT(current_word->m_text);
						current_word->m_is_math = true;
						//text++;
					}
					break;

				case '+':
					if (*text == '=')
					{
						// C/C++ shorthand: '+='
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_is_math = true;
						current_word->m_math_operator = MO_BINARY_OP;
						text++;
					}
					else if (*text == '+' && text[1] != '+')
					{
						// C/C++ shorthand: '++'
						if (text[1] == 0 || unc_isspace(text[1]))
						{
							if (text - 2 < m_comment
								|| unc_isspace(text[-2]))
							{
								UNC_ASSERT(text[-2] != '\n');
								break;
							}
							current_word->m_math_operator = MO_UNARY_POSTFIX_OP;
							current_word->m_keep_with_prev = true;
						}
						else
						{
							UNC_ASSERT(text[1] != '\n');
							current_word->m_math_operator = MO_UNARY_PREFIX_OP;
							current_word->m_keep_with_next = true;
						}
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_is_math = true;
						text++;
					}
					else if (*text != '+')
					{
						// C/C++ shorthand: '+' MAY BE a UNARY PLUS
						if (text[0] != 0
							&& !unc_isspace(text[0])
							&& (current_word->m_is_first_on_line
								|| text - 2 < m_comment
								|| unc_isspace(text[-2])))
						{
							current_word->m_math_operator = MO_UNARY_PREFIX_OP;
							current_word->m_keep_with_next = true;
						}
						else
						{
							current_word->m_math_operator = MO_BINARY_OP;
						}
						identified_token = true;
						current_word->m_word_length = 1;
						UNC_ASSERT(current_word->m_text);
						current_word->m_is_math = true;
						//text++;
					}
					break;

				case '<':
					if (text[0] == '<' && text[1] == '=')
					{
						// C/C++ shorthand: '<<='
						identified_token = true;
						current_word->m_word_length = 3;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text += 2;
					}
					else if (*text == '=')
					{
						// C/C++ LEQ comparison operator: '<='
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					break;

				case '>':
					if (text[0] == '>' && text[1] == '=')
					{
						// C/C++ shorthand: '>>='
						identified_token = true;
						current_word->m_word_length = 3;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text += 2;
					}
					else if (*text == '=')
					{
						// C/C++ GEQ comparison operator: '>='
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					break;

				case '=':
					if (text[0] == '=' && text[1] == '=')
					{
						// PHP/JS shorthand: '==='
						identified_token = true;
						current_word->m_word_length = 3;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text += 2;
					}
					else if (*text == '=')
					{
						// C/C++ EQ comparison operator: '=='
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					break;

				case '&':
				case '|':
					switch (*text)
					{
					case '=':
						/*
						C/C++ shorthands:
							'~=', '+=', etc.
						*/
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
						break;

					case '#':
						/*
						HTML entity number?
						Must be '&#[0-9]+;' or '&#x[0-9]+;' format.
						*/
						identified_token = is_html_numeric_entity(text - 1, &current_word->m_word_length);
						UNC_ASSERT(current_word->m_text);
						if (identified_token)
						{
							UNC_ASSERT(current_word->m_math_operator == 0);
							UNC_ASSERT(!current_word->m_is_math);
							current_word->m_is_xhtml_entity = true;
							text += current_word->m_word_length;
						}
						break;

					default:
						/*
						HTML entity name?
						Must be '&[A-Za-z0-9]+;' format.
						*/
						identified_token = is_html_entity_name(text - 1, &current_word->m_word_length);
						UNC_ASSERT(current_word->m_text);
						if (identified_token)
						{
							UNC_ASSERT(current_word->m_math_operator == 0);
							UNC_ASSERT(!current_word->m_is_math);
							current_word->m_is_xhtml_entity = true;
							text += current_word->m_word_length;
						}
						break;
					}
					break;

				case '^':
				case '%':
					if (*text == '=')
					{
						/*
						C/C++ shorthands:
						'^=', '%='
						*/
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					break;

				case '*':
				case '/':
					if (*text == '=')
					{
						/*
						C/C++ shorthands:
						'*=', '/='
						*/
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					else if (unc_isalpha(*text) || *text == '*')
					{
						/*
						A special case: it is a common custom in text to emphasize/italicize words by surrounding them
						with single '*' or '/' respectively, e.g. *bold* text with /particular/ emphasis. Though one
						should realize
								**extra bold**
						or
								***super shout***
						also occur in plain text: we cater for those as well.

						The general rule here is: the content between the stars/slashes MUST be entirely alphanumeric, though a few
						whitespaces and/or single quotes for shorthand are permitted, e.g.
								**It's the short #1 stick he gets**
						In other words: the text in between should start and end with alphanumerics, while start and end should have the
						same number of stars/slashes, while those stars AND slashes MUST NOT occur in the in-between text at all!

						Further heuristics dictate that such emphasis always remains on a /single line/ so any number of newlines
						breaks the emphasis search.
						*/
						int start_count = 1;
						int end_count;
						int alpha_count;
						int alpha_runlen;

						for (s = text; *s == text[-1]; s++)
						{
							start_count++;
						}

						if (text[-1] == '*' && start_count > 3)
						{
							/* not an acceptable number of stars to start with */
							break;
						}
						if (text[-1] == '/' && start_count > 1)
						{
							/* not an acceptable number of slashes to start with */
							break;
						}
						/*
						now check whether we have a suitable number of alphanumerics for starters.
						*/
						const char *start_of_innards = s;

						for (alpha_runlen = 0; s < eol && unc_isalpha(*s); s++)
						{
							alpha_runlen++;
						}
						if (alpha_runlen < 1)
						{
							break;
						}
						/*
						now seek the end of the emphasis chunk, while minding what we said earlier about what is
						accepted within...

						As a little speedup keep track of the length of the last run of alphanumerics. We can use that once we've found
						the end marker.
						*/
						for (alpha_count = alpha_runlen; s < eol && unc_isprint(*s) && !in_set("*/", *s); s++)
						{
							if (!unc_isalpha(*s))
							{
								alpha_runlen = 0;
							}
							else
							{
								alpha_runlen++;
								alpha_count++;
							}
						}
						/*
						check the end conditions: MUST end with enough alphanumerics and the 'innards' should be 'mostly text', i.e. 75% or better
						of the 'innards' must be alphanumerics:
						*/
						if (!in_set("*/", *s)
							|| alpha_runlen < 1
							|| s - start_of_innards == 0
							|| (100 * alpha_count) / (s - start_of_innards) < 75)
						{
							break;
						}

						for (end_count = 0; *s == text[-1]; s++)
						{
							end_count++;
						}
						if (end_count != start_count)
						{
							break;
						}

						/*
						As we've hit an 'emphasis block', we MIGHT treat it as a single word when there is no whitespace in there.

						What to do when there IS whitespace in there? We SHOULD be smart enough to take this chunk and chop it up,
						adjusting (replicating) the emphasis markers as needed on reflow, when the reflow engine decides to break
						the word sequence in here.

						TODO! ^^^^^^^^^^^^^^^^^^^^^
						*/
						current_word->m_is_punctuation = false;
						identified_token = true;
						UNC_ASSERT(current_word->m_text);
						current_word->m_word_length = (int)(s - current_word->m_text);
						current_word->m_is_emphasized = true;

						text = s;
					}
					break;

				case '.':
				case ':':
					if (*text == '=')
					{
						/*
						C/C++ shorthands:
						'~=', '+=', etc.,
						PHP shorthand
						'.=',
						Pascal assign
						':=',
						*/
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					break;

				case '!':
					if (*text == '=')
					{
						/*
						C/C++ shorthand: '!='
						*/
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					else
					{
						// either '!' or '!!' C/C++ shorthand?
						bool is_shorthand = (*text == '!' && (unc_isident(text[1])
															|| in_set("(:", text[1])));
						is_shorthand |= (unc_isident(*text) || in_set("(:", *text));

						if (!is_shorthand)
						{
							break;
						}
						identified_token = true;
						current_word->m_word_length = (*text == '!' ? 2 : 1);
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_UNARY_PREFIX_OP;
						current_word->m_keep_with_next = true;
						current_word->m_is_math = true;
						text++;
					}
					break;

				case '~':
					if (*text == '=')
					{
						/*
						C/C++ shorthand: '~='
						*/
						identified_token = true;
						current_word->m_word_length = 2;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_BINARY_OP;
						current_word->m_is_math = true;
						text++;
					}
					else if (unc_isident(*text) || in_set("(:\"", *text))
					{
						/*
						C/C++ shorthand: '~'
						*/
						identified_token = true;
						current_word->m_word_length = 1;
						UNC_ASSERT(current_word->m_text);
						current_word->m_math_operator = MO_UNARY_PREFIX_OP;
						current_word->m_keep_with_next = true;
						current_word->m_is_math = true;
					}
					break;

				case '[':
				case ']':
				case '(':
				case ')':
				case '{':
				case '}':
					// for reflow purposes, braces and brackets are broken individually
					identified_token = true;
					break;

				default:
					break;
				}
				UNC_ASSERT(*next_line == 0 ? text <= next_line : 1);

				// cluster 'more of the same' into a single token:
				if (!identified_token)
				{
					for ( ; text[0] == text[-1]; text++)
					{
						current_word->m_word_length++;
						UNC_ASSERT(current_word->m_text);
					}
				}

				if (!current_word->m_is_math)
				{
					// token has not yet been decoded...
					//
					// detect whether there's some math/code operators in there:
					if (current_word->m_word_length <= 2
						&& in_set("*^<>&|", current_word->m_text[0]))
					{
						current_word->m_is_math = true;
						current_word->m_math_operator = MO_BINARY_OP;
					}
					else if (current_word->m_word_length == 1
						&& in_set("%/", current_word->m_text[0]))
					{
						current_word->m_is_math = true;
						current_word->m_math_operator = MO_BINARY_OP;
					}
				}

				UNC_ASSERT(*next_line == 0 ? text <= next_line : 1);
				spc = strleadlen(text, ' ');
				current_word->m_trailing_whitespace_length = spc;
				text += spc;

				current_word = words.prep_next(word_idx);
			}
			UNC_ASSERT(*text == '\n' || *text == 0);
			UNC_ASSERT(*next_line == 0 ? text == next_line : 1);
		}
		else
		{
			UNC_ASSERT(!current_word->m_text);
			UNC_ASSERT(!current_word->m_word_length);
			UNC_ASSERT(current_word->m_leading_whitespace_length >= 0);
			UNC_ASSERT(*s == '\n' || *s == 0);
			UNC_ASSERT(current_word->m_leading_whitespace_length == 0 ? s == text : s == text + current_word->m_leading_whitespace_length);
			text = s;
			UNC_ASSERT(*next_line == 0 ? text == next_line : 1);
		}

		//UNC_ASSERT(*text == '\n' || *text == 0);
		UNC_ASSERT(*next_line == 0 ? text == next_line : 1);
		UNC_ASSERT(*next_line != 0 ? text == next_line : 1);
	}

	/*
	push a single non-printing box as a sentinel for forward scans through the box set.
	*/
	UNC_ASSERT(current_word->m_word_length == 0);
	UNC_ASSERT(current_word->m_leading_whitespace_length == 0);
	UNC_ASSERT(current_word->m_trailing_whitespace_length == 0);
	UNC_ASSERT(current_word->m_line_count == 0);
	current_word->m_line_count = 0;
	current_word->m_text = text;
	current_word->m_do_not_print = true;
	current_word->m_is_first_on_line = true;
	current_word->m_orig_hpos = 0;
	current_word->m_word_length = 0;
	current_word = words.prep_next(word_idx);

	if (m_xml_text_has_stray_lt_gt > 0)
	{
		/*
		'broken XML line detected' ... but did we find some XML after after all?

		If not, discard the 'offender' marker!
		*/
		bool has_xml = false;

		for (int i = 0; i < word_idx; i++)
		{
			reflow_box *box = &words[i];

			has_xml |= (box->m_is_xhtml_start_tag
						|| box->m_is_xhtml_end_tag);
						// || box->m_is_xhtml_entity
						// || box->m_is_xhtml_tag_part);
			if (has_xml)
				break;
		}

		if (!has_xml)
		{
			m_xml_text_has_stray_lt_gt = 0;
			m_xml_offender = NULL;
		}
	}
}









/*
correct/spread the already calculated lead/trail whitespace counts
and merge boxes which only register superfluous whitespace on empty lines.
*/
void cmt_reflow::optimize_reflow_boxes(words_collection &words)
{
	int i;
	reflow_box *prev = NULL;
	reflow_box *current = NULL;

	/* now scan the remainder of the box collection for mergable situations */
	for (i = 0; i < (int)words.count(); i++)
	{
		if (current && !current->m_do_not_print)
		{
			prev = current;
		}

		current = &words[i];

		if (current->m_do_not_print)
		{
			current = prev;
			continue;
		}

		if (prev)
		{
			UNC_ASSERT(prev != current);
			UNC_ASSERT(!prev->m_do_not_print);

			/*
			move leading whitespace to trailing whitespace, when there's no newline in between.
			*/
			if (current->m_line_count == 0
				&& prev->m_word_length > 0 /* don't move leading WS to previous chunk when that one is a linebreak carrier */
				&& current - prev == 1 /* only move WS when 'prev' is truly adjacent to 'current' */
				&& !current->m_is_first_on_line
				&& current->m_leading_whitespace_length)
			{
				prev->m_trailing_whitespace_length += current->m_leading_whitespace_length;
				current->m_leading_whitespace_length = 0;
			}
		}
	}
}









/**
Find the math, code, etc. markers and make sure those expressions / statements are marked as such in their entirety.

This means that keywords, etc. which are not yet marked as being 'is_math' or 'is_code' should be marked as such when occurring in
a math/code/... expression.
*/
void cmt_reflow::expand_math_et_al_markers(words_collection &words)
{
	int box_idx;
	reflow_box *box = NULL;

	for (box_idx = 0; box_idx < (int)words.count(); box_idx++)
	{
		reflow_box *prev_box = box;

		box = &words[box_idx];

		if (box->m_do_not_print)
		{
			box = prev_box;
			continue;
		}

		/*
		detect 'strings' within larger comments and keep those together.

		Make sure we only detect /leading/ quotes!

		NOTE: there is a wicked situation when this can be triggered involuntarily
		      when, for instance, the colloquial words such as "'em" are used in the comments. E.g.:

			      Got 'em!

			  Here the starting single quote should be recognized as NOT being the start of a
			  quoted /section/.
			  This is done by inspecting whether we hit a double newline 'paragraph break' during
			  our forward scan. When no matching quote has been found yet, then we do not mark the
			  quote as the start of a quoted section after all.

			  Of course for DOUBLE QUOTES the situation is not exactly that: you don't see many double quotes
			  in these odd situations where single single quotes are around, so all the wickedry is only
			  applied when we're scanning a single-quoted section. This assumes quite a few things and
			  does not play well with single quoted strings as in PHP or JS, but we'll get there when we get there, anyho'.

	    Be reminded that we should recognize quoted sections like these (and the fourth example is
		a construct where none of the quotes should trigger a quoted-section-marking, when this code
		acts at all intelligent...):

		      - As they say "here dragons roam"

			  - printf("hello world!\n")

			  - "That's nothing," he said, "chew on /this/!"

			  - 'That's nothing,' he said, 'chew on /this/!'

			  - But Manny, where's the chedda'?
			    You bonkers?! 's bloody Open Source, is what it is. No dough, bro'! Unless
				of course you'd perform services, eh!
				'Services'?!?! /I/ can't be buggered to perform any
				bloody 'services'! Me handbrake is who's hot on doing all 'em 'services', haw haw haw.
				<Whack!>
				Moron! Wasn't meanin' that kind 'o services. Now pick up what's left of your
				brain and dial me up some Guns 'n' Roses!

		The 'final solution' to these conundra is to recognize what is going in at a broader perspective:
		when we run into a mess for a certain type of quote (either single or double quote) in this
		comment, then we take note and mark the given type of quote as /not suitable/ for 'string marking':
		the quoted string marking is suppressed for this quote from now on.

		The tough decision was to either limit to a per-paragraph or per-comment basis, but the
		latter was chosen due to the consideration that a comment writer would, most probably, /not/
		change his/her quoting style within a single comment. And if he does, well... then there's
		an editorial mess already so not much damage we can do then, while we pray the whitespace
		is good enough to give us a good reflow anyhow.
		*/
		if (box->m_is_quote
			&& !box->m_is_part_of_quoted_txt
			&& !box->m_suppress_quote_for_string_marking)
		{
			if (box->m_is_first_on_line
				|| box->m_leading_whitespace_length > 0
				|| (prev_box && prev_box->m_trailing_whitespace_length > 0)
				|| (prev_box && prev_box->m_is_punctuation))
			{
				/* a 'good' starting quote: find the ending quote (if any) */
				int eos_idx;
				reflow_box *eos_box;
				bool hit_endquote = false;

				for (eos_idx = box_idx + 1; eos_idx < (int)words.count(); eos_idx++)
				{
					eos_box = &words[eos_idx];

					if (eos_box->m_do_not_print)
						continue;

					if (box->m_text[0] == '\''
						&& eos_box->m_line_count >= 2)
					{
						/* no closing ' quote before we hit double newline? Then ignore the single quote as a 'string marker' */
						break;
					}

					if (eos_box->m_is_quote
						&& !eos_box->m_suppress_quote_for_string_marking
						&& eos_box->m_word_length == box->m_word_length
						&& !memcmp(eos_box->m_text, box->m_text, eos_box->m_word_length))
					{
						UNC_ASSERT(!eos_box->m_is_part_of_quoted_txt);

						reflow_box *next_box = NULL;

						int i = eos_idx;
						next_box = words.get_printable_next(i);

						if (!eos_box->m_is_first_on_line
							&& (eos_box->m_trailing_whitespace_length > 0
								|| (next_box && next_box->m_line_count > 0)
								|| (next_box && next_box->m_leading_whitespace_length > 0)
								|| (next_box && next_box->m_is_punctuation)))
						{
							/*
							found a matching end quote!

							now mark the entire range as part of the quote, plus mark the
							start and end quote as keep-with-next/prev
							*/
							box->m_keep_with_next = true;
							eos_box->m_keep_with_prev = true;

							for (int j = box_idx + 1; j < eos_idx; j++)
							{
								reflow_box *b = &words[j];

#if 0
								b->m_right_priority += 10;
								b->m_left_priority += 10;
#endif

								b->m_is_part_of_quoted_txt = true;
							}
#if 0
							box->m_right_priority += 10;
#endif
							box->m_is_part_of_quoted_txt = true;
#if 0
							eos_box->m_left_priority += 10;
#endif
							eos_box->m_is_part_of_quoted_txt = true;

							hit_endquote = true;
							break;
						}
					}
				}

				if (!hit_endquote)
				{
					/*
					we've apparently run into a quote mess here: find us all similar quotes and suppress
					those for quoted string marking from now on.
					*/
					for (eos_idx = box_idx; eos_idx < (int)words.count(); eos_idx++)
					{
						eos_box = &words[eos_idx];

						if (eos_box->m_do_not_print)
							continue;

						if (eos_box->m_is_quote
							&& eos_box->m_word_length == box->m_word_length
							&& !memcmp(eos_box->m_text, box->m_text, eos_box->m_word_length))
						{
							eos_box->m_suppress_quote_for_string_marking = true;
						}
					}
				}
			}
		}
	}

	/*
	As we need the 'is_part_of_quoted_text' marker in the next math/code detection section,
	it's another loop over all the boxes...
	*/
	for (box_idx = 0; box_idx < (int)words.count(); box_idx++)
	{
		reflow_box *prev_box = box;

		box = &words[box_idx];

		if (box->m_do_not_print)
		{
			box = prev_box;
			continue;
		}

		/*
		NOTE: only detect is_math/is_code/... at the 'leading edge' i.e. only when the
		new box has such a marker SET while the previous box has NOT.

		This prevents repetitively reprocessing the same is_math/is_code/... group of
		boxes, once they have processed once.

		Extra: some simple bullets can also be math operators, e.g. '-', so we should only
		       accept 'true' math operators as a starting point here: when we find a math
			   operator at the start of a line, it can also be a bullet, and we should ignore that one
			   as a /start/. However, it /might/ be correct to include it in the math expression
			   scan later, assuming the math/code expression extends backwards into the previous
			   line, e.g.

			   <pre>
			     result = (var_a + var_b)
			              - (var_c + var_d);
			   </pre>

			   In fact, the example above shows that the math expression should already have been
			   expanded thanks to real math operators on the /previous/ line, so the whole 'bullet vs. math'
			   issue is a bit moot as the 'bullet', which clearly serves as a math operator here, has
			   already been included in the forward scan started on the previous line.

			   Hence, we can safely state that math expressions NEVER start with a math operator which
			   is also marked as a 'bullet'. Thus we can RESET any 'bullet' marker on math operators
			   as well, when they should turn up in a math expression scan.
		*/
		if (box->m_is_bullet)
		{
			box->m_is_code = false;
			box->m_is_math = false;
			box->m_math_operator = MO_NOT_AN_OP;
			continue;
		}

		if ((box->m_is_math && box->m_math_operator && (prev_box ? !prev_box->m_is_math : true))
			|| (box->m_is_code && (prev_box ? !prev_box->m_is_code : true)))
		{
			/*
			This identifies variables which have an '_' underscore or '$' aboard. These may exist alone. When
			they happen to be part of a larger code chunk, such a chunk would be partly identified as 'is_code' at least.

			Of course, we try to (crudely) recognize function calls and other 'C'-ish code statements in the comment
			text as well, so that's what we're going to do here.
			*/
			bool scan = true;
			bool master_is_math = box->m_is_math;
			bool master_is_code = box->m_is_code;
			bool master_is_quoted_text = box->m_is_part_of_quoted_txt;

			int start_idx = (int)words.count();
			int end_idx = 0;

			while (scan)
			{
				scan = false;

				/*
				scan backwards looking for the spot where the 'math chunk' started, most probably.

				We apply a heuristics here, which says:

				- attach the right-hand/left-hand values as required.

				- when we find braces just beyond those values, include those braces too.

				Note that the braces come with their own lh/rh requirements as well.

				For 'is_code' there's extra heuristics:

				- when there's an 'is_math' box preceding a code box, it is code also.

				- try to recognize 'reserved word' series, such as 'unsigned int a' which are three 'regular' boxes
				in a row while only the last one is /not/ a reserved word.
				*/
				bool lh_reqd = false;

				for (start_idx = box_idx; start_idx >= 0; start_idx--)
				{
					reflow_box *b = &words[start_idx];

					if (b->m_do_not_print)
						continue;

					if (master_is_quoted_text
						&& !b->m_is_part_of_quoted_txt)
					{
						/* ABORT as soon as we tread outside the quoted string itself (as we started in it and should stay inside) */
						break;
					}
					else if (!master_is_quoted_text
						&& b->m_is_part_of_quoted_txt
						&& master_is_code)
					{
						/* gobble entire quoted string when we're in 'code' mode */
						continue;
					}
					else if (b->m_is_math)
					{
						lh_reqd = !!(box->m_math_operator & MO_TEST_LH_REQD);
						master_is_math = true;
						continue;
					}
					else if (b->m_is_punctuation
						&& in_set("({[", b->m_text[0]))
					{
						lh_reqd = false;
						master_is_code |= in_set("{", b->m_text[0]);
						continue;
					}
					else if (b->m_is_punctuation
						&& in_set("]})", b->m_text[0]))
					{
						lh_reqd = true;
						master_is_code |= in_set("}", b->m_text[0]);
						continue;
					}
					else if (b->m_line_count >= 2 && !b->m_is_math && !b->m_is_code)
					{
						/* ABORT scan when an empty line == double newline is seen: */
						break;
					}
					else if (lh_reqd)
					{
						lh_reqd = false;
						continue;
					}
					else if (master_is_code)
					{
						lh_reqd = false;

						if (b->m_is_code)
						{
							continue;
						}

						const chunk_tag_t *t = find_keyword(b->m_text, b->m_word_length);

						if (t)
						{
							bool in_pp = comment_is_part_of_preproc_macro();
							bool pp_iter = ((t->lang_flags & FLAG_PP) != 0);

							if (((cpd.lang_flags & t->lang_flags) != 0)
								|| (in_pp && pp_iter))
							{
								/*
								reserved word for this language; this of course assumes any 'code'
								in the comments is the /same/ language as the source code itself!
								*/
								continue;
							}
						}
					}
					break;
				}
				start_idx++;

				{
				int i;

				for (i = start_idx; i <= box_idx; i++)
				{
					reflow_box *b = &words[i];

					master_is_math |= b->m_is_math;
					master_is_code |= b->m_is_code;
				}

				for (i = start_idx; i <= box_idx; i++)
				{
					reflow_box *b = &words[i];

					b->m_is_math |= master_is_math;
					b->m_is_code |= master_is_code;
				}
				}

				bool rh_reqd = false;

				/*
				Now scan forward, starting at the very START: this is done so we won't 'forget'
				some brace levels in there, ever.


				For example, without this tweak, the entire line of

				{a | f(a) > 7 AND a is an integer}

				would not have been marked as 'math' when '|' triggers the scan. Without the tweak, just the section

				a | f(a) > 7

				would be considered 'math'...
				*/
				for (end_idx = start_idx; end_idx < (int)words.count(); end_idx++)
				{
					reflow_box *b = &words[end_idx];

					if (b->m_do_not_print)
						continue;

					if (master_is_quoted_text
						&& !b->m_is_part_of_quoted_txt)
					{
						/* ABORT as soon as we tread outside the quoted string itself (as we started in it and should stay inside) */
						break;
					}
					else if (!master_is_quoted_text
						&& b->m_is_part_of_quoted_txt
						&& master_is_code)
					{
						/* gobble entire quoted string when we're in 'code' mode */
						rh_reqd = false;

						continue;
					}
					else if (b->m_is_punctuation
						&& in_set("({[", b->m_text[0]))
					{
						rh_reqd = false;

						master_is_code |= in_set("{", b->m_text[0]);

						/* find matching closing brace for this function call / code chunk! */
						int end_of_func_call = -1;
						int brace_count[3] = {0};
						int i;

						for (i = end_idx; i < (int)words.count(); i++)
						{
							b = &words[i];

							if (b->m_do_not_print)
								continue;

							if (master_is_quoted_text
								&& !b->m_is_part_of_quoted_txt)
							{
								/* ABORT as soon as we tread outside the quoted string itself (as we started in it and should stay inside) */
								break;
							}
							else if (!master_is_quoted_text
								&& b->m_is_part_of_quoted_txt)
							{
								/* ignore braces within stringed text chunks. */
								continue;
							}

							UNC_ASSERT(b->m_word_length > 0 && in_set("{}[]()", b->m_text[0])
								? b->m_word_length == 1
								: 1);
							UNC_ASSERT(b->m_word_length > 0 && in_set("{}[]()", b->m_text[0])
								? b->m_is_punctuation
								: 1);
							if (b->m_is_punctuation)
							{
								bool the_end = false;

								switch(b->m_text[0])
								{
								default:
									break;

								case '(':
									brace_count[0]++;
									break;

								case '[':
									brace_count[1]++;
									break;

								case '{':
									brace_count[2]++;
									break;

								case '}':
									brace_count[2]--;
									the_end = (brace_count[0] + brace_count[1] + brace_count[2] == 0);
									break;

								case ']':
									brace_count[1]--;
									the_end = (brace_count[0] + brace_count[1] + brace_count[2] == 0);
									break;

								case ')':
									brace_count[0]--;
									the_end = (brace_count[0] + brace_count[1] + brace_count[2] == 0);
									break;
								}

								if (the_end)
								{
									/* end of function arg set / array index / code chunk found. */
									end_of_func_call = i;
									break;
								}
							}
							else if (b->m_line_count >= 2 && !b->m_is_math && !b->m_is_code && !master_is_code)
							{
								/* ABORT scan when an empty line == double newline is seen, unless we're in a code chunk! */
								break;
							}
						}

						/* mark the entire range as 'is_math' */
						if (end_of_func_call > 0)
						{
							//words[end_idx].xhtml_matching_end_tag = end_of_func_call;
							//words[end_of_func_call].xhtml_matching_start_tag = end_idx;
							end_idx = end_of_func_call;
							continue;
						}
						break;
					}
					else if (b->m_is_punctuation
						&& in_set("]})", b->m_text[0]))
					{
						rh_reqd = false;

						master_is_code |= in_set("}", b->m_text[0]);

						/* scan in reverse to find matching brace */
						int start_of_func_call = -1;
						int brace_count[3] = {0};
						int i;

						for (i = end_idx; i >= 0; i--)
						{
							b = &words[i];

							if (b->m_do_not_print)
								continue;

							if (master_is_quoted_text
								&& !b->m_is_part_of_quoted_txt)
							{
								/* ABORT as soon as we tread outside the quoted string itself (as we started in it and should stay inside) */
								break;
							}
							else if (!master_is_quoted_text
								&& b->m_is_part_of_quoted_txt)
							{
								/* ignore braces within stringed text chunks. */
								continue;
							}

							UNC_ASSERT(b->m_word_length > 0 && in_set("{}[]()", b->m_text[0])
								? b->m_word_length == 1
								: 1);
							UNC_ASSERT(b->m_word_length > 0 && in_set("{}[]()", b->m_text[0])
								? b->m_is_punctuation
								: 1);
							if (b->m_is_punctuation)
							{
								bool the_end = false;

								switch(b->m_text[0])
								{
								default:
									break;

								case ')':
									brace_count[0]++;
									break;

								case ']':
									brace_count[1]++;
									break;

								case '}':
									brace_count[2]++;
									break;

								case '{':
									brace_count[2]--;
									the_end = (brace_count[0] + brace_count[1] + brace_count[2] == 0);
									break;

								case '[':
									brace_count[1]--;
									the_end = (brace_count[0] + brace_count[1] + brace_count[2] == 0);
									break;

								case '(':
									brace_count[0]--;
									the_end = (brace_count[0] + brace_count[1] + brace_count[2] == 0);

									if (b->m_text > m_comment
										&& !unc_isspace(b->m_text[-1]))
									{
										/* function call! */
										UNC_ASSERT(b->m_leading_whitespace_length == 0
												&& !b->m_is_first_on_line);
										UNC_ASSERT(i > 0 ? words[i-1].m_trailing_whitespace_length == 0 : 1);

										if (i > 0)
										{
											i--;
											b = &words[i];

											if (!b->m_is_punctuation
												&& !b->m_is_bullet
												&& b->m_word_length > 0)
											{
												b->m_keep_with_next = true;
											}

											if (!the_end)
											{
												i++;
											}
										}
									}
									break;
								}

								if (the_end)
								{
									/* end of function arg set / array index / code chunk found. */
									start_of_func_call = i;
									break;
								}
							}
							else if (b->m_line_count >= 2 && !b->m_is_math && !b->m_is_code && !master_is_code)
							{
								/* ABORT scan when an empty line == double newline is seen, unless we're in a code chunk! */
								break;
							}
						}

						if (start_of_func_call >= 0)
						{
							/*
							When the matching brace happens to be BEFORE our current start position, then
							we've hit the end of another level of braces and hence we need to rescan from
							the /new/ start position to make sure we've got the entire range!
							*/
							if (start_of_func_call < start_idx)
							{
								scan = true;
								start_idx = start_of_func_call;
								box_idx = start_idx;
								continue;
							}
						}
						break;
					}
					else if (b->m_is_math)
					{
						rh_reqd = !!(box->m_math_operator & MO_TEST_RH_REQD);
						continue;
					}
					else if (b->m_line_count >= 2 && !b->m_is_math && !b->m_is_code /* && !master_is_code */ )
					{
						/* ABORT scan when an empty line == double newline is seen, unless it's already part of a code chunk! */
						break;
					}
					else if (rh_reqd)
					{
						rh_reqd = false;
						continue;
					}
					else if (master_is_code)
					{
						rh_reqd = false;

						if (b->m_is_code)
						{
							continue;
						}

						const chunk_tag_t *t = find_keyword(b->m_text, b->m_word_length);

						if (t)
						{
							bool in_pp = comment_is_part_of_preproc_macro();
							bool pp_iter = ((t->lang_flags & FLAG_PP) != 0);

							if (((cpd.lang_flags & t->lang_flags) != 0)
								|| (in_pp && pp_iter))
							{
								/*
								reserved word for this language; this of course assumes any 'code'
								in the comments is the /same/ language as the source code itself!
								*/
								continue;
							}
						}
					}
					break;
				}
				end_idx--;

				{

					int i;

					/* mark the entire range as 'is_math' */
				for (i = start_idx; i <= end_idx; i++)
				{
					reflow_box *b = &words[i];

					master_is_math |= b->m_is_math;
					master_is_code |= b->m_is_code;
				}

				for (i = start_idx; i <= end_idx; i++)
				{
					reflow_box *b = &words[i];

					b->m_is_math |= master_is_math;
					b->m_is_code |= master_is_code;

					/* RESET any possible 'bullet' mark for the math expression operators/elements */
					b->m_is_bullet = false;
				}
				}

				box_idx = start_idx - 1;
			}

#if 0
			/*
			adjust line break priority as well, as we prefer to keep math and code chunks together:
			*/
			for (i = start_idx + 1; i < end_idx; i++)
			{
				reflow_box *b = &words[i];

				b->m_left_priority += (master_is_math * 20) + (master_is_code * 30);
				b->m_right_priority += (master_is_math * 20) + (master_is_code * 30);
			}
			words[end_idx].left_priority += (master_is_math * 20) + (master_is_code * 30);
			words[start_idx].right_priority += (master_is_math * 20) + (master_is_code * 30);
#endif
		}
	}
}








/**
Fixup the paragraph tree:

1) any node with children must have those
   children span the entire box range of the parent. When not, make sure to fill
the gaps with additional children, so that the series of children
always spans the box range of their parent.

2) Propagate some settings from parent to first child and from last child to
   parent, where applicable.

3a) XHTML paragraphs are only /really/ XHTML when they are enclosed by an XML open+end
    tag set.

3b) XHTML paragraphs MAY be HTML paragraphs instead when they start with an 'unclosed'
    XML take on a new line; the occurrence of one or more (unclosed) XML tags in
	a larger text paragraph don't make the entire thing a [X]HTML para!
*/
void cmt_reflow::fixup_paragraph_tree(paragraph_box *para)
{
	UNC_ASSERT(para);
	UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
	UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
	UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
	UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
	//UNC_ASSERT(para->m_parent && !para->m_previous_sibling ? para->m_first_box == para->m_parent->m_first_box : 1);
	//UNC_ASSERT(para->m_parent && !para->m_next_sibling ? para->m_last_box == para->m_parent->m_last_box : 1);

	/*
	gaps can be due to chunks of XML somewhere in the text, etc.
	so gaps can exist at the start, middle and end of the box sequence.
	*/

	paragraph_box *child = para->m_first_child;
	int first_box_idx = para->m_first_box;

	int gap_start = -1;
	int gap_end = -1;
	paragraph_box *prev_child = NULL;

	while (child)
	{
		UNC_ASSERT(child->m_first_box >= first_box_idx);
		UNC_ASSERT(child->m_previous_sibling ? child->m_previous_sibling->m_last_box < child->m_first_box : 1);
		UNC_ASSERT(child->m_next_sibling ? child->m_next_sibling->m_first_box > child->m_last_box : 1);
		UNC_ASSERT(child->m_parent ? child->m_first_box >= child->m_parent->m_first_box : 1);
		UNC_ASSERT(child->m_parent ? child->m_last_box <= child->m_parent->m_last_box : 1);

		int plug_gap = 0;

		if (child->m_first_box > first_box_idx)
		{
			plug_gap = -1;

			gap_start = first_box_idx;
			gap_end = child->m_first_box - 1;
		}
		else if (child->m_next_sibling
				&& child->m_next_sibling->m_first_box != child->m_last_box + 1)
		{
			/*
			gap between two child nodes
			*/
			plug_gap = 2;

			gap_start = child->m_last_box + 1;
			prev_child = child;
			child = child->m_next_sibling;
			gap_end = child->m_first_box - 1;
		}
		else if (!child->m_next_sibling
			&& para->m_last_box != child->m_last_box)
		{
			plug_gap = 1;

			gap_start = child->m_last_box + 1;
			prev_child = child;
			child = NULL;
			gap_end = para->m_last_box;
		}

		if (plug_gap == 0)
		{
			if (!child->m_previous_sibling)
			{
				/*
				first child; aligned with parent.

				Now make sure child has the same settings as parent -- at least for some tidbits!
				*/
				UNC_ASSERT(child->m_first_box == first_box_idx);
				UNC_ASSERT(para->m_first_box == first_box_idx);

				int lc = max(para->m_min_required_linebreak_before, child->m_min_required_linebreak_before);
				para->m_min_required_linebreak_before = child->m_min_required_linebreak_before = lc;

				int lw = max(para->m_leading_whitespace_length, child->m_leading_whitespace_length);
				para->m_leading_whitespace_length = child->m_leading_whitespace_length = lw;
			}
			else
			{
				/*
				next/last child; aligned with previous child.

				Now make sure child edges have the same settings as previous child -- at least for some tidbits!
				*/
				UNC_ASSERT(prev_child == child->m_previous_sibling);
				UNC_ASSERT(child->m_first_box == child->m_previous_sibling->m_last_box + 1);

				int lc = max(prev_child->m_min_required_linebreak_after, child->m_min_required_linebreak_before);
				prev_child->m_min_required_linebreak_after = child->m_min_required_linebreak_before = lc;

				int lw = max(prev_child->m_trailing_whitespace_length, child->m_leading_whitespace_length);
				prev_child->m_trailing_whitespace_length = child->m_leading_whitespace_length = lw;
			}

			/*
			If this is an XML/XHTML tag, check if it is an UNCLOSED tag: if it is, we're apparently
			coping with HTML like this

				<ul>
					<li>bla 1
					<li>bla 1
					<li>bla 1
				</ul>

			where the <li>'s will be marked unclosed: those should be SIBLINGS instead of CHILDS of each other: hence we
			must adjust the paragraph tree to fold those children into becoming siblings.
			*/
			if (child->m_is_xhtml
				&& child->m_is_unclosed_html_tag)
			{

			}

			fixup_paragraph_tree(child);

			first_box_idx = child->m_last_box + 1;
		}
		else
		{
			UNC_ASSERT(plug_gap);

			/* create a new child to fill the gap */
			paragraph_box *newp = new paragraph_box();

			/* copy parent settings et al */
			*newp = *para;

			/* adjust some stuff */
			newp->m_first_child = NULL;
			newp->m_first_box = gap_start;
			newp->m_last_box = gap_end;
			UNC_ASSERT(newp->m_last_box >= newp->m_first_box);
			newp->m_parent = para;

			newp->m_previous_sibling = prev_child;
			newp->m_next_sibling = child;
			//UNC_ASSERT(newp->m_next_sibling != newp->m_first_child);
			if (child)
			{
				UNC_ASSERT(newp->m_previous_sibling == child->m_previous_sibling);
				child->m_previous_sibling = newp;

				newp->m_trailing_whitespace_length = 0;
				newp->m_min_required_linebreak_after = 0;
			}
			if (prev_child)
			{
				UNC_ASSERT(newp->m_next_sibling == prev_child->m_next_sibling);
				prev_child->m_next_sibling = newp;

				newp->m_leading_whitespace_length = 0;
				newp->m_min_required_linebreak_before = 0;
			}
			else
			{
				UNC_ASSERT(para->m_first_child == child);
				para->m_first_child = newp;
			}
			if (newp->m_previous_sibling)
			{
				UNC_ASSERT(para->m_first_child != child);
				UNC_ASSERT(para->m_first_child != newp);
				UNC_ASSERT(newp->m_previous_sibling->m_next_sibling == newp);
			}
			else
			{
				UNC_ASSERT(para->m_first_child == newp);
			}
			UNC_ASSERT(newp->m_previous_sibling ? newp->m_previous_sibling->m_last_box < newp->m_first_box : 1);
			UNC_ASSERT(newp->m_next_sibling ? newp->m_next_sibling->m_first_box > newp->m_last_box : 1);
			UNC_ASSERT(newp->m_first_box >= newp->m_parent->m_first_box);
			UNC_ASSERT(newp->m_last_box <= newp->m_parent->m_last_box);

			if (para->m_xhtml_start_tag_container == para
				&& para->m_xhtml_start_tag_box >= newp->m_first_box
				&& para->m_xhtml_start_tag_box <= newp->m_last_box)
			{
				para->m_xhtml_start_tag_container = newp;
				newp->m_xhtml_start_tag_container = newp;
				UNC_ASSERT(newp->m_is_xhtml);

				/* update subtree as well */
				paragraph_box *xmlnode = para->m_first_child;

				while (xmlnode)
				{
					if (xmlnode->m_xhtml_start_tag_container == para
						&& xmlnode->m_xhtml_start_tag_box >= newp->m_first_box
						&& xmlnode->m_xhtml_start_tag_box <= newp->m_last_box)
					{
						xmlnode->m_xhtml_start_tag_container = newp;
					}

					if (xmlnode->m_first_child)
					{
						xmlnode = xmlnode->m_first_child;
					}
					else if (xmlnode->m_next_sibling)
					{
						xmlnode = xmlnode->m_next_sibling;
					}
					else if (xmlnode->m_parent && xmlnode->m_parent != para)
					{
						xmlnode = xmlnode->m_parent->m_next_sibling;
					}
					else
					{
						break;
					}
				}
			}

			if (para->m_xhtml_end_tag_container == para
				&& para->m_xhtml_end_tag_box >= newp->m_first_box
				&& para->m_xhtml_end_tag_box <= newp->m_last_box)
			{
				para->m_xhtml_end_tag_container = newp;
				newp->m_xhtml_end_tag_container = newp;

				/* update subtree as well */
				paragraph_box *xmlnode = para->m_first_child;

				while (xmlnode)
				{
					if (xmlnode->m_xhtml_end_tag_container == para
						&& xmlnode->m_xhtml_end_tag_box >= newp->m_first_box
						&& xmlnode->m_xhtml_end_tag_box <= newp->m_last_box)
					{
						xmlnode->m_xhtml_end_tag_container = newp;
					}

					if (xmlnode->m_first_child)
					{
						xmlnode = xmlnode->m_first_child;
					}
					else if (xmlnode->m_next_sibling)
					{
						xmlnode = xmlnode->m_next_sibling;
					}
					else if (xmlnode->m_parent && xmlnode->m_parent != para)
					{
						xmlnode = xmlnode->m_parent->m_next_sibling;
					}
					else
					{
						break;
					}
				}
			}

			/* local restart: check the new child or rewind to the previous_child of the new node! */
			child = newp;
			if (plug_gap > 0)
			{
				child = child->m_previous_sibling;
			}
			prev_child = child->m_previous_sibling;
			UNC_ASSERT(child->m_first_box == first_box_idx);
			continue;
		}

		prev_child = child;
		child = child->m_next_sibling;
	}
}






/*
Scan the 'words' (the atomic text boxes) and detect the 'paragraph' hierarchy; store
this hierarchy in the paragraph_collection as a tree, ready for traversal.
*/
int cmt_reflow::grok_the_words(paragraph_box *root, words_collection &words)
{
	UNC_ASSERT(root);
	UNC_ASSERT(words.count() >= 2);

	/* let it span the entire text */
	root->m_first_box = 0;
	root->m_last_box = (int)words.count() - 1; // INclusive boundary!
	UNC_ASSERT(root->m_last_box >= root->m_first_box);
	UNC_ASSERT(words[root->m_last_box].m_word_length == 0);
	UNC_ASSERT(words[root->m_last_box].m_line_count == 0);
	//root->m_last_box--;
	UNC_ASSERT(root->m_last_box >= root->m_first_box);

	/*
	also make sure the root paragraph has leading and trailing mandatory newlines when
	the comment format requires them:
	*/
	if (m_has_leading_and_trailing_nl)
	{
		root->m_min_required_linebreak_before = 1;
		root->m_min_required_linebreak_after = 1;

		/*
		also make sure these newlines end up in the box collective:
		patch the first printable empty box at the start and the
		last printable empty box at the end.
		*/
		int i;
		for (i = 0; i <= root->m_last_box; i++)
		{
			reflow_box *box = &words[i];

			if (box->m_do_not_print)
				continue;

			if (box->m_line_count < root->m_min_required_linebreak_before)
			{
				if (box->m_word_length != 0)
				{
					// patch the box #0: it is meant for such fixes as these.
					box = &words[0];
					UNC_ASSERT(box->m_do_not_print);
					box->m_do_not_print = false;
				}
				UNC_ASSERT(box->m_text);
				box->m_line_count = root->m_min_required_linebreak_before;
			}
			break;
		}
		reflow_box *box_at_eoc = NULL;
		for (i = root->m_last_box; i > 0; i--)
		{
			reflow_box *box = &words[i];

			if (box->m_do_not_print)
			{
				box_at_eoc = box;
				continue;
			}

			if (box->m_word_length == 0 && box->m_line_count == 0)
			{
				box_at_eoc = box;
				continue;
			}

			UNC_ASSERT(box_at_eoc);
			UNC_ASSERT(box_at_eoc->m_text);
			box = box_at_eoc;
			if (box->m_line_count < root->m_min_required_linebreak_after)
			{
				box->m_do_not_print = false;
				box->m_line_count = root->m_min_required_linebreak_after;
			}
			break;
		}
	}

	/*
	done; now scan the words collection; this is a lightly recursive,
	top-down/bottom-up process, which starts
	top-down by detecting the major text sections, after which
	each section is further divided into
	graphics, lists, paragraphs, etc.

	We can start the scan in a sequential fashion as we start to look at the
	'paragraphs of text'. Depending on what we find, we'll take it from there.
	*/
	if (m_reflow_mode == 1)
	{
		/* do not split the one 'paragraph' in many sub-paragraphs! */
		root->m_is_non_reflowable = true;
	}
	else
	{
		/* first make sure math expressions etc. are marked as such in their entirety */
		expand_math_et_al_markers(words);

		/* next, combine the words into 'paragraphs', recursively */
		int dnl = 0;
		int next_elem = find_the_paragraph_boundaries(root, words, 0, dnl);
		UNC_ASSERT(next_elem + 1 == (int)words.count());

		fixup_paragraph_tree(root);

		/*
		The recognition engine... see if this is:

		- a doxygen/javadoc tag (which, by being found here, starts a fresh paragraph)

		- a non-reflowable chunk of text

		- a chunk of XML/HTML text?

		- a math/punctuation/other non-word 'word'; maybe part of a 'graphic element' which must be
		  marked non-reflowable from start to end?

		- a bullet item?

		- a regular word which starts a new paragraph of text?
		*/

		/*
		TODO: reflow the paragraphs --> set the para leadin/leadout, [hanging] indent, and the
			  line_Count for the boxes (removes the line_count values that were before)
		*/
		int level = 0;
		//int deferred_whitespace;
		//int deferred_nl = 0;
		//int width = m_line_wrap_column;
		paragraph_box *para = root;
		paragraph_box *parent = root;

		// deferred_whitespace = write2out_comment_start(para, words);
		//deferred_whitespace = m_extra_post_star_indent;

		UNC_ASSERT(parent == (para->m_parent ? para->m_parent : para));
		UNC_ASSERT(parent == (para->m_parent ? para->m_parent : root));

		/*
		in-order leaf traversal of the paragraph tree
		*/
		while (para)
		{
			if (para->m_first_child)
			{
				level++;
				parent = para;
				para = para->m_first_child;

				UNC_ASSERT(parent == (para->m_parent ? para->m_parent : para));
				UNC_ASSERT(parent == (para->m_parent ? para->m_parent : root));

				continue;
			}
			else
			{
				int i;

#if 0
				if (mandatory_deferred_nl < para->m_min_required_linebreak_before)
				{
					mandatory_deferred_nl = para->m_min_required_linebreak_before;
				}
#endif

				if (!para->m_is_non_reflowable)
				{
					/*
					First make sure the paragraph is REALLY reflowable: it is not when
					it contains only non-reflowable items.

					Graphics and ASCII art items have already marked the paragraph as non-reflowable
					before, when the user settings are set that way.

					Also, paragraphs which exist entirely of 'code' or 'math' are to be considered
					non-reflowable.

					So we only have to check here whether any printable boxes are reflowable...
					*/
					para->m_is_non_reflowable = true; /* provisional setting */
					bool is_math = true;
					bool is_code = true;

					for (i = para->m_first_box; i <= para->m_last_box; i++)
					{
						UNC_ASSERT(i >= 0);
						UNC_ASSERT(i < (int)words.count());

						reflow_box *box = &words[i];

						if (box->m_do_not_print)
							continue;

						if (box->m_word_length == 0)
							continue;

						if (!box->m_is_non_reflowable)
						{
							para->m_is_non_reflowable = false;
							//break; -- keep scanning: math / code
						}
						if (!box->m_is_code)
						{
							is_code = false;
						}
						if (!box->m_is_math)
						{
							is_math = false;
						}
					}

					if (is_math || is_code)
					{
						para->m_is_non_reflowable = true;
					}

					if (!para->m_is_non_reflowable)
					{
						/*
						This is an essentially REFLOWABLE paragraph.

						So we'd better perform our magic here: these paragraphs MAY have leading and/or
						trailing newlines (they MAY NOT, in which case they're special 'paragraphs' acting
						as parts of a larger one -- better pray that doesn't happen to us).
						*/
						int start_of_first_line = -1;
						for (i = para->m_first_box; i <= para->m_last_box; i++)
						{
							UNC_ASSERT(i >= 0);
							UNC_ASSERT(i < (int)words.count());

							reflow_box *box = &words[i];

							if (box->m_do_not_print)
								continue;

							if (box->m_word_length == 0)
								continue;

							if (box->m_is_first_on_line)
							{
								start_of_first_line = i;
							}
							break;
						}
						/*
						multi-line comments have hanging indent (even when the hanging indent is zero ;-) )
						*/
						int start_of_second_line = -1;
						if (start_of_first_line >= 0)
						{
							for (i = start_of_first_line + 1; i <= para->m_last_box; i++)
							{
								UNC_ASSERT(i >= 0);
								UNC_ASSERT(i < (int)words.count());

								reflow_box *box = &words[i];

								if (box->m_do_not_print)
									continue;

								if (box->m_word_length == 0 || !box->m_is_first_on_line)
									continue;

								start_of_second_line = i;
								break;
							}
						}

						if (start_of_first_line < 0)
						{
							para->m_indent_as_previous = true;
							UNC_ASSERT(para->m_starts_on_new_line == false);

							paragraph_box *prev = para->m_previous_sibling;

							if (!prev)
							{
								prev = para->m_parent;
								UNC_ASSERT(prev);
								UNC_ASSERT(para->m_parent == parent);
							}
							para->m_first_line_indent = prev->m_first_line_indent;
							para->m_hanging_indent = prev->m_hanging_indent;
						}
						else
						{
							reflow_box *box = &words[start_of_first_line];

							para->m_first_line_indent = box->m_leading_whitespace_length;
							para->m_starts_on_new_line = true;
						}
						if (start_of_second_line < 0)
						{
							para->m_hanging_indent = para->m_first_line_indent;
						}
						else
						{
							para->m_hanging_indent = words[start_of_second_line].m_leading_whitespace_length;
						}

						//reflow_para(para, words, width, deferred_whitespace, deferred_nl);
					}
				}
			}

			UNC_ASSERT(parent == (para->m_parent ? para->m_parent : para));
			UNC_ASSERT(parent == (para->m_parent ? para->m_parent : root));

			/* when no more sibling, then traverse up the tree and go to the next sibling there. */
			while (!para->m_next_sibling && para->m_parent)
			{
				para = para->m_parent;
				if (para && para->m_parent)
					parent = para->m_parent;
				level--;
			}
			para = para->m_next_sibling;

			UNC_ASSERT(para ? parent == (para->m_parent ? para->m_parent : para) : 1);
			UNC_ASSERT(para ? parent == (para->m_parent ? para->m_parent : root) : 1);
		}

		UNC_ASSERT(parent == root);
		UNC_ASSERT(level == 0);
	}

	return 0;
}





/**
 adjust the last_box for the paragraph and any children, if there are any
*/
void cmt_reflow::adjust_para_last_box(paragraph_box *para, int pos)
{
	for (;;)
	{
		UNC_ASSERT(para);
		UNC_ASSERT(pos >= para->m_first_box);
		UNC_ASSERT(pos <= para->m_last_box);

		para->m_last_box = pos;
		para = para->m_first_child;
		if (!para)
			break;
		while (para->m_next_sibling)
			para = para->m_next_sibling;
		//para = para->m_previous_sibling;
		UNC_ASSERT(para); // 'prev' linked list must be cyclic
		UNC_ASSERT(pos >= para->m_first_box);
		if (pos > para->m_last_box)
			break;
	}
}



int cmt_reflow::skip_tailing_newline_box(paragraph_box *para, words_collection &words, int box_idx, int min_nl_count, int &deferred_newlines)
{
	UNC_ASSERT(box_idx >= 0);
	UNC_ASSERT(box_idx < (int)words.count());
	reflow_box *box = &words[box_idx];
	//UNC_ASSERT(box->m_word_length > 0);

	for (box_idx++; box_idx <= para->m_last_box; box_idx++)
	{
		UNC_ASSERT(box_idx >= 0);
		UNC_ASSERT(box_idx < (int)words.count());
		box = &words[box_idx];

		if (box->m_do_not_print)
			continue;

		/* is this a newline box? mark end of para as having dangling mandatory newlines! */
		if (box->m_line_count >= min_nl_count)
		{
			UNC_ASSERT(deferred_newlines == 0);
			deferred_newlines = box->m_line_count;
			UNC_ASSERT(box->m_word_length == 0);
			box_idx++;
		}
		break;
	}
	box_idx--;

	return box_idx;
}


/**
 'major paragraphs' are identified by them being separated by at least 2 newlines.

 This is one of the simplest paragraph detection codes.

 One peculiarity should be noted here: this stage really performs TWO tasks:

 1) the simple chunking of major text sections, and

 2) the detection and 'flood-expanding' of non-reflow text chunks, such as
    non-reflowable 'boxed texts' and 'graphical elements' (ASCII art).

 Since the second is the most important to get right from the get go, it is done first.
*/
int cmt_reflow::find_the_paragraph_boundaries(paragraph_box *parent, words_collection &words, int box_start_idx, int &deferred_newlines)
{
	paragraph_box *para = new paragraph_box();

	/* copy parent settings et al */
	//*para = *parent;

	/* adjust some stuff */
	para->m_first_child = NULL;
	para->m_first_box = box_start_idx;
	para->m_last_box = parent->m_last_box;
	UNC_ASSERT(para->m_last_box >= para->m_first_box);

	para->m_parent = parent;
	UNC_ASSERT(!para->m_previous_sibling);
	UNC_ASSERT(!para->m_next_sibling);
	UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
	UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

	if (!parent->m_first_child)
	{
		parent->m_first_child = para;
		UNC_ASSERT(parent->m_next_sibling != parent->m_first_child);
	}
	else
	{
		paragraph_box *sibling = parent->m_first_child;

		while (sibling->m_next_sibling)
		{
			sibling = sibling->m_next_sibling;
		}

		sibling->m_next_sibling = para;
		UNC_ASSERT(sibling->m_next_sibling != sibling->m_first_child);
		para->m_previous_sibling = sibling;
	}
	UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
	UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);

	if (deferred_newlines)
	{
		UNC_ASSERT(para->m_first_box != parent->m_first_box);
		UNC_ASSERT(parent->m_is_xhtml ? deferred_newlines >= 0 : deferred_newlines >= 1);
		if (para->m_previous_sibling)
		{
			para->m_previous_sibling->m_min_required_linebreak_after = deferred_newlines;
		}
		para->m_min_required_linebreak_before = deferred_newlines;
		deferred_newlines = 0;
	}

	int graph_char_tally = 0;
	int graph_word_idx = -1;

	int nonreflow_char_tally = 0;
	int nonreflow_word_idx = -1;

	int indent = -1;

	bool create_deferred_sibling = false;

	int box_idx;

	UNC_ASSERT(para->m_last_box < (int)words.count());
	UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < box_start_idx : 1);
	for (box_idx = box_start_idx; box_idx <= para->m_last_box; box_idx++)
	{
		UNC_ASSERT(box_idx >= 0);
		reflow_box *box = &words[box_idx];

		if (box->m_do_not_print)
			continue;

		if (create_deferred_sibling)
		{
			/* create a DEFERRED sibling to store the next chunk */
			paragraph_box *next_para = new paragraph_box();

			/* copy parent settings et al, NOT sibling settings! */
			//*next_para = *parent;

			/* adjust some stuff */
			next_para->m_first_child = NULL;
			next_para->m_first_box = box_idx;
			next_para->m_last_box = para->m_last_box;
			UNC_ASSERT(next_para->m_last_box >= next_para->m_first_box);
			next_para->m_parent = parent;

			UNC_ASSERT(parent->m_first_child);
			para->m_next_sibling = next_para;
			UNC_ASSERT(para->m_next_sibling != para->m_first_child);
			next_para->m_previous_sibling = para;

			adjust_para_last_box(para, box_idx - 1);
			UNC_ASSERT(para->m_last_box >= para->m_first_box);

			UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
			UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
			UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
			UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

			UNC_ASSERT(para->m_is_xhtml ? deferred_newlines >= 0 : deferred_newlines >= 1);
			para->m_min_required_linebreak_after = deferred_newlines;
			next_para->m_min_required_linebreak_before = deferred_newlines;
			deferred_newlines = 0;

			para = next_para;

			UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
			UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
			UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
			UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

			/* and flush ASCII art and boxed text tallies, etc. */
			graph_char_tally = 0;
			graph_word_idx = -1;

			nonreflow_char_tally = 0;
			nonreflow_word_idx = -1;

			create_deferred_sibling = false;
		}

		/*
		non-reflowable boxes are a bit of a special case: when we encounter them by this time,
		we are ALMOST sure these span entire lines (one or more). (tag-bracketed non-reflow chunks MAY
		be PART of a line OR span multiple lines, while STILL being part of the enveloping, reflowable, paragraph.

		What we DO know is such a non-reflowable item is COMPLETE: the
		initial box chopping/parsing stage of the process may have identified a few non-reflow
		lines which form part of a larger entity, which should be treated as a single
		non-reflowable item as a whole.
		UNLESS, that is, we're talking about 'derivatives', i.e. non-reflow sections which are marked
		non-reflowable due to them being graphics or ASCII art, combined with particular user configuration settings
		permitting us to reflow such chunks, or not. We might not yet have discovered
		the section under scrutiny should be non-reflowable then: this can happen when
		the configuration tells us boxed texts, which match certain statistical criteria, are
		to be treated as non-reflowable.

		And then there's the math expressions
		which may not be allowed to be reflown, depending
		on yet another piece of configuration. But those (and code chunks), we cover by allowing them to exist
		within otherwise reflowable paragraphs; the same goes for tag-bracketed non-reflowable parts of otherwise
		reflowable paragraphs: we choose this path as we'd otherwise end up with reflowable paragraphs
		becoming bloody hard to reflow as they would otherwise no longer be contained within a single paragraph
		object. It's easier to 'ignore' non-reflowable parts in there by treating them as a kind of 'single word' in
		the reflow calculations.



		Process:

		track previous dual newline; scan forward to next dual newline.

		count number of 'graphical chars' in that section according to the config params and
		when the threshold is reached, the entire block is marked down as non-reflowable.
		*/

		/* is this a non-reflowable box? mark its position! */
		if (box->m_is_non_reflowable)
		{
			if (box->m_floodfill_non_reflow)
			{
				para->m_is_non_reflowable = true;
				//if (para->m_nonreflow_trigger_box < 0)
				//{
				para->m_nonreflow_trigger_box = box_idx; /* override possible other trigger box; this one is the most important anyhow. */
				//}
				continue;
			}
			else
			{
				/*
				mark paragraph as POTENTIALLY non-reflowable: it IS non-reflowable
				once we discover the paragraph contains only non-reflowable boxes.
				*/
				//para->m_potentially_non_reflowable = true;
				//para->m_nonreflow_trigger_box = box_idx;
			}
		}

		/* is this an already-detected (non-reflowable?) boxed text? if so, mark it as such! */
		if (box->m_is_part_of_boxed_txt)
		{
			para->m_is_boxed_txt = true;

			if (!m_cmt_reflow_box)
			{
				para->m_is_non_reflowable = true;
				if (para->m_nonreflow_trigger_box < 0)
				{
					para->m_nonreflow_trigger_box = box_idx;
				}
				continue;
			}
		}

		/*
		XML/HTML tags MAY imply non-reflowable major paragraph

		Warning: the code must be able to cope with stuff like '<div><h1>X <b>Y</b> Z</h1><p>abc</p></div>' i.e.
		nested XML/HTML tags. unclosed tags should be mentioned, but recovered from graciously, so we can
		handle HTML-formatted text too, though that does not require matching closing tags a la XML/XHTML.
		*/
		if (box->m_is_xhtml_start_tag)
		{
			if (box_idx != para->m_first_box)
			{
				/* create a sibling to store the new (probable) XML/XHTML node */
				paragraph_box *xml_para = new paragraph_box();

				/* copy sibling settings et al */
				//*xml_para = *para;

				/* adjust some stuff */
				xml_para->m_first_child = NULL;
				xml_para->m_first_box = box_idx;
				xml_para->m_last_box = para->m_last_box;
				UNC_ASSERT(xml_para->m_last_box >= xml_para->m_first_box);
				xml_para->m_parent = parent;

				UNC_ASSERT(parent->m_first_child);
				para->m_next_sibling = xml_para;
				UNC_ASSERT(para->m_next_sibling != para->m_first_child);
				xml_para->m_previous_sibling = para;

				/* terminate the old one */
				adjust_para_last_box(para, box_idx - 1);
				UNC_ASSERT(para->m_last_box >= para->m_first_box);

				para->m_min_required_linebreak_after = deferred_newlines;
				xml_para->m_min_required_linebreak_before = deferred_newlines;
				deferred_newlines = 0;

				/* and flush ASCII art and boxed text tallies, etc. */
				graph_char_tally = 0;
				graph_word_idx = -1;

				nonreflow_char_tally = 0;
				nonreflow_word_idx = -1;

				UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
				UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

				para = xml_para;

				UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
				UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
			}

			para->m_xhtml_start_tag_box = box_idx;
			para->m_xhtml_start_tag_container = para;
			para->m_is_xhtml = true;
			UNC_ASSERT(para->m_is_unclosed_html_tag == false);
			UNC_ASSERT(para->m_xhtml_end_tag_box == -1);
			UNC_ASSERT(para->m_xhtml_end_tag_container == NULL);

			/*
			Now we need to take care of split parts of the tag, i.e. where the tag contains attributes and is
			possibly spread across multiple lines.
			*/
			int part_end_box_idx = box_idx;
			reflow_box *part_end_box = box;

			if (box->m_is_xhtml_tag_part)
			{
				UNC_ASSERT(box->m_xhtml_tag_part_begin < box->m_xhtml_tag_part_end);
				UNC_ASSERT(box_idx == box->m_xhtml_tag_part_begin);

				part_end_box_idx = box->m_xhtml_tag_part_end;
				part_end_box = &words[part_end_box_idx];
			}

			UNC_ASSERT(part_end_box_idx != box_idx ? part_end_box->m_is_xhtml_tag_part : 1);
			if (part_end_box->m_is_xhtml_end_tag)
			{
				/*
				A <x/> tag i.e. a tag which open and closed at the same time.
				*/
				box->m_xhtml_matching_start_tag = box_idx;
				box->m_xhtml_matching_end_tag = part_end_box_idx;

				/* spread it across the start tag parts as well... */
				for (int idx = box_idx + 1; idx <= part_end_box_idx; idx++)
				{
					reflow_box *b = &words[idx];

					b->m_xhtml_matching_start_tag = box_idx;
					b->m_xhtml_matching_end_tag = part_end_box_idx;
				}

				UNC_ASSERT(para->m_is_unclosed_html_tag == false);
				para->m_xhtml_end_tag_box = part_end_box_idx;
				para->m_xhtml_end_tag_container = para;

				/*
				As this is a matching START+END tag, it's really the end of the current 'paragraph',
				so we'd better clone it into a sibling to prevent nasty surprises down the line.

				The nasty thing here is that we CAN create a kind of 'premature' sibling, but that
				would cause trouble when we're actually at the end of the current para chunk to
				inspect, so we either need to copy the stop condition OR defer the sibling creation.

				Since this happens in a few spots, the easiest choice is to DEFER here.
				*/

				deferred_newlines = 0;
				box_idx = skip_tailing_newline_box(para, words, part_end_box_idx, 1, deferred_newlines);

				create_deferred_sibling = true;
				continue;
			}
			else
			{
				/*
				WARNING: mark the START node as OPEN until we find a matching END tag!

				we initially assume it's a HTML start tag without matching end tag, e.g. these <li>'s in

				<ul>
					<li>x1
					<li>x2
				</ul>

				unless the subsequent piece of code will prove us wrong (by finding us a matching end tag).
				*/
				para->m_is_unclosed_html_tag = true;
				box->m_is_unclosed_xhtml_start_tag = true;

				UNC_ASSERT(para->m_first_box == box_idx);
				UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
				UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

				paragraph_box *xhtml_start_para = para;
				int part_start_box_idx = box_idx;
				//box_idx = end_box_idx;

				/* WARNING: start further scan beyond [multi-part] tag: */
				deferred_newlines = 0;
				box_idx = skip_tailing_newline_box(para, words, part_end_box_idx, 1, deferred_newlines);

				box_idx = find_the_paragraph_boundaries(para, words, box_idx + 1, deferred_newlines);

				reflow_box *end_box = &words[box_idx];

				/*
				we may need to unwind the paragraph tree to the xml-tag-matching parent in order to create
				proper tree organization.
				*/
				if (end_box->m_is_xhtml_end_tag && !end_box->m_is_unmatched_xhtml_end_tag)
				{
					/*
					the last scanned item is a matched end tag: we need to unwind to the matching parent/start tag. Are we that parent?
					*/
					if (end_box->m_xhtml_matching_start_tag == para->m_xhtml_start_tag_box)
					{
						/*
						Good Golly, that's a real matched pair, indeed! Happy thoughts!

						Note that this also means the last para next_sibling is the container for this
						end tag...
						*/
						UNC_ASSERT(para->m_is_unclosed_html_tag == false);
						UNC_ASSERT(para->m_xhtml_end_tag_box == box_idx);
						UNC_ASSERT(end_box->m_is_unmatched_xhtml_end_tag == false);
					}
					else
					{
						/*
						we're not there yet. Unwind...
						*/
						UNC_ASSERT(para->m_last_box <= box_idx - 1);

						return box_idx;
					}
				}
				else
				{
					/*
					hit the end in the subsection without ever seeing a matching end tag. bummer.

					Signal this state of affairs for the remainder of this code section:
					*/
					end_box = NULL;
				}

				UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
				UNC_ASSERT(para->m_parent ? box_idx >= para->m_parent->m_first_box : 1);
				UNC_ASSERT(para->m_parent ? box_idx <= para->m_parent->m_last_box : 1);
				UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
				UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
				UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

				/*
				make sure we update our 'para' to point at the latest sibling that
				might be generated in there, before we go on scanning...
				*/
				while (para->m_next_sibling)
				{
					UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
					UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
					UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
					UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

					para = para->m_next_sibling;

					UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
					UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
					UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
					UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
				}

				if (end_box)
				{
					/*
					We have a matching start and end xml/xhtml tag section, possibly spanning multiple
					'paragraphs'; by now, it's time to update the start para and point it to the
					current para, which must be the container of the end tag box.
					*/
					UNC_ASSERT(para->m_first_box <= box_idx);
					UNC_ASSERT(para->m_last_box >= box_idx);

					xhtml_start_para->m_xhtml_end_tag_container = para;
					UNC_ASSERT(end_box->m_xhtml_matching_start_tag == part_start_box_idx);

					/* spread it across the start tag parts as well... */
					for (int idx = part_start_box_idx; idx <= part_end_box_idx; idx++)
					{
						reflow_box *b = &words[idx];

						b->m_xhtml_matching_start_tag = part_start_box_idx;
						b->m_xhtml_matching_end_tag = box_idx;
					}

					UNC_ASSERT(para->m_is_unclosed_html_tag == false);
					para->m_xhtml_end_tag_box = box_idx;
					para->m_xhtml_end_tag_container = para;

					if (xhtml_start_para != para)
					{
						para->m_xhtml_start_tag_box = xhtml_start_para->m_xhtml_start_tag_box;
						para->m_xhtml_start_tag_container = xhtml_start_para;

						UNC_ASSERT(para->m_is_unclosed_html_tag == false);
						UNC_ASSERT(xhtml_start_para->m_xhtml_end_tag_box == box_idx);
						para->m_xhtml_end_tag_box = box_idx;
						UNC_ASSERT(para->m_is_dangling_xhtml_close_tag == false);
					}
				}

				/*
				as that last sibling/para contains the section from START till END node
				(for we only return from the recursive call when either the matching
				END tag was located or we hit the very end of the entire content) we should
				make sure we don't damage that chunk, i.e. a deferred sibling generation is in order here.
				*/
				deferred_newlines = 0;
				box_idx = skip_tailing_newline_box(para, words, box_idx, 1, deferred_newlines);

				create_deferred_sibling = true;
				continue;
			}
		}

		if (box->m_is_xhtml_end_tag)
		{
			UNC_ASSERT(!box->m_is_xhtml_start_tag);

			/*
			Cope with HTML and nested XML tags graciously:
			scan the para tree where still-open tags are supposed to exist
			and match the inner-most case-INsensitive open tag: this is not good enough for XML,
			but mandatory for HTML and in actual practice there's very few occasions where you'll
			find XML with tags which only differ in case.

			Anyhow, even under such circumstances, our matching would be okay
			anyway when the XML was correctly formed as the innermost tag is matched first, so the hierarchy
			will remain as-it-was always; the only situation where we will guess wrong is with
			MALFORMED XML/XHTML, while our matching rules are A-okay for HTML under any conditions.

			And the 'guessing wrong' leads to nothing more than lingering 'open' start tags, which
			represents the erroneous format pretty well anyhow.
			*/
			UNC_ASSERT(box->m_xhtml_matching_end_tag == -1);
			paragraph_box *node;

			/* WARNING: mark the END node as OPEN until we find a matching START tag! */
			/* ignore this closing tag: DITCH IT? or make it 'literal'? --> choice: tag it as unmatched, later process rounds will determine what to do with that. */
			box->m_is_unmatched_xhtml_end_tag = true;
			UNC_ASSERT(para->m_is_xhtml != true);

			/* scan from inner to outer; pick first match */
			for (node = para; node; node = node->m_parent)
			{
				if (!node->m_is_xhtml)
					continue;

				UNC_ASSERT(node->m_xhtml_start_tag_box >= 0);
				reflow_box *elem = &words[node->m_xhtml_start_tag_box];
				if (elem->m_is_xhtml_start_tag
					&& !elem->m_is_xhtml_end_tag
					&& elem->m_xhtml_matching_end_tag < 0
					/* && elem->m_word_length - 1 == box->m_word_length - 2 */ )
				{
					const char *start_tag = box->m_text;
					const char *end_tag = elem->m_text;
					const char *se;
					const char *ee;

					UNC_ASSERT(start_tag);
					start_tag++;
					UNC_ASSERT(*start_tag == '/');
					start_tag++;
					se = strnchr_any(start_tag, " >", box->m_word_length - 2);
					UNC_ASSERT(end_tag);
					end_tag++;
					UNC_ASSERT(*end_tag != '/');
					ee = strnchr_any(end_tag, " >", elem->m_word_length - 1);

					UNC_ASSERT(se ? 1 : box->m_is_xhtml_tag_part);
					UNC_ASSERT(ee ? 1 : elem->m_is_xhtml_tag_part);
					if (!se)
					{
						se = start_tag + box->m_word_length - 2;
					}
					if (!ee)
					{
						ee = end_tag + elem->m_word_length - 1;
					}
					if (se - start_tag == ee - end_tag
						&& !strncmp(start_tag, end_tag, se - start_tag))
					{
						box->m_xhtml_matching_start_tag = node->m_xhtml_start_tag_box;
						box->m_is_unmatched_xhtml_end_tag = false;
						elem->m_xhtml_matching_end_tag = box_idx;
						elem->m_is_unclosed_xhtml_start_tag = false;

						/*
						when this is a matching END tag, it's really the end of the current 'paragraph',
						so we'd better clone it into a sibling to prevent nasty surprises down the line.

						The nasty thing here is that we CAN create a kind of 'premature' sibling, but that
						would cause trouble when we're actually at the end of the current para chunk to
						inspect, so we either need to copy the stop condition OR defer the sibling creation.

						Since this happens in a few spots, the easiest choice is to DEFER here.

						HOWEVER, we got here thanks to a recursive call, so we should return to caller
						and let 'em handle the remainder of the input.

						NOTE: unmatched tags are not part of any hierarchy, so they do NOT produce
							  deferred children. OTOH, unmatched START tags DO create siblings, as
							  those may be valid HTML hierarchy chunks.
						*/
						UNC_ASSERT(para->m_parent);

						UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
						UNC_ASSERT(para->m_parent ? box_idx >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? box_idx <= para->m_parent->m_last_box : 1);

						UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
						UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

						/*
						Now that we have a matching end tag, we can mark all XML children of that tree as UNCLOSED
						when they haven't been closed already.
						*/
						paragraph_box *p;

						for (p = para; p != node; p = p->m_parent)
						{
							UNC_ASSERT(p);
							if (p->m_is_xhtml)
							{
								UNC_ASSERT(p->m_xhtml_start_tag_box >= 0);
								reflow_box *pe = &words[p->m_xhtml_start_tag_box];
								if (pe->m_is_xhtml_start_tag
									&& !pe->m_is_xhtml_end_tag
									&& pe->m_xhtml_matching_end_tag < 0)
								{
									UNC_ASSERT(pe->m_is_unclosed_xhtml_start_tag == true);

									UNC_ASSERT(p->m_is_unclosed_html_tag == true);
									/* 're-use' the end marker fields to point at the ENCLOSING PARENT XML: END TAG */
									p->m_xhtml_end_tag_box = box_idx;
									p->m_xhtml_end_tag_container = node;

									/* ensure child XML bits end BEFORE the current PARENT close tag! */
									if (p->m_last_box >= box_idx)
									{
										p->m_last_box = box_idx - 1;
										UNC_ASSERT(p->m_first_box <= p->m_last_box);
									}
								}
							}

							/* ensure child XML bits end BEFORE the current PARENT close tag! */
							if (p->m_last_box >= box_idx)
							{
								p->m_last_box = box_idx - 1;
								UNC_ASSERT(p->m_first_box <= p->m_last_box);
							}
						}

						/*
						mark the end tag box and the matching parent para
						*/
						para->m_xhtml_end_tag_box = box_idx;
						para->m_xhtml_end_tag_container = node;
						para->m_xhtml_start_tag_box = node->m_xhtml_start_tag_box;
						UNC_ASSERT(node == node->m_xhtml_start_tag_container);
						UNC_ASSERT(para->m_xhtml_start_tag_container == NULL);
						para->m_xhtml_start_tag_container = node;
						para->m_is_unclosed_html_tag = false;
						UNC_ASSERT(para->m_is_dangling_xhtml_close_tag == false);

						node->m_xhtml_end_tag_box = box_idx;
						node->m_xhtml_end_tag_container = node;
						//node->m_xhtml_start_tag_box = node->m_xhtml_start_tag_box;
						UNC_ASSERT(node->m_xhtml_start_tag_container == node);
						node->m_is_unclosed_html_tag = false;
						UNC_ASSERT(node->m_is_dangling_xhtml_close_tag == false);

						/*
						gonna unwind to a parent; clip the end of this subsection now
						*/
						UNC_ASSERT(para->m_last_box == box_idx - 1);
						/* since the parents are XHTML, so are we now */
						para->m_is_xhtml = true;

						return box_idx;
					}
				}
			}

			/*
			when we get here, it means we've hit a end tag which does not have a matching start tag.

			Mark as such, and keep it in a 'paragraph' of its own. Do NOT roll up the para hierarchy, as we don't know
			about that.
			*/
			para->m_is_dangling_xhtml_close_tag = true;

			deferred_newlines = 0;
			box_idx = skip_tailing_newline_box(para, words, box_idx, 1, deferred_newlines);

			create_deferred_sibling = true;
			continue;
		}

		/*
		locate [almost] continuous runs of 'graphical' content:

		a 'graphical run' is assumed to be broken (reset) when the amount of non-graphical printable
		characters is larger than 50% for this run.
		*/
		if (box->m_word_length > 0)
		{
			int graph_count = 0;
			int nonreflow_count = 0;
			int print_count = 0;

			deferred_newlines = 0;

			count_graphics_nonreflow_and_printable_chars(box->m_text, box->m_word_length,
												  &graph_count,
												  &nonreflow_count,
												  &print_count);

			/*
			when the word consists of more than 50% graph or nonreflow characters, add those to the
			tally against which we check the threshold settings; when the word is larger than 2 characters
			and the ratio is below 50%, the tally will be reset to ZERO.
			*/
			if (graph_count >= (print_count + 1) / 2)
			{
				graph_char_tally += graph_count;
				if (graph_word_idx == -1)
				{
				   graph_word_idx = box_idx;
				}

				if (graph_char_tally >= m_cmt_reflow_graphics_threshold)
				{
					para->m_is_non_reflowable = true;
					if (para->m_nonreflow_trigger_box < 0)
					{
						para->m_nonreflow_trigger_box = graph_word_idx;
					}
					para->m_is_graphics = true;
					if (para->m_graphics_trigger_box < 0)
					{
						para->m_graphics_trigger_box = graph_word_idx;
					}
				}
			}
			else if (box->m_word_length >= 3)
			{
				graph_char_tally = 0;
				graph_word_idx = -1;
			}

			if (nonreflow_count >= (print_count + 1) / 2)
			{
				nonreflow_char_tally += nonreflow_count;
				if (nonreflow_word_idx == -1)
				{
					nonreflow_word_idx = box_idx;
				}

				if (nonreflow_char_tally >= m_cmt_reflow_box_threshold)
				{
					para->m_is_boxed_txt = true;
					para->m_is_non_reflowable = true;
					if (para->m_nonreflow_trigger_box < 0)
					{
						para->m_nonreflow_trigger_box = nonreflow_word_idx;
					}
				}
			}
			else if (box->m_word_length >= 3)
			{
				nonreflow_char_tally = 0;
				nonreflow_word_idx = -1;
			}


			/*
			track 'this line indent' and 'previous line indent' so we can have start + hanging
			indent for paragraph X available any time we need/want.

	tip: real hanging indent should have the first WORD lined up with the word on the previous line.
		 like this...

		 And not like
		   this ('this' doesn't line up with either 'and' or 'not', so it's probably a single-linebreak
		   'new paragraph' -- if the difference in indent is sufficiently large, e.g.
			 this, at delta=2, is probably just being messy, while
					this, at delta=7
			 is probably some sort of 'intermission', which should be treated as a paragraph so it doesn't reflow
			 across the indent jump. This might be generalized as not so much seperate paragraphs, but rather
			 forcing line breaks when the indent delta is large enough, which, when we all keep it in a single paragraph,
			 means we'll have to support more indent levels than just one 'hanging', i.e. one indent level
			 per line. Hmmmm.... may be the 'make it separate paras' is the smarter solution here.

			 This text
					for instance
			 might be construed as 3 paras, where the 3rd is indent-continuating from the 1st.


			 Meanwhile, this does not entirely solve my 'no-reflow' bother, but we're getting there.
			*/
			if (!para->m_is_non_reflowable
				&& !para->m_is_graphics)
			{
				/* only do this when we're not sitting inside a non-reflow area. */

				if (box->m_is_first_on_line)
				{
					if (indent < 0)
						indent = box->m_leading_whitespace_length;

					bool do_marker = false;
					int do_after_marker = -1;
					bool is_intermission = false;
					bool is_bullet = false;
					bool is_doxygen_tag = false;
					bool is_reqd_linebreak_in_para = false;

					UNC_ASSERT(box == &words[box_idx]);
					UNC_ASSERT(box->m_is_first_on_line);
					UNC_ASSERT(box->m_word_length > 0);

					if (abs(box->m_leading_whitespace_length - indent) >= m_cmt_reflow_intermission_indent_threshold)
					{
						/*
						This one also catches all sorts of hanging indents: filter those out right now...

						Assume we've hit a real intermission line (or set of lines), so we only need to discover whether this
						is a hanging indent instead.

						Which leads us to the question: what is an intermission, really?

						A: an intermission is one or more lines inside an otherwise continuing text, which are indented
						   such as to mimic, say, a HTML <blockquote>.
						   And how does that one, assuming there are no empty lines anywhere to unambiguously detect
						   paragraph breaks, differ from a hanging indent like the one we have right here, for example?
						   Simple.
						   An intermission is always followed by one or more lines which are aligned with the text (lines)
						   BEFORE the intermission. While a 'hanging indent' will 'stick around' until the very end
						   of the paragraph.
						*/
						do_marker = true;
						//deferred_newlines = 1;
						is_intermission = true;

						/*
						hence we now scan forward to see whether the current text section 'jumps back' after a few lines;
						if not, we've got a hanging indent. If it does, we've got an intermission.
						*/
						for (int i = box_idx + 1; i <= para->m_last_box; i++)
						{
							reflow_box *next_box = &words[i];

							if (next_box->m_do_not_print)
								continue;

							if (next_box->m_line_count > 1)
							{
								/* a definite end to this paragraph! */
								is_intermission = false;
								do_marker = false;
								UNC_ASSERT(deferred_newlines == 0);
								UNC_ASSERT(do_after_marker == -1);
								break;
							}

							if (!next_box->m_is_first_on_line)
								continue;

							/* does this line 'jump back' to the previous indent level? */
							if (indent == next_box->m_leading_whitespace_length)
							{
								/* bingo! intermission! And this is one box past the end of it. */
								do_after_marker = i;
								deferred_newlines = 1;
								break;
							}
							/*
							the only other tolerable indent is 'same as previous line' which equals
							'same as first line of intermission'. We DO support 'nested' intermissions;
							when we encounter different indentation levels
							than the two previously mentioned though, we ASSUME this is just a bunch of text that
							was entered in a hurry and needs some serious paragraph reflow action instead when those
							indent levels are in between both the 'regular' and the 'first intermission line'
							indentations.

							Yes, this is catering for personal habits.
							*/
							if (abs(next_box->m_leading_whitespace_length - indent) < m_cmt_reflow_intermission_indent_threshold)
							{
								/* messy comment! */
								is_intermission = false;
								do_marker = false;
								UNC_ASSERT(deferred_newlines == 0);
								UNC_ASSERT(do_after_marker == -1);
								break;
							}
						}
					}

					/*
					Keep formatting intact around doxygen/javadoc tags which exist on their own in a line, e.g.

					----------------------------
					@remark
					This is a comment
					----------------------------
					*/
					if (!do_marker)
					{
						UNC_ASSERT(do_after_marker == -1);
						UNC_ASSERT(box->m_is_first_on_line);
						UNC_ASSERT(box->m_word_length > 0);
						if (box->m_is_doxygen_tag && !box->m_is_inline_javadoc_tag)
						{
							do_marker = true;
							//deferred_newlines = 1;
							is_doxygen_tag = true;

							int i;

							for (i = box_idx + 1; i <= para->m_last_box; i++)
							{
								reflow_box *next_box = &words[i];

								if (next_box->m_do_not_print)
									continue;

								/*
								in-between boxes may not carry anything except whitespace and a single linebreak
								*/
								if (next_box->m_word_length > 0 /* non-whitespace box */ )
								{
									do_marker = false;
									is_doxygen_tag = false;
									break;
								}

								if (next_box->m_line_count > 1)
								{
									/* a definite end to this paragraph! */
									UNC_ASSERT(deferred_newlines == 0);
									UNC_ASSERT(do_after_marker == -1);
									break;
								}
								else if (next_box->m_line_count == 1)
								{
									do_after_marker = i + 1;
									deferred_newlines = 1;
									break;
								}
							}
						}
					}

					/*
					Keep formatting intact around doxygen/javadoc tags which
					are clustered together yet have each line/paragraph begin
					with a @tag, e.g.:

					----------------------------
					@note A note.
					@warning A longer warning spanning
					         multiple lines.
				    @note And another note.
					----------------------------
					*/
					if (!do_marker)
					{
						UNC_ASSERT(do_after_marker == -1);
						UNC_ASSERT(box->m_is_first_on_line);
						UNC_ASSERT(box->m_word_length > 0);
						if (box->m_is_doxygen_tag && !box->m_is_inline_javadoc_tag)
						{
							do_marker = true;
							//deferred_newlines = 1;
							is_doxygen_tag = true;

							int i;

							for (i = box_idx + 1; i <= para->m_last_box; i++)
							{
								reflow_box *next_box = &words[i];

								if (next_box->m_do_not_print)
									continue;

								if (next_box->m_line_count > 1)
								{
									/* a definite end to this paragraph! */
									UNC_ASSERT(deferred_newlines == 0);
									UNC_ASSERT(do_after_marker == -1);
									break;
								}

								if (next_box->m_is_first_on_line
									&& next_box->m_is_doxygen_tag
									&& !next_box->m_is_inline_javadoc_tag)
								{
									/*
									located another single/multiline doxygen comment. Break on that one.
								    */
									do_after_marker = i;
									deferred_newlines = 1;
									break;
								}
							}
						}
					}

					/*
					In order to construct bullet lists, we need to cut each bullet
					into a separate paragraph, at least.
					*/
					if (!do_marker)
					{
						UNC_ASSERT(do_after_marker == -1);
						UNC_ASSERT(box->m_is_first_on_line);
						UNC_ASSERT(box->m_word_length > 0);
						if (box->m_is_bullet)
						{
							do_marker = true;
							is_bullet = true;

							int i;

							for (i = box_idx + 1; i <= para->m_last_box; i++)
							{
								reflow_box *next_box = &words[i];

								if (next_box->m_do_not_print)
									continue;

								if (next_box->m_line_count > 1)
								{
									/* a definite end to this paragraph! */
									UNC_ASSERT(deferred_newlines == 0);
									UNC_ASSERT(do_after_marker == -1);
									break;
								}
								else if (next_box->m_is_first_on_line && next_box->m_is_bullet)
								{
									do_after_marker = i;
									deferred_newlines = 1;
									break;
								}
							}
						}
					}

					/*
					And then there's the option where a paragraph break is detected when the previous line
					ended with one of a particular set of characters and this line starts with one of another
					set.
					For example, this can help keep the inter-line breaks for large chunks of text where lines
					end with some punctuation, say period '.', question mark '?', exclamation mark '!' or colon ':',
					and the next line starting off with a capital, an example of which can be seen above in this comment itself.

					We handle this one by only starting work when we have a match for the SOL (Start Of Line) set:
					a subsequent backwards scan will quickly reveal whether we're sitting on a paragraph break or not.
					*/
					if (!do_marker)
					{
						UNC_ASSERT(do_after_marker == -1);
						UNC_ASSERT(box->m_is_first_on_line);
						UNC_ASSERT(box->m_word_length > 0);
						if (in_RE_set(m_cmt_reflow_SOL_markers, box->m_text[0]))
						{
							int i;
							reflow_box *prev = NULL;
							int count = 0;

							for (i = box_idx - 1; i >= para->m_first_box; i--)
							{
								prev = &words[i];

								if (prev->m_do_not_print)
									continue;

								count += prev->m_line_count;
								if (count > 1)
								{
									/* bigger break than just a single newline here. */
									prev = NULL;
									break;
								}
								if (prev->m_word_length == 0)
									continue;

								break;
							}
							if (i < para->m_first_box)
							{
								prev = NULL;
							}

							if (prev)
							{
								UNC_ASSERT(prev->m_word_length > 0);
								UNC_ASSERT(!prev->m_do_not_print);

								if (in_RE_set(cpd.settings[UO_cmt_reflow_EOL_markers].str, prev->m_text[prev->m_word_length - 1]))
								{
									do_marker = true;
									UNC_ASSERT(count == 1);
									//deferred_newlines = 1;
									is_reqd_linebreak_in_para = true;
								}
							}
						}
					}

					if (do_marker)
					{
						if (box_idx != para->m_first_box)
						{
							/* create a sibling to store the new 'intermission text' */
							paragraph_box *next_para = new paragraph_box();

							/* copy sibling settings et al */
							//*next_para = *para;

							/* adjust some stuff */
							next_para->m_first_child = NULL;
							next_para->m_first_box = box_idx;
							next_para->m_last_box = para->m_last_box;
							UNC_ASSERT(next_para->m_last_box >= next_para->m_first_box);
							next_para->m_parent = parent;

							UNC_ASSERT(parent->m_first_child);
							para->m_next_sibling = next_para;
							UNC_ASSERT(para->m_next_sibling != para->m_first_child);
							next_para->m_previous_sibling = para;

							/* terminate the old one */
							adjust_para_last_box(para, box_idx - 1);
							UNC_ASSERT(para->m_last_box >= para->m_first_box);

							/* do not use 'deferred_newlines' here! That one is for 'do_after_marker' et al! */
							//UNC_ASSERT(deferred_newlines == 0);
							para->m_min_required_linebreak_after = 1;
							next_para->m_min_required_linebreak_before = 1;

							/* and flush ASCII art and boxed text tallies, etc. */
							graph_char_tally = 0;
							graph_word_idx = -1;

							nonreflow_char_tally = 0;
							nonreflow_word_idx = -1;

							UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
							UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
							UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
							UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

							para = next_para;

							UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
							UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
							UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
							UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
						}

						para->m_is_intermission = is_intermission;
						para->m_is_bullet = is_bullet;
						para->m_is_doxygen_par = is_doxygen_tag;
						para->m_bullet_box = box_idx;
						para->m_doxygen_tag_box = box_idx;

						if (is_reqd_linebreak_in_para)
						{
							para->m_continue_from_previous = true;
							UNC_ASSERT(para->m_min_required_linebreak_before >= 1);
						}
					}

					UNC_ASSERT(do_after_marker >= 0 ? do_marker : true);
					if (do_after_marker >= 0)
					{
						UNC_ASSERT(words[do_after_marker].m_line_count < 2); // ensure the code above handles the common dual-NL case without setting do_after_marker

						box_idx = do_after_marker;
						UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
						UNC_ASSERT(para->m_parent ? box_idx >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? box_idx <= para->m_parent->m_last_box : 1);

						/* create a sibling to store the new 'intermission text' */
						paragraph_box *next_para = new paragraph_box();

						/* copy sibling settings et al */
						//*next_para = *para;

						/* adjust some stuff */
						next_para->m_first_child = NULL;
						next_para->m_first_box = box_idx;
						next_para->m_last_box = para->m_last_box;
						UNC_ASSERT(next_para->m_last_box >= next_para->m_first_box);
						next_para->m_parent = parent;

						UNC_ASSERT(parent->m_first_child);
						para->m_next_sibling = next_para;
						UNC_ASSERT(para->m_next_sibling != para->m_first_child);
						next_para->m_previous_sibling = para;

						/* terminate the old one */
						adjust_para_last_box(para, box_idx - 1);
						UNC_ASSERT(para->m_last_box >= para->m_first_box);

						UNC_ASSERT(deferred_newlines >= 1);
						para->m_min_required_linebreak_after = deferred_newlines;
						next_para->m_min_required_linebreak_before = deferred_newlines;
						deferred_newlines = 0;

						/* and flush ASCII art and boxed text tallies, etc. */
						graph_char_tally = 0;
						graph_word_idx = -1;

						nonreflow_char_tally = 0;
						nonreflow_word_idx = -1;

						UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
						UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

						para = next_para;

						UNC_ASSERT(para->m_previous_sibling ? para->m_previous_sibling->m_last_box < para->m_first_box : 1);
						UNC_ASSERT(para->m_next_sibling ? para->m_next_sibling->m_first_box > para->m_last_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
						UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);

						/*
						Adjust box_idx for the increment in the for() loop and the newline check below
						*/
						box_idx--;
					}

					/* track indent */
					indent = box->m_leading_whitespace_length;
				}
			}
		}

		/* is this a dual-newline box? this is definite end-of-para marker so we note it! */
		box_idx = skip_tailing_newline_box(para, words, box_idx, 1, deferred_newlines);
		if (deferred_newlines >= 2)
		{
			/* save a few lines of code ;-) */
			create_deferred_sibling = true;
		}
		else
		{
			/*
			discard single newlines as para breaks... UNLESS we've ended with punctuation
			in the previous line plus we continue with a capital in the next AND the last line
			is significantly shorter than the next one.
			*/
			//deferred_newlines = 0;
		}
	}

	if (box_idx > para->m_last_box)
	{
		box_idx--;
	}

	UNC_ASSERT(para->m_parent ? para->m_first_box >= para->m_parent->m_first_box : 1);
	UNC_ASSERT(para->m_parent ? para->m_last_box <= para->m_parent->m_last_box : 1);
	UNC_ASSERT(para->m_parent ? box_idx >= para->m_parent->m_first_box : 1);
	UNC_ASSERT(para->m_parent ? box_idx <= para->m_parent->m_last_box : 1);

	return box_idx;
}












struct reflow_tune_parameters_t
{
	/* temporaries ... */
	int deferred_whitespace;
	int deferred_nl;
	int mandatory_deferred_nl;
	int words_printed_on_this_line;
	int content_printed_on_this_line;
	int level;

	/* configuration ... */
	//int linewidth;
	int max_usable_linewidth;
	int firstline_extra_space;
	int lastline_extra_space;
	int start_column;
	int width_delta; //< current adjustment from the mean

public:
	reflow_tune_parameters_t(const cmt_reflow *cmt, int delta = 0) :
			deferred_whitespace(0),
			deferred_nl(0),
			mandatory_deferred_nl(0),
			words_printed_on_this_line(0),
			content_printed_on_this_line(0),
			level(0),
			/* configuration ... */
			//linewidth(0),
			max_usable_linewidth(0),
			firstline_extra_space(0),
			lastline_extra_space(0),
			start_column(0),
			width_delta(delta)
	{
		int linewidth = cmt->m_line_wrap_column - cmt->m_left_global_output_column;
		deferred_whitespace = 0; // cmt->m_extra_post_star_indent;
		start_column = cmt->m_left_global_output_column + deferred_whitespace;

		max_usable_linewidth = linewidth - cmt->m_extra_pre_star_indent
			 - cmt->m_extra_post_star_indent
			- cmt->m_lead_cnt;
		if (!cmt->m_has_leading_and_trailing_nl)
		{
			firstline_extra_space = 1 /* TODO */ ;
			lastline_extra_space = 1 /* TODO */ ;
		}
	}
};






typedef enum
{
	REFLOW_SCORIN_CHI2 = 0,
} reflow_scoring_mode_t;



typedef enum scoring_line_type
{
	FIRST_LINE_OF_PARA = 0,
	NEXT_LINE_OF_PARA,
	LAST_LINE_OF_PARA
} scoring_line_type;



class break_suggestions
{
public:
	break_suggestions(const paragraph_box *para, words_collection &words, reflow_tune_parameters_t &tuning, reflow_scoring_mode_t mode = REFLOW_SCORIN_CHI2) :
			line_count(0),
			ragged_right_cost_sum(0.0),
			total_line_count(0),
			total_ragged_right_cost_sum(0.0),
			boxset(words),
			tuning_params(tuning),
			scoring_mode(mode)
	{
	}
	~break_suggestions()
	{
	}
	break_suggestions(const break_suggestions &src) :
			line_count(src.line_count),
			ragged_right_cost_sum(src.ragged_right_cost_sum),
			total_line_count(src.total_line_count),
			total_ragged_right_cost_sum(src.total_ragged_right_cost_sum),
			boxset(src.boxset),
			tuning_params(src.tuning_params),
			scoring_mode(src.scoring_mode)
	{
	}
	break_suggestions &operator =(const break_suggestions &src)
	{
		if (&src != this)
		{
			line_count = src.line_count;
			ragged_right_cost_sum = src.ragged_right_cost_sum;
			total_line_count = src.total_line_count;
			total_ragged_right_cost_sum = src.total_ragged_right_cost_sum;

			int i;
			int count = (int)src.boxset.count();
			boxset.reserve(count);
			for (i = 0; i < count; i++)
			{
				boxset[i] = src.boxset[i];
			}

			tuning_params = src.tuning_params;
			scoring_mode = src.scoring_mode;
		}
		return *this;
	}

protected:
	int line_count; //< the number of lines counted so far in this paragraph
	double ragged_right_cost_sum; //< the cummulative line breaking costs
	int total_line_count;
	double total_ragged_right_cost_sum;
	words_collection boxset;
	reflow_tune_parameters_t &tuning_params;
	reflow_scoring_mode_t scoring_mode;

public:
	double get_score() const
	{
		if (total_line_count + line_count == 0)
			return 0.0;  // ideal score for this nil paragraph

		return (total_ragged_right_cost_sum + ragged_right_cost_sum) / (total_line_count + line_count);
	}
	int get_linecount_total() const
	{
		return total_line_count + line_count;
	}
	int get_linecount() const
	{
		return line_count;
	}
	int increment_linecount()
	{
		return line_count++;
	}
	void mark_start_of_paragraph(paragraph_box *para)
	{
		UNC_ASSERT(para);

		total_line_count += line_count;
		total_ragged_right_cost_sum += ragged_right_cost_sum;
		line_count = 0;
		ragged_right_cost_sum = 0.0;
	}
	/*
	mark the end of the render: all paragraphs have been reflows in this trial render.
	*/
	void mark_end_of_sequence(words_collection &words)
	{
		// mark the end of the last paragraph
		total_line_count += line_count;
		total_ragged_right_cost_sum += ragged_right_cost_sum;
		line_count = 0;
		ragged_right_cost_sum = 0.0;

		// and copy the current render results to 'cache'
		int i;
		int count = (int)words.count();
		boxset.reserve(count);
		for (i = 0; i < count; i++)
		{
			boxset[i] = words[i];
		}
	}


	void add_cost(int width_remaining, paragraph_box *para, int content_printed_on_this_line, int words_printed_on_this_line, scoring_line_type linetype)
	{
		if (content_printed_on_this_line > 0)
		{
			int linenumber = increment_linecount();
			UNC_ASSERT(linenumber >= 0);
			UNC_ASSERT(linenumber < (int)boxset.count());
			UNC_ASSERT(linenumber < line_count);
			double cost = 0.0;

			switch (scoring_mode)
			{
			default:
				UNC_ASSERT(!"Shouldn't get here!");

			case REFLOW_SCORIN_CHI2:
				if (!para->para_is_a_usual_piece_of_text())
				{
					// this line always will be counted as 'almost perfect'
					cost = 1.0;
				}
				else
				{
					// rate words_printed_on_this_line in relation to widows/orphans...
					switch (linetype)
					{
					case FIRST_LINE_OF_PARA:
						cost = width_remaining;

						/* rate with regard to orphans as well... */
						if (words_printed_on_this_line < cpd.settings[UO_cmt_reflow_orphans].n)
						{
							cost += cpd.settings[UO_cmt_reflow_orphans].n - words_printed_on_this_line;
						}

						cost = cost * cost;
						break;

					default:
					case NEXT_LINE_OF_PARA:
						cost = width_remaining;
						cost = cost * cost;
						break;

					case LAST_LINE_OF_PARA:
						/*
						Here we don't care how much width was left; after all it's the remainder/end of the paragraph.
						*/
						cost = 0.0;

						/* rate with regard to widows as well... */
						if (words_printed_on_this_line < cpd.settings[UO_cmt_reflow_widows].n)
						{
							cost += cpd.settings[UO_cmt_reflow_widows].n - words_printed_on_this_line;
						}

						cost = cost * cost;
						break;
					}
				}
				break;
			}
			ragged_right_cost_sum += cost;
		}
	}
	void reset(void)
	{
		line_count = 0;
		ragged_right_cost_sum = 0.0;
		total_line_count = 0;
		total_ragged_right_cost_sum = 0.0;
	}
	void apply(paragraph_box *para, words_collection &words)
	{
		/* apply settings to the box set pointed at by para/words */
		int i;
		int count = (int)boxset.count();

		words.reserve(count);
		for (i = para->m_first_box; i <= para->m_last_box; i++)
		{
			words[i] = boxset[i];
		}
	}
};




/*
Return 0 if there's one or more newlines immediately up ahead in the reflow box stream.

Return 1 when there isn't, i.e. 'push' a deferred newline.

@note This scan reaches beyond the current paragraph: if we don't, then pending newlines
      are incorrectly pushed at the outgoing edge of the paragraph while the next paragraph
	  MAY start with a newline-carrying box or either paragraph comes with a
	  'mandatory minimum number of newlines' requirement, thus indirectly increasing the total number
	  of newlines being printed between them as pending/pushed newlines would have been 'printed'
	  already before when the first para (as is usual) carries a last newline-containing box.
*/
int cmt_reflow::there_is_no_newline_up_ahead(paragraph_box *para, words_collection &words, int current_box_idx)
{
	reflow_box *box = &words[current_box_idx];

	if (box->m_word_length > 0
		|| box->m_left_edge_thickness > 0
		|| box->m_right_edge_thickness > 0)
	{
		/*
		Before we reach the newline, there's text to be printed. Hence
		we push for one(1) deferred newline right now.
		*/
		return 1;
	}

	int i;
	for (i = current_box_idx + 1; i < (int)words.count(); i++)
	{
		box = &words[i];

		if (box->m_do_not_print)
			continue;

		if (box->m_line_count > 0)
		{
			// newline ahead; do not push an extra 'deferred' one!
			return 0;
		}

		if (box->m_word_length > 0
			|| box->m_left_edge_thickness > 0
			|| box->m_right_edge_thickness > 0)
		{
			// end the scan: printable text up ahead.
			break;
		}
	}

	/*
	No newlines found up ahead, yet we MAY have crossed the paragraph border by now
	and in there a non-zero mandatory newline requirement may be listed, which acts
	as a 'newline up ahead' as well.
	*/
	if (para->m_last_box < i)
	{
		if (para->m_min_required_linebreak_after > 0)
		{
			return 0;
		}

		/* find out who is the next paragraph: */
		if (para->m_next_sibling)
		{
			para = para->m_next_sibling;

			if (para->m_min_required_linebreak_before > 0)
			{
				return 0;
			}
		}
		else if (para->m_parent)
		{
			para = para->m_parent;

			if (para->m_next_sibling)
			{
				para = para->m_next_sibling;

				if (para->m_min_required_linebreak_before > 0)
				{
					return 0;
				}
			}
		}
	}

	return 1;
}



int cmt_reflow::reflow_a_single_para_4_trial(paragraph_box *para, words_collection &words, break_suggestions &scoring, reflow_tune_parameters_t &tuning)
{
	int i;

	scoring.mark_start_of_paragraph(para);

	/*
	reflow paragraph before printing it: adjust line breaks and whitespace.

	ASSUMPTION: paragraphs always start at a whitespace/linebreak boundary, i.e. we may always
	            assume that we start with zero orphans and not any need to keep-with-previous here?

	ANSWER: NO. There are XML tag 'paragraphs' and other bits of text marked up as 'paragraphs'
	        which really are part of larger sentences/paragraphs that possibly should appear as one
			flowing block of text.
	*/

	bool is_first_line_of_para = true;
	bool waiting_for_first_nonempty_box_on_line = true;
	bool para_is_a_usual_piece_of_text = para->para_is_a_usual_piece_of_text();
	bool line_is_a_usual_piece_of_text = para_is_a_usual_piece_of_text; // true: don't count the fewer words printed on this line against it.
	int deferred_nl = tuning.deferred_nl;
	int deferred_whitespace = tuning.deferred_whitespace;
	//int mandatory_deferred_nl = 0;
	int words_printed_on_this_line = 0;
	int content_printed_on_this_line = 0;

	int width = tuning.max_usable_linewidth;
	int last_box_to_keep_together = -1;

	window_orphan_info_t wo_info;
	calculate_widow_and_orphan_aspects(para, words, tuning.max_usable_linewidth, wo_info);

	if (para->m_starts_on_new_line)
	{
		deferred_whitespace = para->m_first_line_indent;
	}

	for (i = para->m_first_box; i <= para->m_last_box; i++)
	{
		UNC_ASSERT(i >= 0);
		UNC_ASSERT(i < (int)words.count());

		reflow_box *box = &words[i];

		if (box->m_do_not_print)
			continue;

		int box_print_width = box->m_word_length;
		if (// content_printed_on_this_line == 0
			box->m_is_part_of_boxed_txt
			// && is_first_line_of_para
			// && box->m_is_first_on_line
			)
		{
			/*
			TODO: properly handle semi-boxed and fully boxed comments by rendering them without
				  the top/bottom/left/right borders and only once done, wrap those borders around
				  the paragraph / series-of-paragraphs.
			*/
			box_print_width += box->m_left_edge_thickness + box->m_right_edge_thickness;
		}

		/*
		Reset line breaks between words; the reflow code will (re-)insert them at the appropriate spots.

		A newline (line break) is equivalent to a single whitespace when reflowing.
		*/
		if (box->m_is_non_reflowable)
		{
			/*
			we're still in a non-reflowable section, so we only move the newlines by deferring them.
			*/
			if (box->m_line_count > 0)
			{
				deferred_nl += box->m_line_count;
				box->m_line_count = 0;

				deferred_whitespace = box->m_leading_whitespace_length;
				box->m_leading_whitespace_length = 0;

				// tuning.mandatory_deferred_nl = 0;
				//words_printed_on_this_line = 0;
				//content_printed_on_this_line = 0;
				//line_is_a_usual_piece_of_text = para_is_a_usual_piece_of_text;
			}
			else if (box->m_is_first_on_line)
			{
				if (box->m_line_count > 0)
				{
					deferred_nl += box->m_line_count;
				}
				else if (content_printed_on_this_line > 0
					&& deferred_nl == 0)
				{
					// force a newline in here when one hasn't happened just before we got here.
					deferred_nl = there_is_no_newline_up_ahead(para, words, i);
				}
				box->m_line_count = 0;

				deferred_whitespace = box->m_leading_whitespace_length;
				box->m_leading_whitespace_length = 0;
			}
		}
		else if (i <= last_box_to_keep_together)
		{
			// we're still inside a keep-together section...
			if (box->m_line_count > 1)
			{
				deferred_nl += box->m_line_count;
				box->m_line_count = 0;

				//deferred_whitespace = box->m_leading_whitespace_length;
				deferred_whitespace = 0;
				box->m_leading_whitespace_length = 0;

				// tuning.mandatory_deferred_nl = 0;
				//words_printed_on_this_line = 0;
				//content_printed_on_this_line = 0;
				//line_is_a_usual_piece_of_text = para_is_a_usual_piece_of_text;
			}
			else if (box->m_line_count > 0)
			{
				if (deferred_whitespace == 0
					&& content_printed_on_this_line > 0
					&& deferred_nl == 0)
				{
					deferred_whitespace = 1;
				}
				box->m_line_count = 0;
				box->m_leading_whitespace_length = 0;
			}
			else if (box->m_is_first_on_line)
			{
				// remove leading whitespace when this box WAS positioned at the front of the line.
				if (deferred_whitespace == 0
					&& content_printed_on_this_line > 0
					&& deferred_nl == 0)
				{
					deferred_whitespace = 1;
				}
				box->m_leading_whitespace_length = 0;
			}
		}
		else
		{
			// regular processing ...
			if (box->m_line_count > 0)
			{
				if (deferred_whitespace == 0
					&& content_printed_on_this_line > 0
					&& deferred_nl == 0)
				{
					deferred_whitespace = 1;
				}
				box->m_line_count = 0;
				box->m_leading_whitespace_length = 0;
			}
			else if (box->m_is_first_on_line)
			{
				// remove leading whitespace when this box WAS positioned at the front of the line.
				if (deferred_whitespace == 0
					&& content_printed_on_this_line > 0
					&& deferred_nl == 0)
				{
					deferred_whitespace = 1;
				}
				box->m_leading_whitespace_length = 0;
			}

			last_box_to_keep_together = i;
			box_print_width = estimate_box_print_width(para, words, i, &last_box_to_keep_together);
		}

		deferred_whitespace += box->m_leading_whitespace_length;
		box->m_leading_whitespace_length = 0;
		box->m_line_count = 0;
		box->m_is_first_on_line = false;

		/* always print at least one word of text per line */
		if (content_printed_on_this_line > 0
			&& deferred_nl == 0
			&& width <= box_print_width + deferred_whitespace
#if 0
			/* next: prevent orphans */
			&& (!is_first_line_of_para || i > wo_info.orphan_last_box_idx)
			/* next: prevent widows: break so there's space on the next line for the widows */
			&& (is_first_line_of_para || (i < wo_info.widow_first_box_idx
				&& width <= box_print_width + deferred_whitespace + wo_info.widow_render_width))
#endif
			/* next: exercise the words-per-line minimum restriction IFF we may */
			&& (words_printed_on_this_line >= m_cmt_reflow_minimum_words_per_line
				|| !para_is_a_usual_piece_of_text
				|| !line_is_a_usual_piece_of_text))
		{
			/* line break before this word! */
			deferred_nl = there_is_no_newline_up_ahead(para, words, i);
			deferred_whitespace = 0;
		}
#if 0
		/* never allow /any/ content to extend beyond the allowed overflow bound */
		if (content_printed_on_this_line >= 1
			&& width + m_cmt_reflow_overshoot <= box_print_width + deferred_whitespace)
		{
			/* line break before this word! */
			if (deferred_nl == 0)
			{
				deferred_nl = there_is_no_newline_up_ahead(words, i);
			}
		}
#endif

		if (deferred_nl < tuning.mandatory_deferred_nl)
		{
			if (deferred_nl == 0)
			{
				deferred_whitespace = 0;
			}
			deferred_nl = tuning.mandatory_deferred_nl;
		}
		if (deferred_nl > 0)
		{
			box->m_line_count = deferred_nl;
#if 0
			if(box->m_line_count > 1)
				fprintf(stderr, "r%d\n", i);
#endif
			UNC_ASSERT(box->m_line_count > 1 ? (i == para->m_first_box) : 1);
			UNC_ASSERT((box->m_line_count > 1 && i == para->m_first_box) ? para->m_min_required_linebreak_before > 0 : 1);
			box->m_is_first_on_line = (box->m_word_length > 0
										|| box->m_left_edge_thickness > 0
										|| box->m_right_edge_thickness > 0);
			waiting_for_first_nonempty_box_on_line = !box->m_is_first_on_line;

			scoring_line_type linetype = ((is_first_line_of_para && content_printed_on_this_line == 0) ? FIRST_LINE_OF_PARA : NEXT_LINE_OF_PARA);
			scoring.add_cost(width, para, content_printed_on_this_line, words_printed_on_this_line, linetype);

			width = tuning.max_usable_linewidth;

			if (deferred_whitespace == 0)
			{
				deferred_whitespace = 0; // m_extra_post_star_indent;
				if (is_first_line_of_para && content_printed_on_this_line == 0)
				{
					deferred_whitespace += para->m_first_line_indent;
				}
				else
				{
					deferred_whitespace += para->m_hanging_indent;
				}
			}

			if (is_first_line_of_para && content_printed_on_this_line > 0)
			{
				is_first_line_of_para = false;
			}
			deferred_nl = 0;
			tuning.mandatory_deferred_nl = 0;
			words_printed_on_this_line = 0;
			content_printed_on_this_line = 0;
			line_is_a_usual_piece_of_text = para_is_a_usual_piece_of_text;
		}

		UNC_ASSERT(deferred_nl == 0);
		UNC_ASSERT(tuning.mandatory_deferred_nl == 0);

		if (box->m_word_length > 0
			|| box->m_left_edge_thickness > 0
			|| box->m_right_edge_thickness > 0)
		{
			box->m_leading_whitespace_length = deferred_whitespace;
			width -= deferred_whitespace;
			deferred_whitespace = 0;
#if 0
			write2output(box->m_text, box->m_word_length);
#endif
			width -= box->m_word_length
					+ box->m_left_edge_thickness
					+ box->m_right_edge_thickness;
			//is_first_line_of_para = false;
			content_printed_on_this_line++;
			if (box->box_is_a_usual_piece_of_text(true))
			{
				words_printed_on_this_line++;
			}
			else if (!box->box_is_a_usual_piece_of_text(false))
			{
				line_is_a_usual_piece_of_text = false;
			}

			if (waiting_for_first_nonempty_box_on_line)
			{
				box->m_is_first_on_line = true;
				waiting_for_first_nonempty_box_on_line = false;
			}
		}
		else
		{
			UNC_ASSERT(box->m_leading_whitespace_length == 0);
		}

		UNC_ASSERT(deferred_nl == 0);
		UNC_ASSERT(tuning.mandatory_deferred_nl == 0);

		/* only print trailing whitespace when the next word is also printed on this line */
		deferred_whitespace += box->m_trailing_whitespace_length;
		box->m_trailing_whitespace_length = 0;
	}

	if (tuning.mandatory_deferred_nl < para->m_min_required_linebreak_after)
	{
		tuning.mandatory_deferred_nl = para->m_min_required_linebreak_after;
	}

	scoring.add_cost(width, para, content_printed_on_this_line, words_printed_on_this_line, LAST_LINE_OF_PARA);

	tuning.deferred_nl = deferred_nl;
	tuning.deferred_whitespace = deferred_whitespace;

	return SUCCESS;
}



int cmt_reflow::reflow_para_tree_4_trial(paragraph_box *para, words_collection &words, break_suggestions &scoring, reflow_tune_parameters_t &tuning)
{
	int rv = SUCCESS;

	UNC_ASSERT(para);
	UNC_ASSERT(para->m_last_box + 1 == (int)words.count());

	while (para && rv == SUCCESS)
	{
		paragraph_box *last_child = get_last_sibling(para->m_first_child);

		/*
		child para's which do not span the entire parent when combined may be 'marker' paragraphs, e.g. XML tags, math sections, etc.

		These are to be treated as 'continuous' and, eventually, non-reflowing. No clear idea how to approach those, exactly.

		It means the para tree traversal must be changed, anyway, as multiple levels of 'para' can be active, one level for each box.
		But it doesn't mean 'para' is the one for this box, unless we chop para reflow in tiny pieces. We can do so, but then we need to
		drop back to parent (or parent of parent! etc.etc.) when there's a box 'gap' between SIBLINGS! Which means we need to track
		child/para position PER LEVEL no matter what!
		*/
		UNC_ASSERT(para->m_first_child ? para->m_first_child->m_first_box == para->m_first_box : 1);
		UNC_ASSERT(last_child ? last_child->m_last_box == para->m_last_box : 1);

#if defined(_MSC_VER)
#pragma message(__FILE__ "(" STRING(__LINE__) ") : TODO: cope with paras with subparts.")
#endif

		if (para->m_first_child)
		{
			UNC_ASSERT(para->m_first_child && para->m_first_child->m_first_box == para->m_first_box);
			UNC_ASSERT(last_child && last_child->m_last_box == para->m_last_box);

			/*
			child para's which do not span the entire parent when combined are 'marker' paragraphs, e.g. XML tags, math sections, etc.

			These are to be treated as 'continuous' and, eventually, non-reflowing. No clear idea how to approach those, exactly.

			It means the para tree traversal must be changed, anyway, as multiple levels of 'para' can be active, one level for each box.
			But it doesn't mean 'para' is the one for this box, unless we chop para reflow in tiny pieces. We can do so, but then we need to
			drop back to parent (or parent of parent! etc.etc.) when there's a box 'gap' between SIBLINGS! Which means we need to track
			child/para position PER LEVEL no matter what!
			*/

			tuning.level++;
			para = para->m_first_child;
			continue;
		}
		else
		{
			if (tuning.mandatory_deferred_nl < para->m_min_required_linebreak_before)
			{
				tuning.mandatory_deferred_nl = para->m_min_required_linebreak_before;
			}

			/*
			reflow paragraph before printing it: adjust line breaks and whitespace.
			*/
			rv = reflow_a_single_para_4_trial(para, words, scoring, tuning);
		}

		/* when no more sibling, then traverse up the tree and go to the next sibling there. */
		while (!para->m_next_sibling && para->m_parent)
		{
			para = para->m_parent;
			tuning.level--;
		}
		para = para->m_next_sibling;
	}

	if (tuning.level == 0)
	{
		// write2out_comment_end(deferred_whitespace, deferred_nl);
	}

	scoring.mark_end_of_sequence(words);

	return rv;
}


void cmt_reflow::determine_optimal_para_reflow(paragraph_box *para, words_collection &words, reflow_tune_parameters_t &tuning)
{
	break_suggestions best(para, words, tuning);
	break_suggestions current(para, words, tuning);

	UNC_ASSERT(para);
	UNC_ASSERT(para->m_last_box + 1 == (int)words.count());

	//const int linewidth = m_line_wrap_column - m_left_global_output_column;
	//int width = linewidth;
	//int level = 0;

	//int deferred_whitespace = m_extra_post_star_indent; // write2out_comment_start(para, words);
	//UNC_ASSERT(deferred_whitespace == cpd.settings[UO_cmt_sp_after_star_cont].n);

	//const int start_col = m_left_global_output_column + deferred_whitespace;

	const int lower_lw_limit = (tuning.max_usable_linewidth + 5) / 10;

	reflow_tune_parameters_t testtuning(tuning);

	/*
	First run the 'regular' reflow action. This one acts as the start/reference;
	when subsequent trials deliver a better reflow score, the winner will be applied
	at the end.
	*/
	best.reset();
	int rv = reflow_para_tree_4_trial(para, words, best, testtuning);
	UNC_ASSERT(rv == 0);

	if (0)
	{
	int i;

	/*
	Next, run trials for shortened linewidths.
	*/
	for (i = 1; i < lower_lw_limit && i < tuning.max_usable_linewidth - 20 /* minimum acceptable width for reflow trials */; i++)
	{
		testtuning = tuning;
		testtuning.max_usable_linewidth = tuning.max_usable_linewidth - i;
		testtuning.width_delta = -i;

		current.reset();
		int trv = reflow_para_tree_4_trial(para, words, current, testtuning);
		if (trv == SUCCESS)
		{
			/*
			successful trial; see if the score is better then our current best.

			Note that the further the trial deviates from the initial configuration, the higher
			the required score must be to 'win' -- this is done so that tunings near the initial
			are favored, or in other words: "user preferences are important"
			*/
			UNC_ASSERT(tuning.max_usable_linewidth > 0);
			double delta = i;
			double factor = 1.0 + delta / tuning.max_usable_linewidth;

			/* smaller is better: */
			double current_score = current.get_score() * factor;
			double best_score = best.get_score();
			if (current_score < best_score)
			{
				best = current;
			}
		}
	}

	/*
	Next, run trials for 'overshooting' linewidths.
	*/
	for (i = 1; i <= m_cmt_reflow_overshoot; i++)
	{
		testtuning = tuning;
		testtuning.max_usable_linewidth = tuning.max_usable_linewidth + i;
		testtuning.width_delta = +i;

		current.reset();
		int trv = reflow_para_tree_4_trial(para, words, current, testtuning);
		if (trv == SUCCESS)
		{
			/*
			successful trial; see if the score is better then our current best.

			Note that the further the trial deviates from the initial configuration, the higher
			the required score must be to 'win' -- this is done so that tunings near the initial
			are favored, or in other words: "user preferences are important"

			Overshooting is regarded as worse than 'shrinking' the width, BTW.
			*/
			UNC_ASSERT(tuning.max_usable_linewidth > 0);
			double delta = i;
			double factor = 1.0 + delta * delta / tuning.max_usable_linewidth;

			/* smaller is better: */
			double current_score = current.get_score() * factor;
			double best_score = best.get_score();
			if (current_score < best_score)
			{
				best = current;
			}
		}
	}

	}

	/*
	Now that we have determined the best layout, apply it.
	*/
	best.apply(para, words);
}




void cmt_reflow::reflow_para_hierarchy(paragraph_box *para, words_collection &words)
{
	reflow_tune_parameters_t tuning(this);

	if (!para->m_is_non_reflowable)
	{
		determine_optimal_para_reflow(para, words, tuning);
	}
}













/*
simply dump the text boxes to the output; all the whitespace and newlines have been
set up in each text box by the reflow engine before this method is invoked.

TODO: The only tough bit is handling 'boxed comments' in here...
*/
void cmt_reflow::write_comment_to_output(paragraph_box *para, words_collection &words)
{
	UNC_ASSERT(para);

	//int start_box = para->m_first_box;
	//int break_box = words.count();

	int deferred_whitespace;
	int deferred_nl = 0;

	deferred_whitespace = write2out_comment_start(para, words);
	//UNC_ASSERT(deferred_whitespace == cpd.settings[UO_cmt_sp_after_star_cont].n);
	//int mandatory_deferred_nl = para->m_min_required_linebreak_before;

	while (para)
	{
		int i;

		/*
		now we dump the para to the output; any reflowing of the para has been done, so everything is set and ready to go.
		*/
		for (i = para->m_first_box; i <= para->m_last_box; i++)
		{
			UNC_ASSERT(i >= 0);
			UNC_ASSERT(i < (int)words.count());

			reflow_box *box = &words[i];

			if (box->m_do_not_print)
				continue;

			deferred_nl += box->m_line_count;
			if (deferred_nl > 0)
			{
				deferred_whitespace = 0; // cpd.settings[UO_cmt_sp_after_star_cont].n;
			}

			deferred_whitespace += box->m_leading_whitespace_length;

			if (box->m_word_length > 0
				|| box->m_left_edge_thickness > 0
				|| box->m_right_edge_thickness > 0)
			{
				int j;

				/* take the initial minimum required NL-count into consideration: */
				//if (deferred_nl < mandatory_deferred_nl)
				//{
				//	deferred_nl = mandatory_deferred_nl;
				//	mandatory_deferred_nl = 0;
				//}

				for (j = 0; j < deferred_nl; j++)
				{
					deferred_whitespace = write2out_comment_next_line();
					deferred_whitespace += box->m_leading_whitespace_length;
				}
				deferred_nl = 0;

				for (j = deferred_whitespace; j > 0; j -= min(16, j))
				{
					write2output("                ", min(16, j));
				}
				deferred_whitespace = 0;

				if (box->m_left_edge_thickness > 0)
				{
					UNC_ASSERT(box->m_left_edge_text);
					write2output(box->m_left_edge_text, box->m_left_edge_thickness);
				}
				write2output(box->m_text, box->m_word_length);
				if (box->m_right_edge_thickness > 0)
				{
					UNC_ASSERT(box->m_right_edge_text);
					write2output(box->m_right_edge_text, box->m_right_edge_thickness);
				}
			}

			/* only print trailing whitespace when the next word is also printed on this line */
			deferred_whitespace += box->m_trailing_whitespace_length;
		}

#if 0
		if (!para->m_next_sibling)
		{
			if (mandatory_deferred_nl < para->m_min_required_linebreak_after)
			{
				mandatory_deferred_nl = para->m_min_required_linebreak_after;
			}
		}
#endif
		para = para->m_next_sibling;
	}

	if (deferred_nl > 0)
	{
		deferred_whitespace = 0; // cpd.settings[UO_cmt_sp_after_star_cont].n;
	}
	write2out_comment_end(deferred_whitespace, deferred_nl);
}









/*
Use the text statistics calculated from the given reflow point collective
and render the text, with or without a box surrounding it.

Process steps:

- take the text and chop it it up, creating a list of wrap/reflow points.
  These reflow points are graded (priority) depending on their context and
user settings.

  Recognizes bullet lists, DoxyGen tags, etc. as special tokens and annotates
  the reflow points list accordingly, in order to allow the reflower to produce
an adequate layout.

- takes a collection of reflow points and calculates a visually appealing
  reflow, i.e. determines where we should wrap exactly.
  Annotate the reflow point collection accordingly.

  This reflower takes forced breaks, forced and hinted indents, etc. into account
  while generating the layout. Widow and orphan control is also part of the
game here.

- takes an annotated reflow point collection and calculates the line width(s),
  number of lines, etc. statistics, which are used by the box renderer (and
possibly a few others too).
*/
void cmt_reflow::render(void)
{
	push(""); // make sure 'comment' is initialized properly, even for empty comments!
	UNC_ASSERT(m_comment);

#if 0
	/* debugging: output the 'original text': */
	write2output("\n\n***INPUT***\n");
	write2output(m_comment);
	write2output("\n***END-OF-INPUT***\n");
#endif

	/*
	First remove the first and last NEWLINEs (empty lines, really) to ensure
	single line and block comments are reformatted properly: these first and last
	newlines are solely determined by the UO_cmt_c_nl_start/UO_cmt_c_nl_end
	settings (and those of their C++ equivalents UO_cmt_cpp_nl_start/UO_cmt_cpp_nl_start)
	*/
	strip_first_and_last_nl_from_text();
	UNC_ASSERT(m_comment_len == strlen(m_comment));

	set_deferred_cmt_config_params_phase2();

	/*
	now we have the entire text stored in m_comment.

	Chop it up into words and paragraphs
	*/
	words_collection words(*this);

	chop_text_into_reflow_boxes(words);
	UNC_ASSERT(m_comment_len == strlen(m_comment));

	optimize_reflow_boxes(words);

	/*
	analyze the boxes, cluster them into simple 'paragraphs' and
	apply reflow/non-reflow heuristics: count and merge consecutive non-alnum (== 'is_punctuation') boxes,
	then tag the entire paragraph as non-reflowable when there's 'enough' of this 'graphics' in there.

	Also recognize bullet lists and mark these as such.

	The result is a completed paragraph node tree, which can be traversed by the reflow engine.
	*/
	paragraph_box *root = new paragraph_box();

	set_deferred_cmt_config_params_phase3();
	int rv = grok_the_words(root, words);

	UNC_ASSERT(m_comment_len == strlen(m_comment));
#if 0
	/* debugging: output the 'original text': */
	write2output("\n\n***TEXT***\n");
	write2output(m_comment, m_comment_len);
	write2output("\n***END-OF-TEXT***\n");
#endif

#if 0
	/* debugging: */
	write2output("\n\n***WORDS DUMP***\n");
	dump2output(words);
#endif

#if 0
	/* debugging: */
	//write2output("\n\n***PARAGRAPHS DUMP***\n");
	dump2output(root, words);
#endif

#if 01
	if (m_xml_text_has_stray_lt_gt > 0)
	{
		/* illegal XML/HTML text */
		pretty_print_diagnostic2output(m_comment, m_comment_len,
							m_xml_offender, 1,
							"**XML FORMAT FAILURE**",
							words, root);
	}
#endif

#if 0
	write2output("\n\n***PARAGRAPHS OUTPUT***\n");
#endif
	if (this->m_reflow_mode != 1)
	{
		reflow_para_hierarchy(root, words);
	}
	UNC_ASSERT(m_comment_len == strlen(m_comment));

	/*
	make sure the comment is positioned at a valiable start column, i.e.
	if the m_left_global_output_column setting doesn't make sense in the current situation, adjust
	the m_left_global_output_column to cope.
	*/
	int left_col = m_left_global_output_column;
	int actual_left_col = get_global_block_left_column();
	//int curcol = align_tab_column(left_col + 1);
	if (actual_left_col > left_col)
	{
		m_left_global_output_column = actual_left_col + 1;
	}

	write_comment_to_output(root, words);

#if 0
	show_diagnostics(root, words);
#endif

	delete root;
}





