#define _XOPEN_SOURCE_EXTENDED 1
#include "compat_conio.h"
#include <locale.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>

/* Fallback backend. The original LikeConio library is preferred if installed. */
static int fg=7,bg=0,started=0;
static short nc_color(int dos) {
    static const short map[8]={COLOR_BLACK,COLOR_BLUE,COLOR_GREEN,COLOR_CYAN,
                                  COLOR_RED,COLOR_MAGENTA,COLOR_YELLOW,COLOR_WHITE};
    return map[dos&7];
}
static void apply(void) {
    if(!started) return;
    if(has_colors() && COLOR_PAIRS>1) {
        int pair=((fg&7)*8+(bg&7))%(COLOR_PAIRS-1)+1;
        init_pair((short)pair,nc_color(fg),nc_color(bg));
        attrset(COLOR_PAIR(pair)|(fg>7?A_BOLD:A_NORMAL));
    } else attrset(fg>7?A_BOLD:A_NORMAL);
}
int conio_init(void) {
    if(!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)) return CONIO_ERROR;
    setlocale(LC_CTYPE,"");
    if(!initscr()) return CONIO_ERROR;
    started=1;
    cbreak();noecho();keypad(stdscr,TRUE);
    if(has_colors()) start_color();
    apply();
    return CONIO_OK;
}
void conio_end(void) {if(started){endwin();started=0;}}
int conio_refresh(void) {return refresh()==ERR?-1:0;}
int conio_clrscr(void) {return erase()==ERR?-1:0;}
int conio_gotoxy(int x,int y) {return move(y-1,x-1)==ERR?-1:0;}
int conio_screenwidth(void) {return COLS;}
int conio_screenheight(void) {return LINES;}
int conio_textcolor(int color) {fg=color;apply();return 0;}
int conio_textbackground(int color) {bg=color;apply();return 0;}
int conio_setcursor(int cursor) {int val=cursor==0?0:(cursor==2?2:1);return curs_set(val)==ERR?-1:0;}
int conio_putwch(wchar_t value) {
    wchar_t one[2]={value,0};
    return addnwstr(one,1)==ERR?-1:0;
}
int conio_getwch(void) {
    wint_t result=0;
    int rc=get_wch(&result);
    if(rc==ERR) return -1;
    if(rc==OK) return (int)result;
    switch(result) {
        case KEY_UP:return CONIO_KEY_UP;
        case KEY_DOWN:return CONIO_KEY_DOWN;
        case KEY_LEFT:return CONIO_KEY_LEFT;
        case KEY_RIGHT:return CONIO_KEY_RIGHT;
        case KEY_HOME:return CONIO_KEY_HOME;
        case KEY_END:return CONIO_KEY_END;
        case KEY_PPAGE:return CONIO_KEY_PAGEUP;
        case KEY_NPAGE:return CONIO_KEY_PAGEDOWN;
        case KEY_IC:return CONIO_KEY_INSERT;
        case KEY_DC:return CONIO_KEY_DELETE;
        case KEY_F(1):return CONIO_KEY_F1;
        case KEY_F(2):return CONIO_KEY_F2;
        case KEY_F(3):return CONIO_KEY_F3;
        case KEY_F(4):return CONIO_KEY_F4;
        case KEY_F(5):return CONIO_KEY_F5;
        case KEY_F(6):return CONIO_KEY_F6;
        case KEY_F(7):return CONIO_KEY_F7;
        case KEY_F(8):return CONIO_KEY_F8;
        case KEY_F(9):return CONIO_KEY_F9;
        case KEY_F(10):return CONIO_KEY_F10;
        case KEY_F(11):return CONIO_KEY_F11;
        case KEY_F(12):return CONIO_KEY_F12;
        case KEY_RESIZE:return CONIO_KEY_RESIZE;
        case KEY_BTAB:return CONIO_KEY_BACKTAB;
        case KEY_BACKSPACE:return 127;
        case KEY_ENTER:return '\n';
        default:return CONIO_KEY_UNKNOWN;
    }
}
