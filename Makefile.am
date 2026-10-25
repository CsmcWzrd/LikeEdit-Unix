AM_CPPFLAGS = -I$(srcdir)/include
AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
bin_PROGRAMS = LikeEdit
LikeEdit_SOURCES = src/main.c src/editor.c src/syntax.c
LikeEdit_LDADD = $(CONIO_LIBS)
if SYSTEM_CONIO
AM_CPPFLAGS += -DLIKEEDIT_SYSTEM_CONIO=1
else
LikeEdit_SOURCES += src/compat_conio.c
endif
check_PROGRAMS = LikeEditCoreTests
LikeEditCoreTests_SOURCES = tests/test_core.c src/editor.c src/syntax.c
TESTS = LikeEditCoreTests
EXTRA_DIST = README.md docs/PORTING.md tests/pty_smoke.py CMakeLists.txt Makefile .github/workflows/build.yml include/editor.h include/compat_conio.h
