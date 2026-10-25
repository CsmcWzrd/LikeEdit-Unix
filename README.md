# LikeEdit for Linux, Unix and macOS

**LikeEdit 1.1.0** is a C11, vi-inspired (not Vim) terminal editor: multiple open files, Unicode text, syntax highlighting, a bottom-half file selector, and command/edit modes. This is a Unix-family **reimplementation of the original Windows LikeEdit behavior**, aligned with the 2026-10-25 LikeEdit Windows README, not a source-level copy of the previous ZIP: the previous project's archive was discoverable but could not be extracted in this environment.

The Unix TUI uses the **LikeConio** API in `conio.h` from the Conio-over-ncurses project (public source: https://github.com/CsmcWzrd/LikeConio). When the original library is not installed, the default CMake build uses a small local ncursesw compatibility implementation with matching API calls **only for LikeEdit's needs**. It is not a replacement for the full LikeConio library. For a direct integration with the original library use `-DLIKEEDIT_REQUIRE_SYSTEM_CONIO=ON` after installing LikeConio.

## Requirements

- Linux, FreeBSD, NetBSD, OpenBSD, macOS, or another POSIX Unix
- C11 compiler; wide-character ncurses and its development headers; UTF-8 locale/terminal
- GNU Make **or** CMake >=3.16 **or** GNU Autotools; Python 3 for the PTY smoke test
- Interactive real TTY and `TERM` set (e.g. `xterm-256color`)

On Ubuntu: `sudo apt install build-essential libncurses-dev cmake pkg-config python3`

On Fedora: `sudo dnf install gcc make ncurses-devel cmake pkgconf-pkg-config python3`

On macOS with Homebrew: `brew install ncurses cmake pkg-config autoconf automake python3` and set `PKG_CONFIG_PATH="$(brew --prefix ncurses)/lib/pkgconfig:$PKG_CONFIG_PATH"` as needed.

On FreeBSD: `pkg install ncurses pkgconf cmake autoconf automake python3` (use `gmake` for the GNU Makefile).

## CMake (preferred)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/LikeEdit example.c second.py
sudo cmake --install build
```

To build directly against your existing **LikeConio source tree** without installing it:

```sh
cmake -S . -B build-with-likeconio -DLIKEEDIT_CONIO_SOURCE_DIR=../LikeConio
cmake --build build-with-likeconio --parallel
ctest --test-dir build-with-likeconio --output-on-failure
```

To require and use the *installed original* LikeConio, first install the LikeConio project, then:

```sh
cmake -S . -B build-original -DLIKEEDIT_REQUIRE_SYSTEM_CONIO=ON -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build-original --parallel
ctest --test-dir build-original --output-on-failure
```

## GNU Make

```sh
make
make check
./LikeEdit main.c main.h README.md
sudo make install PREFIX=/usr/local
```

Link against original LikeConio instead of the fallback with `make USE_SYSTEM_CONIO=1 SYSTEM_CONIO_CFLAGS='-I/path/to/likeconio/include' SYSTEM_CONIO_LIBS='-L/path/to/lib -lconio'`.

## GNU Autotools

```sh
autoreconf -fi
./configure --prefix=/usr/local
make
make check
sudo make install
```

Use `./configure --with-system-conio` after installing LikeConio. For non-default prefixes use environment variables `CPPFLAGS`/`LDFLAGS` and the runtime loader's search path.

## Controls

| Mode | Key | Action |
|---|---|---|
| COMMAND | `i`, `a`, `I`, `A` | Insert / append / line start / line end; enter EDIT |
| EDIT | Esc | Return to COMMAND |
| COMMAND | `Ctrl+T` (Ctrl+Shift+T when forwarded), `:buffers`, `:ls` | Open bottom-half file selector |
| Selector | Up/Down or `j`/`k`, Enter, Esc | Choose buffer and enter EDIT, or cancel |
| Either | Arrows, Home, End, PageUp, PageDown | Navigate |
| COMMAND | `h`, `j`, `k`, `l`, `0`, `$`, `gg`, `G` | Vi-style navigation |
| COMMAND | `x`, `dd`, `o`, `O` | Delete codepoint, delete line, open below/above |
| EDIT | Enter, Backspace, Delete, Tab | Edit text |
| Either | Ctrl+S / Ctrl+O / Ctrl+N / Ctrl+Q | Save / open / new / quit if clean |

Commands: `:w [path]`, `:q`, `:q!`, `:wq`, `:x`, `:e path`, `:new`, `:bn`, `:bp`, `:bd`, `:bd!`, `:buffers`, `:ls`, `:set number`, `:set nonumber`, `:help`.

`Ctrl+Shift+T` cannot be distinguished from `Ctrl+T` by many terminal protocols, and some terminal emulators reserve it to open a terminal tab. In COMMAND mode use `Ctrl+T` or `:buffers` as a portable fallback. The actual bottom-half picker is implemented.

## File behavior and caveats

- UTF-8 input and file content; saves UTF-8 BOM, predominant LF/CRLF, final newline, and per-buffer caret/scroll state.
- Rejects invalid UTF-8, NUL bytes, non-regular files, and files larger than 128 MiB.
- Saves through temporary file plus rename on the same filesystem (POSIX). Existing file mode is preserved; symlink paths are replaced by a regular file on save (do not edit via symlink paths).
- Color themes are intentionally simple; syntax color support covers C/C++/C#/Python/JS/TS/JSON/shell/HTML/CSS/Rust/Go/Java/SQL with lexical best-effort highlighting. UTF-8 display width depends on terminal, locale, and font.
- No undo/search/replace/Vim operator language or pixel-perfect grapheme clustering.
- Windows version's source ZIP was not retrievable as raw bytes for a line-by-line port. The portable editor model and Unix TUI here are fresh implementations using the documented interface.
- Linux GCC source tests, PTY interactive tests, CMake/Make/Autotools paths were exercised using a **test-only ncurses header declaration** with real Linux ncurses runtime libraries, because the build environment lacked installed ncurses development headers. Actual distribution builds must use the platform's official ncurses development package. macOS and BSD builds are provided but require native verification.

See [docs/PORTING.md](docs/PORTING.md) for implementation notes and portability testing.

### Xcode project generation (macOS)

```sh
cmake -S . -B build-xcode -G Xcode -DLIKEEDIT_CONIO_SOURCE_DIR=../LikeConio
cmake --build build-xcode --config Release
```

The Xcode target is still a terminal executable, not an AppKit windowed application.
