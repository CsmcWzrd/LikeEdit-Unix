# LikeEdit Unix porting notes

## Architecture

- `include/editor.h`, `src/editor.c`: standalone C11 portable editor buffers and file I/O. Stores one UTF-8 line per allocation, one Unicode scalar index per cursor column, with CRLF/BOM preservation and atomic file writes.
- `src/syntax.c`: extension and token-based highlighting, including C-style block comments and line comments.
- `src/main.c`: TUI and input dispatch; implements the bottom-half buffer picker and vi-inspired command/edit modes. Uses only the `conio_*` API from CsmcWzrd/LikeConio.
- `src/compat_conio.c`, `include/compat_conio.h`: minimal ncursesw backend for standalone builds, not the original full conio library.
- Build with `LIKEEDIT_CONIO_SOURCE_DIR=../LikeConio` to compile the original project as a sibling CMake dependency, or `LIKEEDIT_REQUIRE_SYSTEM_CONIO=ON` / `--with-system-conio` to force an installed LikeConio.

## Cross-platform status

GNU/Linux with gcc + ncursesw: building and automated tests are part of this distribution. macOS, FreeBSD, OpenBSD, NetBSD, and other Unix targets need their own native CI/interactive checks; GNU and POSIX vary in ncurses discovery and terminal escape sequences.

## Terminal keyboard limits

Ctrl+Shift+T typically encodes as Ctrl+T (0x14) in conventional terminal protocols, and macOS Terminal/iTerm2/Windows Terminal may reserve keyboard shortcuts. A terminal-side shortcut must be configured to pass the key to the application or `Ctrl+T` / `:buffers` should be used. Unicode and combining characters are passed by `conio_getwch`, if the terminal and locale support wide characters.

## Limitations and safety

The original LikeEdit source file could not be downloaded from the ChatGPT Project Library in this runtime; this implementation follows the documented feature set but was not a line-by-line source merge. For a formal port, diff this new editor core and TUI against the original LikeEdit ZIP when its source is available. Files larger than 128 MiB and invalid UTF-8 are rejected intentionally. The ncurses backend is not thread-safe; no background threads are created. Ctrl+Q refuses to quit with dirty buffers. Symlinks are not preserved by atomic replacement.

## Suggested platform verification

1. Set terminal to UTF-8 and `TERM=xterm-256color`.
2. Run `ctest --test-dir build --output-on-failure` or `make check`.
3. Open multiple files, hit `Esc` then `Ctrl+T`, choose a different buffer, press Enter and verify EDIT mode.
4. Use `:w`, `:q`, `:q!`, `:bd!`, Ctrl+O/N/S/Q; edit C, Python, and JSON.
5. Test newline styles, trailing newline, UTF-8 BMP and astral glyphs, terminal resizing and save permissions.
