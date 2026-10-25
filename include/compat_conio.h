#ifndef LIKEEDIT_COMPAT_CONIO_H
#define LIKEEDIT_COMPAT_CONIO_H
/* The subset of LikeConio 1.0 API needed by LikeEdit, for fallback builds. */
#include <wchar.h>
#define CONIO_OK 0
#define CONIO_ERROR (-1)
enum conio_color {
    CONIO_BLACK=0,CONIO_BLUE=1,CONIO_GREEN=2,CONIO_CYAN=3,
    CONIO_RED=4,CONIO_MAGENTA=5,CONIO_BROWN=6,CONIO_LIGHTGRAY=7,
    CONIO_DARKGRAY=8,CONIO_LIGHTBLUE=9,CONIO_LIGHTGREEN=10,
    CONIO_LIGHTCYAN=11,CONIO_LIGHTRED=12,CONIO_LIGHTMAGENTA=13,
    CONIO_YELLOW=14,CONIO_WHITE=15
};
enum conio_cursor {CONIO_CURSOR_HIDDEN=0,CONIO_CURSOR_NORMAL=1,CONIO_CURSOR_BLOCK=2};
enum conio_key {
    CONIO_KEY_BASE=0x110000, CONIO_KEY_UP,CONIO_KEY_DOWN,CONIO_KEY_LEFT,CONIO_KEY_RIGHT,
    CONIO_KEY_HOME,CONIO_KEY_END,CONIO_KEY_PAGEUP,CONIO_KEY_PAGEDOWN,
    CONIO_KEY_INSERT,CONIO_KEY_DELETE,CONIO_KEY_F1,CONIO_KEY_F2,CONIO_KEY_F3,CONIO_KEY_F4,
    CONIO_KEY_F5,CONIO_KEY_F6,CONIO_KEY_F7,CONIO_KEY_F8,CONIO_KEY_F9,CONIO_KEY_F10,
    CONIO_KEY_F11,CONIO_KEY_F12,CONIO_KEY_RESIZE,CONIO_KEY_BACKTAB,CONIO_KEY_UNKNOWN
};
int conio_init(void);
void conio_end(void);
int conio_refresh(void);
int conio_clrscr(void);
int conio_gotoxy(int x,int y);
int conio_screenwidth(void);
int conio_screenheight(void);
int conio_textcolor(int color);
int conio_textbackground(int color);
int conio_setcursor(int cursor);
int conio_putwch(wchar_t value);
int conio_getwch(void);
#endif
