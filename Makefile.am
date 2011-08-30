## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS=foreign no-dependencies 1.11

SUBDIRS = src man

pkgdata_DATA = etc/ben.cfg etc/d.cfg etc/defaults.cfg etc/gnu-indent.cfg etc/kr-indent.cfg etc/linux-indent.cfg etc/linux.cfg etc/mono.cfg etc/xsupplicant.cfg

# INCLUDES = @INCLUDES@

testdir = tests
# test_SCRIPTS = tests/run_tests.py
TESTS = run_tests.sh

EXTRA_DIST = etc m4 scripts run_tests.sh make_token_names.sh $(pkgdata_DATA) \
		tests/*.test \
		tests/*.sh \
		tests/*.py \
		tests/config \
		tests/input/ \
		tests/output/ \
		win32/Uncrustify_Win32_2003.sln \
		win32/Uncrustify_Win32_2003.vcproj \
		win32/Uncrustify_Win32_2005.sln \
		win32/Uncrustify_Win32_2005.vcproj \
		win32/Uncrustify_Win32_2008.sln \
		win32/Uncrustify_Win32_2008.vcproj \
		win32/Uncrustify_Win32_2010.sln \
		win32/Uncrustify_Win32_2010.vcxproj \
		win32/Uncrustify_Win32_2010.vcxproj.filters \
		win32/Uncrustify_Win32.sln \
		win32/Uncrustify_Win32.vcproj \
		win32/Uncrustify_Win32.dsw \
		win32/Uncrustify_Win32.dsp \
		win32/windows_compat.h \
		uncrustify.xcodeproj osx make_win32.sh documentation \
		ChangeLog \
		BUGS AUTHORS aclocal.m4 autogen.sh COPYING do-c.sh do-d.sh extras.vpj HELP INSTALL \
		install-sh make_token_names.sh mk_all_distros.sh \
		NEWS README release-steps.txt todo.txt uncrustify.vpj uncrustify.vpw \
		update-default-cfg.sh


check-local: 


depend:
	(cd src && $(MAKE) $(AM_MAKEFLAGS) depend);


DISTCLEANFILES =
CLEANFILES = *~ *.bak
MAINTAINERCLEANFILES = aclocal.m4 Makefile.in


.PHONY: test_all


# DO NOT DELETE THIS LINE -- make depend depends on it.

