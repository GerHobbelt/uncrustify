/**
 * @file output.cpp
 * Does all the output & comment formatting.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include "reflow_text.h"
#include <cstring>
#include <cstdlib>
//#include <cassert>

static chunk_t *output_comment(chunk_t *pc);

/**
 * All output text is sent here, one char at a time.
 */
static void add_char(char ch)
{
   static char last_char = 0;

   /* If we did a '\r' and it isn't followed by a '\n', then output a newline */
   if ((last_char == '\r') && (ch != '\n'))
   {
      fputs(cpd.newline, cpd.fout);
      cpd.column      = 1;
      cpd.did_newline = 1;
      cpd.spaces      = 0;
   }

   /* convert a newline into the LF/CRLF/CR sequence */
   if (ch == '\n')
   {
      fputs(cpd.newline, cpd.fout);
      cpd.column      = 1;
      cpd.did_newline = 1;
      cpd.spaces      = 0;
   }
   else if (ch == '\r')
   {
      /* do not output '\r' */
      cpd.column      = 1;
      cpd.did_newline = 1;
      cpd.spaces      = 0;
   }
   else
   {
      /* Explicitly disallow a tab after a space */
      if ((ch == '\t') && (last_char == ' '))
      {
         int endcol = next_tab_column(cpd.column);
         while ((int)cpd.column < endcol)
         {
            add_char(' ');
         }
         return;
      }
      else if (ch == ' ')
      {
         cpd.spaces++;
         cpd.column++;
      }
      else
      {
         while (cpd.spaces > 0)
         {
            fputc(' ', cpd.fout);
            cpd.spaces--;
         }
         fputc(ch, cpd.fout);
         if (ch == '\t')
         {
            cpd.column = next_tab_column(cpd.column);
         }
         else
         {
            cpd.column++;
         }
      }
   }
   last_char = ch;
}


static void add_text(const char *text)
{
   char ch;

   while ((ch = *text) != 0)
   {
      text++;
      add_char(ch);
   }
}


static void add_text_len(const char *text, size_t len)
{
   while (len-- > 0)
   {
	   UNC_ASSERT(*text);
      add_char(*text);
      text++;
   }
}







/**
 * Advance to a specific column
 * cpd.column is the current column
 *
 * @param column  The column to advance to
 */
static void output_to_column(int column, bool allow_tabs, int max_tabbed_column = -1)
{
	if (allow_tabs)
	   LOG_FMT(LOUTIND, " to_col:%d/%d/%d - ", cpd.column, max_tabbed_column, column);

   if (max_tabbed_column < 0)
	   max_tabbed_column = column;
   else if (max_tabbed_column > column)
	   max_tabbed_column = column;

   cpd.did_newline = 0;
   if (allow_tabs)
   {
      /* tab out as far as possible and then use spaces */
      while (next_tab_column(cpd.column) <= max_tabbed_column)
      {
         add_text("\t");
      }
   }
   /* space out the final bit */
   while ((int)cpd.column < column)
   {
      add_text(" ");
   }
}


/**
 * Output a comment to the column using indent_with_tabs and
 * indent_cmt_with_tabs as the rules.
 * base_col is the indent of the first line of the comment.
 * On the first line, column == base_col.
 * On subsequnet lines, column >= base_col.
 *
 * @param brace_col the brace-level indent of the comment
 * @param base_col  the indent of the start of the comment (multiline)
 * @param column    the column that we should end up in
 */
static void cmt_output_indent(int brace_col, int base_col, int column)
{
   int iwt;
   int tab_col;

   iwt = cpd.settings[UO_indent_cmt_with_tabs].b ? 2 :
         (cpd.settings[UO_indent_with_tabs].n ? 1 : 0);

   tab_col = (iwt == 0) ? 0 : ((iwt == 1) ? brace_col : base_col);

   //LOG_FMT(LSYS, "%s(brace=%d base=%d col=%d iwt=%d) tab=%d cur=%d\n",
   //        __func__, brace_col, base_col, column, iwt, tab_col, cpd.column);

   cpd.did_newline = 0;
   if ((iwt == 2) || ((cpd.column == 1) && (iwt == 1)))
   {
      /* tab out as far as possible and then use spaces */
      while (next_tab_column(cpd.column) <= tab_col)
      {
         add_text("\t");
      }
   }

   /* space out the rest */
   while (cpd.column < column)
   {
      add_text(" ");
   }
}


void output_parsed(FILE *pfile)
{
   chunk_t *pc;
   int     cnt;

   output_options(pfile);
   output_defines(pfile);
   output_types(pfile);

   fprintf(pfile, "-=====-\n");
   fprintf(pfile, "Line      Tag          Parent     Columns  Br/Lvl/pp Flag Nl  Text");
   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      fprintf(pfile, "\n%3d> %13.13s[%13.13s][%2d/%2d/%2d][%d/%d/%d][%10" PRIx64 "][%d-%d]",
              pc->orig_line, get_token_name(pc->type),
              get_token_name(pc->parent_type),
              pc->column, pc->orig_col, pc->orig_col_end,
              pc->brace_level, pc->level, pc->pp_level,
              pc->flags, pc->nl_count, pc->after_tab);

      if ((pc->type != CT_NEWLINE) && (pc->len != 0))
      {
         for (cnt = 0; cnt < pc->column; cnt++)
         {
            fprintf(pfile, " ");
         }
         if (pc->type != CT_NL_CONT)
         {
            fprintf(pfile, "%.*s", pc->len, pc->str);
         }
         else
         {
            fprintf(pfile, "\\");
         }
      }
   }
   fprintf(pfile, "\n-=====-\n");
   fflush(pfile);
}


void output_options(FILE *pfile)
{
   int idx;
   const option_map_value *ptr;

   fprintf(pfile, "-== Options ==-\n");
   for (idx = 0; idx < UO_option_count; idx++)
   {
      ptr = get_option_name(idx);
      if (ptr != NULL)
      {
         if (ptr->type == AT_STRING)
         {
            fprintf(pfile, "%3d) %32s = \"%s\"\n",
                    ptr->id, ptr->name,
                    op_val_to_string(ptr->type, cpd.settings[ptr->id]).c_str());
         }
         else
         {
            fprintf(pfile, "%3d) %32s = %s\n",
                    ptr->id, ptr->name,
                    op_val_to_string(ptr->type, cpd.settings[ptr->id]).c_str());
         }
      }
   }
}


/**
 * This renders the chunk list to a file.
 */
void output_text(FILE *pfile)
{
   chunk_t *pc;
   chunk_t *prev;
   int     cnt;
   int     lvlcol;
   bool    allow_tabs;

   cpd.fout = pfile;

   cpd.did_newline = 1;
   cpd.column      = 1;

   if (cpd.bom != NULL)
   {
      add_text_len(cpd.bom->str, cpd.bom->len);
      cpd.did_newline = 1;
      cpd.column      = 1;
   }

   if (cpd.frag_cols > 0)
   {
      int indent = cpd.frag_cols - 1;

      for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
      {
         pc->column        += indent;
         pc->column_indent += indent;
      }
      cpd.frag_cols = 0;
   }

   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      if (pc->type == CT_NEWLINE)
      {
         for (cnt = 0; cnt < pc->nl_count; cnt++)
         {
            add_char('\n');
         }
         cpd.did_newline = 1;
         cpd.column      = 1;
         LOG_FMT(LOUTIND, " xx\n");
      }
      else if (pc->type == CT_NL_CONT)
      {
         /* FIXME: this really shouldn't be done here! */
         if ((pc->flags & PCF_WAS_ALIGNED) == 0)
         {
            if (cpd.settings[UO_sp_before_nl_cont].a & AV_REMOVE)
            {
               pc->column = cpd.column + (cpd.settings[UO_sp_before_nl_cont].a == AV_FORCE);
            }
            else
            {
               /* Try to keep the same relative spacing */
               prev = chunk_get_prev(pc);
               while ((prev != NULL) && (prev->orig_col == 0) && (prev->nl_count == 0))
               {
                  prev = chunk_get_prev(prev);
               }

               if ((prev != NULL) && (prev->nl_count == 0))
               {
				   UNC_ASSERT(pc->orig_col >= prev->orig_col_end);
                  int orig_sp = (pc->orig_col - prev->orig_col_end);
                  pc->column = cpd.column + orig_sp;
				  UNC_ASSERT(pc->column >= 0);
                  if ((cpd.settings[UO_sp_before_nl_cont].a != AV_IGNORE) &&
                      (pc->column < (int)(cpd.column + 1)))
                  {
                     pc->column = cpd.column + 1;
                  }
               }
            }
         }
         output_to_column(pc->column, (cpd.settings[UO_indent_with_tabs].n == 2));
         add_char('\\');
         add_char('\n');
         cpd.did_newline = 1;
         cpd.column      = 1;
         LOG_FMT(LOUTIND, " \\xx\n");
      }
      else if (pc->type == CT_COMMENT_MULTI
		  || pc->type == CT_COMMENT_CPP
		  || pc->type == CT_COMMENT)
      {
         pc = output_comment(pc);
      }
      else if (pc->type == CT_JUNK)
      {
         /* do not adjust the column for junk */
         add_text_len(pc->str, pc->len);
      }
      else if (pc->len == 0)
      {
         /* don't do anything for non-visible stuff */
         LOG_FMT(LOUTIND, " <%d> -", pc->column);
      }
      else
      {
		int lvl = pc->brace_level * cpd.settings[UO_indent_columns].n + 1;

         /* indent to the 'level' first */
         if (cpd.did_newline)
         {
            if (cpd.settings[UO_indent_with_tabs].n == 1)
            {
               /* FIXME: it would be better to properly set column_indent in
                * indent_text(), but this hack for '}' and ':' seems to work. */
               if ((pc->type == CT_BRACE_CLOSE) ||
                   chunk_is_str(pc, ":", 1) ||
                   (pc->type == CT_PREPROC))
               {
                  lvlcol = pc->column;
               }
               else
               {
                  lvlcol = pc->column_indent;
                  if (lvlcol > pc->column)
                  {
                     lvlcol = pc->column;
                  }
               }

               if (lvlcol > 1)
               {
                  output_to_column(lvlcol, true);
               }
            }
            allow_tabs = (cpd.settings[UO_indent_with_tabs].n == 2) ||
                         (chunk_is_comment(pc) &&
                          (cpd.settings[UO_indent_with_tabs].n != 0));

			LOG_FMT(LOUTIND, "  %d> col %d/%d/%d/%d lvl:%d/%d/%d - ", pc->orig_line, pc->column, cpd.column, pc->column_indent, lvl, pc->brace_level, pc->pp_level, pc->level);
         }
         else
         {
            /**
             * Reformatting multi-line comments can screw up the column.
             * Make sure we don't mess up the spacing on this line.
             * This has to be done here because comments are not formatted
             * until the output phase.
             */
            if (pc->column < (int)cpd.column)
            {
               reindent_line(pc, cpd.column);
            }

            /* not the first item on a line */
            if (cpd.settings[UO_align_keep_tabs].b)
            {
               allow_tabs = pc->after_tab;
            }
            else
            {
               prev       = chunk_get_prev(pc);
               allow_tabs = (cpd.settings[UO_align_with_tabs].b &&
                             ((pc->flags & PCF_WAS_ALIGNED) != 0) &&
                             ((prev->column + prev->len + 1) != pc->column));
            }
            LOG_FMT(LOUTIND, " %d(%d)/%d -", pc->column, allow_tabs, lvl);
         }

         output_to_column(pc->column, allow_tabs);
         add_text_len(pc->str, pc->len);
         cpd.did_newline = chunk_is_newline(pc);
      }
   }
}




#if 0

/**
 * text starts with '$('. see if this matches a keyword and add text based
 * on that keyword.
 * @return the number of characters eaten from the text
 */
static int add_comment_kw(const char *text, int len, cmt_reflow& cmt)
{
   if ((len >= 11) && (memcmp(text, "$(filename)", 11) == 0))
   {
      add_text(path_basename(cpd.filename));
      return(11);
   }
   if ((len >= 8) && (memcmp(text, "$(class)", 8) == 0))
   {
      chunk_t *tmp = get_next_class(cmt.pc);
      if (tmp != NULL)
      {
         add_text_len(tmp->str, tmp->len);
         return(8);
      }
   }

   /* If we can't find the function, we are done */
   chunk_t *fcn = get_next_function(cmt.pc);
   if (fcn == NULL)
   {
      return(0);
   }

   if ((len >= 10) && (memcmp(text, "$(message)", 10) == 0))
   {
      add_text_len(fcn->str, fcn->len);
      chunk_t *tmp = chunk_get_next_ncnl(fcn);
      chunk_t *word = NULL;
      while (tmp)
      {
         if ((tmp->type == CT_BRACE_OPEN) || (tmp->type == CT_SEMICOLON))
         {
            break;
         }
         if (tmp->type == CT_OC_COLON)
         {
            if (word != NULL)
            {
               add_text_len(word->str, word->len);
               word = NULL;
            }
            add_text_len(":", 1);
         }
         if (tmp->type == CT_WORD)
         {
            word = tmp;
         }
         tmp = chunk_get_next_ncnl(tmp);
      }
      return(10);
   }
   if ((len >= 11) && (memcmp(text, "$(function)", 11) == 0))
   {
      if (fcn->parent_type == CT_OPERATOR)
      {
         add_text_len("operator ", 9);
      }
      add_text_len(fcn->str, fcn->len);
      return(11);
   }
   if ((len >= 12) && (memcmp(text, "$(javaparam)", 12) == 0))
   {
      add_comment_javaparam(fcn, cmt);
      return(12);
   }
   if ((len >= 9) && (memcmp(text, "$(fclass)", 9) == 0))
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
         add_text_len(tmp->str, tmp->len);
         return(9);
      }
   }
   return(0);
}


static int next_up(const char *text, int text_len, const char *tag)
{
   int offs    = 0;
   int tag_len = strlen(tag);

   while (unc_isspace(*text) && (text_len > 0))
   {
      text++;
      text_len--;
      offs++;
   }

   if ((tag_len <= text_len) && (memcmp(text, tag, tag_len) == 0))
   {
      return(offs);
   }
   return(-1);
}


/**
 * Outputs a comment. The initial opening '//' may be included in the text.
 * Subsequent openings (if combining comments), should not be included.
 * The closing (for C/D comments) should not be included.
 *
 * TODO:
 * If reflowing text, the comment should be added one word (or line) at a time.
 * A newline should only be sent if a blank line is encountered or if the next
 * line is indented beyond the current line (optional?).
 * If the last char on a line is a ':' or '.', then the next line won't be
 * combined.
 */
static void add_comment_text(const char *text, int len,
                             cmt_reflow& cmt, bool esc_close)
{
   bool was_star   = false;
   bool was_slash  = false;
   bool was_dollar = false;
   bool in_word    = false;
   int  tmp;

   for (int idx = 0; idx < len; idx++)
   {
      if (!was_dollar && cmt.kw_subst &&
          (text[idx] == '$') && (len > (idx + 3)) && (text[idx + 1] == '('))
      {
         idx += add_comment_kw(&text[idx], len - idx, cmt);
         if (idx >= len)
         {
            break;
         }
      }

      /* Split the comment */
      if (text[idx] == '\n')
      {
         in_word = false;
         add_char('\n');
         cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
         if (cmt.xtra_indent)
         {
            add_char(' ');
         }

         /* hack to get escaped newlines to align and not dup the leading '//' */
         tmp = next_up(text + idx + 1, len - (idx + 1), cmt.cont_text);
         if (tmp < 0)
         {
            add_text(cmt.cont_text);
         }
         else
         {
            idx += tmp;
         }
      }
      else if (cmt.reflow &&
               (text[idx] == ' ') &&
               (cpd.settings[UO_cmt_width].n > 0) &&
               ((cpd.column > cpd.settings[UO_cmt_width].n) ||
                next_word_exceeds_limit(text + idx)))
      {
         in_word = false;
         add_char('\n');
         cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
         if (cmt.xtra_indent)
         {
            add_char(' ');
         }
         add_text(cmt.cont_text);
      }
      else
      {
         /* Escape a C closure in a CPP comment */
         if (esc_close &&
             ((was_star && (text[idx] == '/')) ||
              (was_slash && (text[idx] == '*'))))
         {
            add_char(' ');
         }
         if (!in_word && !unc_isspace(text[idx]))
         {
            cmt.word_count++;
         }
         in_word = !unc_isspace(text[idx]);
         add_char(text[idx]);
         was_star   = (text[idx] == '*');
         was_slash  = (text[idx] == '/');
         was_dollar = (text[idx] == '$');
      }
   }
}


static void output_cmt_start(cmt_reflow& cmt, chunk_t *pc)
{
   cmt.pc          = pc;
   cmt.column      = pc->column;
   cmt.brace_col   = pc->column_indent;
   cmt.base_col    = pc->column_indent;
   cmt.word_count  = 0;
   cmt.kw_subst    = false;
   cmt.xtra_indent = 0;
   cmt.cont_text   = "";
   cmt.reflow      = false;

   if (cmt.brace_col == 0)
   {
      cmt.brace_col = 1 + (pc->brace_level * cpd.settings[UO_output_tab_size].n);
   }

   //LOG_FMT(LSYS, "%s: line %d, brace=%d base=%d col=%d orig=%d aligned=%x\n",
   //        __func__, pc->orig_line, cmt.brace_col, cmt.base_col, cmt.column, pc->orig_col,
   //        pc->flags & (PCF_WAS_ALIGNED | PCF_RIGHT_COMMENT));

   if ((pc->parent_type == CT_COMMENT_START) ||
       (pc->parent_type == CT_COMMENT_WHOLE))
   {
      if (!cpd.settings[UO_indent_col1_comment].b &&
          (pc->orig_col == 1) &&
          !(pc->flags & PCF_INSERTED))
      {
         cmt.column    = 1;
         cmt.base_col  = 1;
         cmt.brace_col = 1;
      }
   }
   else if (pc->parent_type == CT_COMMENT_END)
   {
      /* Make sure we have at least one space past the last token */
      chunk_t *prev = chunk_get_prev(pc);
      if (prev != NULL)
      {
         int col_min = prev->column + prev->len + 1;
         if (cmt.column < col_min)
         {
            cmt.column = col_min;
         }
      }
   }

   /* tab aligning code */
   if (cpd.settings[UO_indent_cmt_with_tabs].b &&
       ((pc->parent_type == CT_COMMENT_END) ||
        (pc->parent_type == CT_COMMENT_WHOLE)))
   {
      cmt.column = align_tab_column(cmt.column - 1);
      //LOG_FMT(LSYS, "%s: line %d, orig:%d new:%d\n",
      //        __func__, pc->orig_line, pc->column, cmt.column);
      pc->column = cmt.column;
   }
   cmt.base_col = cmt.column;

   //LOG_FMT(LSYS, "%s: -- brace=%d base=%d col=%d\n",
   //        __func__, cmt.brace_col, cmt.base_col, cmt.column);

   /* Bump out to the column */
   cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);

   cmt.kw_subst = (pc->flags & PCF_INSERTED) != 0;
}


/**
 * Checks to see if the current comment can be combined with the next comment.
 * The two can be combined if:
 *  1. They are the same type
 *  2. There is exactly one newline between then
 *  3. They are indented to the same level
 */
static bool can_combine_comment(chunk_t *pc, cmt_reflow& cmt)
{
   /* We can't combine if there is something other than a newline next */
   if (pc->parent_type == CT_COMMENT_START)
   {
      return(false);
   }

   /* next is a newline for sure, make sure it is a single newline */
   chunk_t *next = chunk_get_next(pc);
   if ((next != NULL) && (next->nl_count == 1))
   {
      /* Make sure the comment is the same type at the same column */
      next = chunk_get_next(next);
      if ((next != NULL) &&
          (next->type == pc->type) &&
          (((next->column == 1) && (pc->column == 1)) ||
           ((next->column == cmt.base_col) && (pc->column == cmt.base_col)) ||
           ((next->column > cmt.base_col) && (pc->parent_type == CT_COMMENT_END))))
      {
         return(true);
      }
   }
   return(false);
}

#endif


/**
 * Outputs the C/C++ comment at pc.
 * Comment combining is done here as well.
 *
 * @return the last chunk output'd
 */
static chunk_t *output_comment(chunk_t *pc)
{
   cmt_reflow cmt;

   /* See if we can combine this comment with the next comment */
   while (cmt.can_combine_comment(pc))
   {
	  cmt.m_is_merged_comment = true;
      cmt.push_chunk(pc);
	  cmt.push("\n");
      pc = chunk_get_next(chunk_get_next(pc));
   }
   cmt.push_chunk(pc);
   cmt.m_last_pc = pc;

   cmt.render();

   return pc;
}






void cmt_reflow::write(char ch)
{
	UNC_ASSERT(ch);
	if (ch == NONBREAKING_SPACE_CHAR)
	{
		ch = ' ';
	}
	::add_char(ch);
}

void cmt_reflow::write(const char *str)
{
	UNC_ASSERT(str);
	UNC_ASSERT(*str);
	::add_text(str);
}

void cmt_reflow::write(const char *str, size_t len)
{
	UNC_ASSERT(str);
	UNC_ASSERT(len);
	UNC_ASSERT(*str);
	::add_text_len(str, len);
}

#if 0

void cmt_reflow::output_to_column(int column, bool allow_tabs, int max_tabbed_column)
{
   int        cmt_col = pc->column;
   const char *cmt_str;
   int        remaining;
   char       ch;
   char       *line = new char[1024 + pc->len];
   int        line_len;
   int        line_count = 0;
   int        ccol;
   int        col_diff = 0;
   bool       nl_end   = false;
   cmt_reflow cmt;

   output_cmt_start(cmt, pc);

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

   ccol      = pc->column;
   remaining = pc->len;
   cmt_str   = pc->str;
   line_len  = 0;
   while (remaining > 0)
   {
      ch = *cmt_str;
      cmt_str++;
      remaining--;

      /* handle the CRLF and CR endings. convert both to LF */
      if (ch == '\r')
      {
         ch = '\n';
         if (*cmt_str == '\n')
         {
            cmt_str++;
            remaining--;
         }
      }

      /* Find the start column */
      if (line_len == 0)
      {
         nl_end = false;
         if (ch == ' ')
         {
            ccol++;
            continue;
         }
         else if (ch == '\t')
         {
            ccol = calc_next_tab_column(ccol, cpd.settings[UO_input_tab_size].n);
            continue;
         }
         else
         {
            //LOG_FMT(LSYS, "%d] Text starts in col %d, col_diff=%d, real=%d\n",
            //        line_count, ccol, col_diff, ccol - col_diff);
         }
      }

      line[line_len++] = ch;

      /* If we just hit an end of line OR we just hit end-of-comment... */
      if ((ch == '\n') || (remaining == 0))
      {
         line_count++;

         /* strip trailing tabs and spaces before the newline */
         if (ch == '\n')
         {
            line_len--;
            nl_end = true;

            /* Say we aren't in a preproc to prevent changing any bs-nl */
            cmt_trim_whitespace(line_len, line, false);
         }
         line[line_len] = 0;

         if (line_count > 1)
         {
            ccol -= col_diff;
         }

         if (line_len > 0)
         {
            cmt.column = ccol;
            cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
            add_text_len(line, line_len);
         }
         if (nl_end)
         {
            add_char('\n');
         }
         line_len = 0;
         ccol     = 1;
      }
   }
   delete line;
}


/**
 * This renders the #if condition to a string buffer.
 */
static void generate_if_conditional_as_text(string& dst, chunk_t *ifdef)
{
   chunk_t *pc;
   int     column = -1;

   dst.erase();
   for (pc = ifdef; pc != NULL; pc = chunk_get_next(pc))
   {
      if (column == -1)
      {
         column = pc->column;
      }
      if ((pc->type == CT_NEWLINE) ||
          (pc->type == CT_COMMENT_MULTI) ||
          (pc->type == CT_COMMENT_CPP))
      {
         break;
      }
      else if (pc->type == CT_NL_CONT)
      {
         dst   += ' ';
         column = -1;
      }
      else if ((pc->type == CT_COMMENT) ||
               (pc->type == CT_COMMENT_EMBED))
      {
      }
      else // if (pc->type == CT_JUNK) || else
      {
         int spacing;

         for (spacing = pc->column - column; spacing > 0; spacing--)
         {
            dst += ' ';
            column++;
         }
         dst.append(pc->str, pc->len);
         column += pc->len;
      }
   }
}


/*
 * See also it's preprocessor counterpart
 *   add_long_closebrace_comment
 * in braces.cpp
 *
 * Note: since this concerns itself with the preprocessor -- which is line-oriented --
 * it turns out that just looking at pc->pp_level is NOT the right thing to do.
 * See a --parsed dump if you don't believe this: an '#endif' will be one level
 * UP from the corresponding #ifdef when you look at the tokens 'ifdef' versus 'endif',
 * but it's a whole another story when you look at their CT_PREPROC ('#') tokens!
 *
 * Hence we need to track and seek matching CT_PREPROC pp_levels here, which complicates
 * things a little bit, but not much.
 */
void add_long_preprocessor_conditional_block_comment(void)
{
   chunk_t *pc;
   chunk_t *tmp;
   chunk_t *br_open;
   chunk_t *br_close;
   chunk_t *pp_start = NULL;
   chunk_t *pp_end   = NULL;
   int     nl_count;

   for (pc = chunk_get_head(); pc; pc = chunk_get_next_ncnl(pc))
   {
      /* just track the preproc level: */
      if (pc->type == CT_PREPROC)
      {
         pp_end = pp_start = pc;
      }

      if (pc->type != CT_PP_IF)
      {
         continue;
      }
#if 0
      if ((pc->flags & PCF_IN_PREPROC) != 0)
      {
         continue;
      }
#endif

      br_open  = pc;
      nl_count = 0;

      tmp = pc;
      while ((tmp = chunk_get_next(tmp)) != NULL)
      {
         /* just track the preproc level: */
         if (tmp->type == CT_PREPROC)
         {
            pp_end = tmp;
         }

         if (chunk_is_newline(tmp))
         {
            nl_count += tmp->nl_count;
         }
         else if ((pp_end->pp_level == pp_start->pp_level) &&
                  ((tmp->type == CT_PP_ENDIF) ||
                   (br_open->type == CT_PP_IF ? tmp->type == CT_PP_ELSE : 0)))
         {
            br_close = tmp;

            LOG_FMT(LPPIF, "found #if / %s section on lines %d and %d, nl_count=%d\n",
                    (tmp->type == CT_PP_ENDIF ? "#endif" : "#else"),
                    br_open->orig_line, br_close->orig_line, nl_count);

            /* Found the matching #else or #endif - make sure a newline is next */
            tmp = chunk_get_next(tmp);

            LOG_FMT(LPPIF, "next item type %d (is %s)\n",
                    (tmp ? tmp->type : -1), (tmp ? chunk_is_newline(tmp) ? "newline"
                                             : chunk_is_comment(tmp) ? "comment" : "other" : "---"));
            if ((tmp == NULL) || (tmp->type == CT_NEWLINE) /* chunk_is_newline(tmp) */)
            {
               int nl_min;

               if (br_close->type == CT_PP_ENDIF)
               {
                  nl_min = cpd.settings[UO_mod_add_long_ifdef_endif_comment].n;
               }
               else
               {
                  nl_min = cpd.settings[UO_mod_add_long_ifdef_else_comment].n;
               }

               const char *txt = !tmp ? "EOF" : ((tmp->type == CT_PP_ENDIF) ? "#endif" : "#else");
               LOG_FMT(LPPIF, "#if / %s section candidate for augmenting when over NL threshold %d != 0 (nl_count=%d)\n",
                       txt, nl_min, nl_count);

               if ((nl_min > 0) && (nl_count > nl_min)) /* nl_count is 1 too large at all times as #if line was counted too */
               {
                  /* determine the added comment style */
                  c_token_t style = (cpd.lang_flags & (LANG_CPP | LANG_CS)) ?
                                    CT_COMMENT_CPP : CT_COMMENT;

                  string str;
                  generate_if_conditional_as_text(str, br_open);

                  LOG_FMT(LPPIF, "#if / %s section over threshold %d (nl_count=%d) --> insert comment after the %s: %s\n",
                          txt, nl_min, nl_count, txt, str.c_str());

                  /* Add a comment after the close brace */
                  insert_comment_after(br_close, style, str.size(), str.c_str());
               }
            }

            /* checks both the #else and #endif for a given level, only then look further in the main loop */
            if (br_close->type == CT_PP_ENDIF)
            {
               break;
            }
         }
      }
   }
}

#else

void cmt_reflow::output_to_column(int column, bool allow_tabs, int max_tabbed_column)
{
	::output_to_column(column, allow_tabs, max_tabbed_column);
}

#endif

