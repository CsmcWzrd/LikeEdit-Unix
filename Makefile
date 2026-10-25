# GNU make (on BSD/macOS use gmake or the CMake/Autotools build)
CC ?= cc
PREFIX ?= /usr/local
DESTDIR ?=
CFLAGS ?= -O2 -g
CPPFLAGS ?=
LDFLAGS ?=
# 1 links the original LikeConio from the sibling project, 0 uses ncurses fallback.
USE_SYSTEM_CONIO ?= 0
PKG_CONFIG ?= pkg-config
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null)
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null)
ifeq ($(strip $(NCURSES_LIBS)),)
NCURSES_LIBS = -lncursesw
endif
SYSTEM_CONIO_CFLAGS ?=
SYSTEM_CONIO_LIBS ?= -lconio
CORE = src/editor.o src/syntax.o
EDITOR = src/main.o
ifeq ($(USE_SYSTEM_CONIO),1)
CPPFLAGS += -DLIKEEDIT_SYSTEM_CONIO=1 $(SYSTEM_CONIO_CFLAGS)
CONIO_OBJECT =
CONIO_LIBS = $(SYSTEM_CONIO_LIBS)
else
CPPFLAGS += $(NCURSES_CFLAGS)
CONIO_OBJECT = src/compat_conio.o
CONIO_LIBS = $(NCURSES_LIBS)
endif
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean check test install uninstall
all: LikeEdit
LikeEdit: $(CORE) $(EDITOR) $(CONIO_OBJECT)
	$(CC) $(LDFLAGS) -o $@ $^ $(CONIO_LIBS)
LikeEditCoreTests: $(CORE) tests/test_core.o
	$(CC) $(LDFLAGS) -o $@ $^
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
check test: LikeEdit LikeEditCoreTests
	./LikeEditCoreTests
	python3 tests/pty_smoke.py ./LikeEdit
install: LikeEdit
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 LikeEdit $(DESTDIR)$(PREFIX)/bin/LikeEdit
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/LikeEdit
clean:
	rm -f src/*.o tests/*.o LikeEdit LikeEditCoreTests
