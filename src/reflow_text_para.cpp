/**
 * @file reflow_text_para.cpp
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






paragraph_box::paragraph_box() :
	m_first_box(-1), m_last_box(-1),
		m_previous_sibling(NULL), m_next_sibling(NULL), m_first_child(NULL), m_parent(NULL),
		m_first_line_indent(0), m_hanging_indent(0),
		m_starts_on_new_line(false),
		m_keep_with_next(0), m_keep_with_prev(0),
		m_is_non_reflowable(false), m_is_boxed_txt(false), m_is_graphics(false),
		m_graphics_trigger_box(-1), m_nonreflow_trigger_box(-1),
		m_indent_as_previous(false), m_continue_from_previous(false),
		m_is_bullet(false), m_is_bulletlist(false), m_bullet_box(-1), m_bulletlist_level(0),
		m_is_doxygen_par(false), m_doxygen_tag_box(-1),
		m_is_xhtml(false), m_is_unclosed_html_tag(false),
		m_is_dangling_xhtml_close_tag(false),
		m_xhtml_start_tag_box(-1), m_xhtml_end_tag_box(-1),
		m_xhtml_start_tag_container(NULL), m_xhtml_end_tag_container(NULL),
		m_leading_whitespace_length(0), m_trailing_whitespace_length(0),
		m_min_required_linebreak_before(0), m_min_required_linebreak_after(0),
		//m_forced_linebreak_before(0), m_forced_linebreak_after(0),
		m_is_math(false), m_is_code(false), m_is_path(false), m_is_intermission(false),
		//m_is_reflow_par(true), // !!!
		m_left_edge_text(NULL), m_left_edge_thickness(0), m_left_edge_trailing_whitespace(0),
		m_right_edge_text(NULL), m_right_edge_thickness(0), m_right_edge_leading_whitespace(0)
{}

paragraph_box::~paragraph_box()
{
	/* remove entire tree from memory */
	if (m_first_child)
		delete m_first_child;
	if (m_next_sibling)
		delete m_next_sibling;
}





/**
Return TRUE when this paragraph can be considered a 'usual text' as considered for
widow, orpan and wordcount-per-line conditions.

Otherwise, return FALSE.
*/
bool paragraph_box::para_is_a_usual_piece_of_text(void) const
{
	if (this->m_is_bullet
		|| this->m_is_bulletlist
		|| this->m_is_boxed_txt
		|| this->m_is_code
		|| this->m_is_dangling_xhtml_close_tag
		|| this->m_is_doxygen_par
		|| this->m_is_graphics
		|| this->m_is_math
		|| this->m_is_non_reflowable
		|| this->m_is_path
		|| this->m_is_unclosed_html_tag
		|| this->m_is_xhtml
		|| !this->m_starts_on_new_line
		|| this->m_is_intermission
		|| this->m_xhtml_end_tag_container
		|| this->m_xhtml_start_tag_container
		)
	{
		return false;
	}
	return true;
}


