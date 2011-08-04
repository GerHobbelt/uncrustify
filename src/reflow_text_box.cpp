/**
 * @file reflow_text_box.cpp
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





/**
Return TRUE when this box can be considered part of a 'usual text' as considered for
widow, orpan and wordcount-per-line conditions.

Otherwise, return FALSE.
*/
bool reflow_box::box_is_a_usual_piece_of_text(bool count_basic_punctuation_as_unusual) const
{
	if (this->m_is_bullet
		|| this->m_is_cdata_xml_chunk
		|| this->m_is_code
		|| this->m_is_doxygen_tag
		|| this->m_is_inline_javadoc_tag
		|| this->m_is_escape_code
		|| this->m_is_math
		|| this->m_is_non_reflowable
		|| this->m_is_part_of_boxed_txt
		|| this->m_is_part_of_graphical_txt
		//|| this->m_is_part_of_quoted_txt
		|| this->m_is_path
		|| (count_basic_punctuation_as_unusual && this->m_is_punctuation)
		|| this->m_is_quote
		|| this->m_is_unclosed_xhtml_start_tag
		|| this->m_is_unmatched_xhtml_end_tag
		|| this->m_is_uri_or_email
		|| this->m_is_xhtml_end_tag
		|| this->m_is_xhtml_entity
		|| this->m_is_xhtml_start_tag
		|| this->m_is_xhtml_tag_part
		// || this->m_line_count > 0
		)
	{
		return false;
	}
	return true;
}


