# LikeEdit Unix 1.1.0 build verification

Performed in the generated artifact environment (Linux x86_64, GCC 14.2, Clang). No macOS/BSD native machine was available for execution.

| Check | Result |
|---|---|
| Portable editor C11 core: GCC ASan+UBSan | PASS, 69 assertions |
| Portable editor C11 core: Clang ASan+UBSan | PASS, 69 assertions |
| GCC + ncursesw UI with `-Wall -Wextra -Wpedantic -Werror` | PASS |
| Clang + ncursesw UI with `-Wall -Wextra -Wpedantic -Werror` | PASS |
| PTY terminal: bottom-half picker, mode switch, Ctrl+S save, Esc and `:q` | PASS |
| CMake fallback build + ctest | PASS, 2 tests |
| GNU Make fallback build + make check | PASS, 69 core assertions and PTY |
| Autotools autoreconf, configure, make, make check | PASS, 1 core test |
| CMake direct sibling `conio::conio` target branch + ctest | PASS, 2 tests using test conio stand-in |
| CMake manually located static libconio.a branch + ctest | PASS, 2 tests using test conio stand-in |
| Original downloaded Windows LikeEdit core versus reimplementation diff | NOT PERFORMED, Project archive inaccessible as raw bytes |
| Genuine LikeConio library source build and real API regression | NOT PERFORMED, Project archive inaccessible as raw bytes, GitHub CMake API inspected |
| macOS / BSD native compilation and terminal execution | NOT PERFORMED |

**Environment caveat:** The environment had the Linux ncursesw shared runtime binaries but not the ncurses development header package. For the *local verification only*, a temporary minimal ncurses declaration header was created **outside** the release directory and linked to real Linux ncursesw/libtinfo runtime libraries. This is not a substitute for installed development headers on a consumer host. There is no generated test stub or binary in the source release.

**Source provenance:** The LikeEdit Windows README was retrieved from the Console based editor Project Library. The LikeEdit source ZIP and conio-ncurses source ZIP were visible but returned `This Project file does not have an authorized raw-byte materialization path` when materialization was requested. The original LikeConio public repository `https://github.com/CsmcWzrd/LikeConio` was used to verify the `conio.h` function signatures and CMake `conio::conio` target. No unverified statement of a line-by-line port is made.
