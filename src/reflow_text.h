/**
 * @file reflow_text.h
 *
 * @author  Ben Gardner / Ger Hobbelt
 * @license GPL v2+
 *
 * $Id: token_enum.h 1533 2009-04-15 01:43:50Z bengardner $
 */
#ifndef REFLOW_TEXT_H_INCLUDED
#define REFLOW_TEXT_H_INCLUDED

#include <limits.h>   // INT_MAX
#include <stdlib.h>   // free


struct reflow_box;
struct paragraph_box;
class words_collection;
class break_suggestions;
struct reflow_tune_parameters_t;

class cmt_reflow
{
	friend class words_collection;
	friend struct reflow_tune_parameters_t;

public:
   chunk_t    *m_first_pc;
   chunk_t    *m_last_pc;    /* == first_pc when not a grouped set of original comments */

protected:
   int        m_left_global_output_column;      /* Column of the comment start */
   int        m_brace_col;   /* Brace column (for indenting with tabs) */
   int        m_base_col;    /* Base column (for indenting with tabs) */
   int        m_word_count;  /* number of words on this line */
   bool       m_kw_subst;    /* do keyword substitution */
#if 0 /* [i_a] 0.56 has this, where I have 'extra_pre_star_indent' */
   int        m_xtra_indent; /* extra indent of non-first lines (0 or 1) */
#endif
   //const char *m_cont_text;  /* fixed text to output at the start of a line (0 to 3 chars) */
   int        m_reflow_mode; /* reflow mode for the current text */
   bool       m_is_cpp_comment;
public:
   bool       m_is_merged_comment;
protected:
   bool	      m_is_single_line_comment;
   int        m_extra_pre_star_indent; /* 0 or 1: extra number of characters to indent for comment line 2+ */
   int        m_extra_post_star_indent; /* 0 or 1: extra number of characters to indent for comment line 2+ */
   bool       m_has_leading_nl;
   bool       m_has_trailing_nl;
	bool    m_indent_cmt_with_tabs; /* cpd.settings[UO_indent_cmt_with_tabs].b */
	int m_cmt_reflow_graphics_threshold; /* cpd.settings[UO_cmt_reflow_graphics_threshold].n */
	int m_cmt_reflow_box_threshold; /* cpd.settings[UO_cmt_reflow_box_threshold].n */
	const char *m_cmt_reflow_box_markers; /* cpd.settings[UO_cmt_reflow_box_markers].str */
	bool m_cmt_reflow_box; /* cpd.settings[UO_cmt_reflow_box].b */
	const char *m_cmt_reflow_graphics_markers; /* cpd.settings[UO_cmt_reflow_graphics_markers].str */
	const char *m_cmt_reflow_no_line_reflow_markers_at_SOL; /* cpd.settings[UO_cmt_reflow_no_line_reflow_markers_at_SOL].str */
	const char *m_cmt_reflow_no_par_reflow_markers_at_SOL; /* cpd.settings[UO_cmt_reflow_no_par_reflow_markers_at_SOL].str */
	const char *m_cmt_reflow_no_cmt_reflow_markers_at_SOL; /* cpd.settings[UO_cmt_reflow_no_cmt_reflow_markers_at_SOL].str */
	const char *m_cmt_reflow_bullets; /* cpd.settings[UO_cmt_reflow_bullets].str */
	const char *m_cmt_reflow_bullet_terminators; /* cpd.settings[UO_cmt_reflow_bullet_terminators].str */
	const char *m_cmt_reflow_SOL_markers; /* cpd.settings[UO_cmt_reflow_SOL_markers].str */
	int m_string_escape_char; /* cpd.settings[UO_string_escape_char].n */
	bool m_comment_is_part_of_preproc_macro;
	int m_cmt_reflow_overshoot; /* cpd.settings[UO_cmt_reflow_overshoot].n */
	int m_cmt_reflow_minimum_words_per_line; /* cpd.settings[UO_cmt_reflow_minimum_words_per_line].n */
	int m_cmt_reflow_intermission_indent_threshold; /* cpd.settings[UO_cmt_reflow_intermission_indent_threshold].n */

   /**
   0: surely NO; +1/+2: surely YES; -1: don't know yet.

   This member is relevant for detecting XML/HTML comments which turn out to
   NOT be such; this can have various reasons, but it always comes down to the
   comment parser getting the impression that some XML/HTML tag is ill formatted (code +1)
   or the text contains at least one dangling '<', '>' or has 'nested '<' characters (which is
   downright illegal in XML/HTML as well) (code: +2)

   When the comment turned out to be a legal-ish XML/HTML comment, you'll get a (code: 0).

   When the comment isn't even suspected of being XML/HTML (most probably due to it not having
   any '<' in there), the code remains (code: -1).
   */
   int m_xml_text_has_stray_lt_gt;
   const char *m_xml_offender; /* point in text which caused the parser to give up on assuming this to be XML/HTML */


protected:
   char *m_comment;  /* the entire comment string, sans comment markers */
   size_t m_comment_len; /* (used) length of the 'comment' string buffer, excluding NUL sentinel */
   size_t m_comment_size; /* allocated size of the 'comment' string buffer */

   int m_orig_startcolumn;     /* Column at which the text was positioned; used while adding comment text */

   int m_lead_cnt; /* number of '*' lead characters used for each comment line [0..2] */
   char *m_lead_marker; /* the exact 'lead/prefix' string used for this comment (not necessarily "*") */

   bool m_is_doxygen_comment;
   bool m_is_backreferencing_doxygen_comment; /* is a doxygen/javadoc section which documents a PRECEDING item */
   char *m_doxygen_marker; /* the detected 'doxygen/javadoc' marker at the start of this comment */

	/* configuration settings */
protected:
	const char **m_no_reflow_marker_start;
	const char **m_no_reflow_marker_end;
	int m_line_wrap_column;
	int m_tab_width;
	const char *m_defd_lead_markers;

public:
   cmt_reflow();
   ~cmt_reflow();

	void push_chunk(chunk_t *pc);
	void push(const char *text);
	void push(const char *text, size_t len);
	void push(char c, size_t repeat_count);

	bool can_combine_comment(chunk_t *pc);

	void render(void);

protected:
	//void calculate_comment_body_indent(const char *str, int len); -- obsoleted
	//int get_line_leader(const char *str, int len); -- obsoleted

	void set_doxygen_marker(const char *marker, size_t len);

	bool detect_as_javadoc_chunk(chunk_t *pc, bool setup = false);
	static bool chunk_is_inline_comment(const chunk_t *pc);
	static bool is_viable_bullet_marker(const char *str, size_t len);
	static bool is_doxygen_tagmarker(const char *text, char doxygen_tag_marker);
	bool comment_is_part_of_preproc_macro(void) const;

	int add_kw(const char *text);
	void add_javaparam(chunk_t *pc);

	void output_start(chunk_t *pc);
    void push_text(const char *text, int len = -1, bool esc_close = false, int first_extra_offset = 0, int at_column = -1, chunk_t *pc = NULL); // formerly: add_comment_text()

	size_t expand_tabs_and_clean(char **dst_ref, size_t *dstlen_ref, const char *src, size_t srclen, int first_column, bool part_of_preproc_continuation);
	int strip_nonboxed_lead_markers(char *text, int at_column);
	void strip_first_and_last_nl_from_text(void);
	void count_graphics_nonreflow_and_printable_chars(const char *text, int len, int *graph_countref, int *nonreflow_countref, int *print_countref);

	void set_cmt_config_params(void);
	void set_deferred_cmt_config_params_phase1(void);
	void set_deferred_cmt_config_params_phase2(void);
	void set_deferred_cmt_config_params_phase3(void);
	void set_no_reflow_markers(const char *start_tags, const char *end_tags);

	void chop_text_into_reflow_boxes(words_collection &words);

	void optimize_reflow_boxes(words_collection &words);

	int grok_the_words(paragraph_box *root, words_collection &words);

	void expand_math_et_al_markers(words_collection &words);

	typedef struct render_estimates_t
	{
		int render_width;
		int previous_preferred_break_box_idx;
		int next_preferred_break_box_idx;
		int previous_preferred_break_width;
		int next_preferred_break_width;
	} render_estimates_t;

	void estimate_render_width(paragraph_box *para, words_collection &words, int start_box_idx, int last_box_idx, int deferred_ws, render_estimates_t &info);
	int estimate_box_print_width(paragraph_box *para, words_collection &words, int box_idx, int *last_box_for_this_bit = NULL);

	typedef struct window_orphan_info_t
	{
		int widow_first_box_idx;
		int orphan_last_box_idx;
		int widow_render_width;
		int orphan_render_width;
	} window_orphan_info_t;

	void calculate_widow_and_orphan_aspects(paragraph_box *para, words_collection &words, int line_width, window_orphan_info_t &info);

	int find_the_paragraph_boundaries(paragraph_box *para, words_collection &words, int current_box_idx, int &deferred_newlines);
	void fixup_paragraph_tree(paragraph_box *para);
	void adjust_para_last_box(paragraph_box *para, int pos);
	int skip_tailing_newline_box(paragraph_box *para, words_collection &words, int current_box_idx, int min_nl_count, int &deferred_newlines);
	static void push_tag_piece_and_possible_newlines(words_collection &words, const char *&s, int &word_idx, reflow_box *&current_word, const char *&last_nl);

	void resize_buffer(size_t extralen);

	void dump2output(paragraph_box *para, words_collection &words);
	void dump2output(words_collection &words, int mode = 0, int start_idx = 0, int end_idx = INT_MAX);

	static paragraph_box *get_last_sibling(paragraph_box *para);

	int there_is_no_newline_up_ahead(paragraph_box *para, words_collection &words, int current_box_idx);
	int reflow_a_single_para_4_trial(paragraph_box *para, words_collection &words, break_suggestions &scoring, reflow_tune_parameters_t &tuning);
	int reflow_para_tree_4_trial(paragraph_box *para, words_collection &words, break_suggestions &scoring, reflow_tune_parameters_t &tuning);
	void determine_optimal_para_reflow(paragraph_box *para, words_collection &words, reflow_tune_parameters_t &tuning);
	void reflow_para_hierarchy(paragraph_box *para, words_collection &words);

	void write_comment_to_output(paragraph_box *para, words_collection &words);

	int get_global_block_left_column(void);
	void write_line_to_initial_column(void);

	int write2out_comment_start(paragraph_box *para, words_collection &words);
	int write2out_comment_next_line(void);
	void write2out_comment_end(int deferred_whitespace, int deferred_nl);

	void write2output(const char *text, size_t len);
	void write2output(const char *text);

	void write(const char *text, size_t len);
	void write(const char *str);
	void write(char ch);

	void pretty_print_diagnostic2output(const char *text, size_t len, const char *offender, size_t offender_len, const char *report_header, words_collection &words, paragraph_box *para);
	size_t write_offender_text2output(const char *offender, size_t offender_len, size_t *marker_start, size_t *marker_end, bool do_print = true);
	void show_diagnostics(paragraph_box *root, words_collection &words);
};




#endif
