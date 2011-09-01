AUTOMAKE_OPTIONS = foreign no-dependencies


DISTCLEANFILES =
CLEANFILES = *.o *~ *.bak
MAINTAINERCLEANFILES = Makefile.in


bin_PROGRAMS = uncrustify

BUILT_SOURCES = token_names.h

uncrustify_SOURCES = uncrustify.cpp chunk_list.cpp indent.cpp align.cpp combine.cpp \
		tokenize.cpp tokenize_cleanup.cpp space.cpp newlines.cpp output.cpp \
		args.cpp options.cpp keywords.cpp punctuators.cpp parse_frame.cpp \
		logmask.cpp logger.cpp ChunkStack.cpp braces.cpp brace_cleanup.cpp \
		align_stack.cpp defines.cpp width.cpp lang_pawn.cpp md5.cpp \
		backup.cpp parens.cpp universalindentgui.cpp semicolons.cpp \
		sorting.cpp detect.cpp reflow_text.cpp reflow_text_dbg.cpp \
		reflow_text_utils.cpp reflow_text_box.cpp reflow_text_para.cpp

noinst_HEADERS = chunk_list.h options.h char_table.h chunk_list.h \
		uncrustify_types.h prototypes.h logmask.h logger.h \
		ChunkStack.h args.h token_names.h token_enum.h ListManager.h md5.h \
		align_stack.h backup.h base_types.h log_levels.h \
		punctuators.h reflow_text.h reflow_text_internal.h \
		uncrustify_version.h \
		unc_ctype.h

token_names.h: token_enum.h ../make_token_names.sh
	@echo "Rebuilding token_names.h"
	@sh ../make_token_names.sh

punctuators.h: ../scripts/punc.py punctuators.cpp
	@echo "Rebuilding punctuators.h"
	cd ../
	python scripts/punc.py > src/punctuators.h

# uncrustify_CPPFLAGS = -Wall

depend:
	ccmakedep -- $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(uncrustify_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -- $(uncrustify_SOURCES)



# DO NOT DELETE THIS LINE -- make depend depends on it.
