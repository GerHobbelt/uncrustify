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
void output_to_column(int column, bool allow_tabs, int max_tabbed_column = -1)
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




