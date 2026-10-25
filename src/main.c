#define _XOPEN_SOURCE 700
#include "editor.h"
#ifdef LIKEEDIT_SYSTEM_CONIO
#include <conio.h>
#else
#include "compat_conio.h"
#endif
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <termios.h>
#include <unistd.h>

enum {MODE_COMMAND=0,MODE_EDIT=1};
enum {C_NORMAL=CONIO_LIGHTGRAY,C_COMMENT=CONIO_LIGHTGREEN,
      C_STRING=CONIO_YELLOW,C_KEYWORD=CONIO_LIGHTCYAN,
      C_NUMBER=CONIO_LIGHTMAGENTA,C_OP=CONIO_WHITE,C_DIRECTIVE=CONIO_LIGHTRED};
static const int syntax_colors[]={C_NORMAL,C_COMMENT,C_STRING,C_KEYWORD,C_NUMBER,C_OP,C_DIRECTIVE};

typedef struct {
    LE_Editor editor;
    int mode, picker, running, numbers;
    int pending_g,pending_d;
    size_t picker_index,picker_scroll;
    char message[256];
} LE_App;

static int uniwidth(uint32_t cp,int col) {
    if(cp=='\t') return 4-(col%4);
    if(cp<32) return 1;
    int width=wcwidth((wchar_t)cp);
    return width>=0?width:1;
}
static int width_before(const LE_Line *line,size_t column) {
    size_t byte=0,used; uint32_t cp;
    int width=0;
    while(byte<line->bytes && column && le_utf8_decode(line->text+byte,line->bytes-byte,&cp,&used)) {
        width+=uniwidth(cp,width);byte+=used;column--;
    }
    return width;
}
static int screen_gutter(const LE_App *app,int width) {
    if(!app->numbers) return 0;
    size_t n=app->editor.count?app->editor.buffers[app->editor.active]->count:1;
    int digits=1;
    while(n>=10) {n/=10;digits++;}
    int result=digits+2;
    return result>width/2?0:result;
}
static void cell(int x,int y,wchar_t ch,int fg,int bg) {
    if(x<=0||y<=0||x>conio_screenwidth()||y>conio_screenheight()) return;
    conio_textbackground(bg);conio_textcolor(fg);conio_gotoxy(x,y);
    if(ch<32&&ch!=' ') ch='?';
    (void)conio_putwch(ch);
}
static void print_at(int x,int y,const char *msg,int fg,int bg,int max) {
    if(!msg||max<=0) return;
    int pos=0;
    while(msg[pos]&&pos<max) {
        unsigned char c=(unsigned char)msg[pos];
        if(c<0x20||c>0x7e) c='?';
        cell(x+pos,y,c,fg,bg);
        pos++;
    }
}
static void clear_row(int y,int color) {
    int w=conio_screenwidth();
    conio_textbackground(color);conio_textcolor(CONIO_LIGHTGRAY);
    for(int i=1;i<=w;i++) cell(i,y,L' ',CONIO_LIGHTGRAY,color);
}
static int display_rows(void) {
    int h=conio_screenheight();return h>2?h-2:1;
}
static void ensure_visible(LE_App *app,int w,int rows) {
    LE_Buffer *b=le_current(&app->editor);
    if(!b) return;
    if(b->cy<b->sy) b->sy=b->cy;
    if(b->cy>=b->sy+(size_t)rows) b->sy=b->cy-(size_t)rows+1;
    int gutter=screen_gutter(app,w),avail=w-gutter;
    if(avail<1) avail=1;
    int cx=width_before(&b->lines[b->cy],b->cx);
    if((size_t)cx<b->sx) b->sx=(size_t)cx;
    if((size_t)cx>=b->sx+(size_t)avail) b->sx=(size_t)cx-(size_t)avail+1;
}
static void lex_skip_line(const char *lang,const LE_Line *line,int *comment) {
    int quote=0;
    for(size_t i=0;i<line->bytes;) {
        size_t n=1;
        (void)le_lex_class(lang,line->text,line->bytes,i,comment,&quote,&n);
        if(!n) n=1;
        i+=n;
    }
}
static void draw_textline(const LE_Line *line,const char *lang,int *block,
                          int x,int y,int avail,size_t offset) {
    int column=0,quoted=0;
    size_t i=0;
    while(i<line->bytes) {
        size_t token=1;
        int klass=le_lex_class(lang,line->text,line->bytes,i,block,&quoted,&token);
        int color=syntax_colors[(klass>=0&&klass<=6)?klass:0];
        size_t finish=i+token;
        if(finish>line->bytes) finish=line->bytes;
        while(i<finish) {
            size_t used=0;uint32_t cp=0;
            if(!le_utf8_decode(line->text+i,finish-i,&cp,&used)) {
                if(!le_utf8_decode(line->text+i,line->bytes-i,&cp,&used)) {i++;continue;}
            }
            if(cp=='\t') {
                int tab=uniwidth(cp,column);
                for(int j=0;j<tab;j++) {
                    if(column>=(int)offset && column-(int)offset<avail)
                        cell(x+column-(int)offset,y,L' ',color,CONIO_BLACK);
                    column++;
                }
            } else {
                int cw=uniwidth(cp,column);
                if(cw==0) {
                    /* Combining mark: draw on the prior cell where supported. */
                    if(column>(int)offset&&column-(int)offset<=avail)
                        cell(x+column-(int)offset-1,y,(wchar_t)cp,color,CONIO_BLACK);
                } else {
                    if(column>=(int)offset&&column-(int)offset+cw<=avail)
                        cell(x+column-(int)offset,y,(wchar_t)cp,color,CONIO_BLACK);
                    column+=cw;
                }
            }
            i+=used;
        }
    }
}
static void draw_picker(LE_App *app,int w,int rows) {
    int top=rows/2+1;
    if(top<1) top=1;
    int shown=rows-top;
    if(shown<1) return;
    for(int y=top;y<=rows;y++) clear_row(y,CONIO_BLUE);
    print_at(2,top,"OPEN FILES  (Up/Down, Enter to edit, Esc to cancel)",CONIO_WHITE,CONIO_BLUE,w-2);
    int listrows=shown;
    if((size_t)listrows>app->editor.count) listrows=(int)app->editor.count;
    if(app->picker_index<app->picker_scroll) app->picker_scroll=app->picker_index;
    if(app->picker_index>=app->picker_scroll+(size_t)listrows)
        app->picker_scroll=app->picker_index-(size_t)listrows+1;
    for(int row=0;row<listrows;row++) {
        size_t index=app->picker_scroll+(size_t)row;
        if(index>=app->editor.count) break;
        LE_Buffer *b=app->editor.buffers[index];
        int selected=index==app->picker_index;
        int bg=selected?CONIO_LIGHTGRAY:CONIO_BLUE;
        int fg=selected?CONIO_BLACK:CONIO_WHITE;
        int y=top+1+row;
        if(y>rows) break;
        for(int i=1;i<=w;i++) cell(i,y,' ',fg,bg);
        char label[512];
        snprintf(label,sizeof(label),"%c %3lu  %s%s",selected?'>':' ',
                 (unsigned long)(index+1),b->path?b->path:"[No Name]",b->dirty?" [+]":"");
        print_at(2,y,label,fg,bg,w-2);
    }
}
static void redraw(LE_App *app) {
    int w=conio_screenwidth(),h=conio_screenheight();
    if(w<=0||h<=0) return;
    int rows=display_rows();
    LE_Buffer *b=le_current(&app->editor);
    if(!b) return;
    ensure_visible(app,w,rows);
    conio_clrscr();
    int gutter=screen_gutter(app,w);
    const char *lang=le_language(b->path);
    int block=0;
    for(size_t i=0;i<b->sy&&i<b->count;i++) lex_skip_line(lang,&b->lines[i],&block);
    for(int row=0;row<rows;row++) {
        int y=row+1;size_t line=b->sy+(size_t)row;
        if(line>=b->count) {
            cell(1,y,L'~',CONIO_DARKGRAY,CONIO_BLACK);continue;
        }
        if(gutter) {
            char num[40];
            snprintf(num,sizeof(num),"%lu",(unsigned long)(line+1));
            int pad=gutter-(int)strlen(num)-1;
            for(int c=1;c<=pad;c++) cell(c,y,L' ',CONIO_DARKGRAY,CONIO_BLACK);
            print_at(pad+1,y,num,CONIO_DARKGRAY,CONIO_BLACK,gutter-pad-1);
            cell(gutter,y,L' ',CONIO_DARKGRAY,CONIO_BLACK);
        }
        draw_textline(&b->lines[line],lang,&block,gutter+1,y,w-gutter,b->sx);
    }
    if(app->picker) draw_picker(app,w,rows);
    if(h>=2) {
        clear_row(h-1,CONIO_BLUE);
        char status[512];
        snprintf(status,sizeof(status)," LikeEdit [%s] %s%s  %s  %lu:%lu  %lu/%lu ",
                app->mode==MODE_EDIT?"EDIT":"COMMAND",b->path?b->path:"[No Name]",
                b->dirty?" [+]":"",lang,(unsigned long)(b->cy+1),
                (unsigned long)(b->cx+1),(unsigned long)(app->editor.active+1),
                (unsigned long)app->editor.count);
        print_at(1,h-1,status,CONIO_WHITE,CONIO_BLUE,w);
    }
    clear_row(h,CONIO_BLACK);
    print_at(1,h,app->message[0]?app->message:
      " Ctrl+T: files  Ctrl+O: open  Ctrl+S: save  Ctrl+N: new  Ctrl+Q: quit  :help",
      CONIO_LIGHTGRAY,CONIO_BLACK,w);
    conio_setcursor(CONIO_CURSOR_NORMAL);
    if(!app->picker) {
        int x=gutter+1+width_before(&b->lines[b->cy],b->cx)-(int)b->sx;
        int y=1+(int)(b->cy-b->sy);
        if(x<1) x=1;
        if(x>w) x=w;
        if(y<1) y=1;
        if(y>rows) y=rows;
        conio_gotoxy(x,y);
    } else conio_setcursor(CONIO_CURSOR_HIDDEN);
    conio_refresh();
}
static void say(LE_App *app,const char *message) {
    snprintf(app->message,sizeof(app->message),"%s",message?message:"");
}
static int append_wchar(char *str,size_t cap,size_t *bytes,int key) {
    if(key<0||key>0x10ffff|| (key>=0xd800&&key<=0xdfff)) return -1;
    mbstate_t state={0}; char tmp[MB_LEN_MAX];
    size_t used=wcrtomb(tmp,(wchar_t)key,&state);
    if(used==(size_t)-1||*bytes+used>=cap) return -1;
    memcpy(str+*bytes,tmp,used);*bytes+=used;str[*bytes]=0;return 0;
}
static void pop_utf8(char *s,size_t *n) {
    if(!*n) return;
    size_t k=*n-1;
    while(k>0&&(((unsigned char)s[k]&0xc0)==0x80)) k--;
    s[k]=0;*n=k;
}
static int prompt(LE_App *app,const char *name,char *out,size_t cap) {
    (void)app;
    size_t n=0;out[0]=0;
    int h=conio_screenheight(),w=conio_screenwidth();
    for(;;) {
        clear_row(h,CONIO_BLUE);
        print_at(1,h,name,CONIO_WHITE,CONIO_BLUE,w);
        int left=(int)strlen(name)+1;
        print_at(left,h,out,CONIO_YELLOW,CONIO_BLUE,w-left+1);
        int x=left+(int)le_line_columns(&(LE_Line){.text=out,.bytes=n});
        if(x>w)x=w;
        conio_setcursor(CONIO_CURSOR_NORMAL);
        conio_gotoxy(x,h);conio_refresh();
        int key=conio_getwch();
        if(key==27) return 0;
        if(key==10||key==13) return 1;
        if(key==8||key==127) {pop_utf8(out,&n);continue;}
        if(key>=32 && key<CONIO_KEY_BASE) (void)append_wchar(out,cap,&n,key);
    }
}
static int save_current(LE_App *app,const char *explicit_path) {
    LE_Buffer *b=le_current(&app->editor);
    char path[4096],err[256]={0};
    if(!explicit_path && !b->path) {
        if(!prompt(app,"Save as: ",path,sizeof(path))) {say(app,"Save cancelled");return 0;}
        explicit_path=path;
    }
    if(le_buffer_save(b,explicit_path,err,sizeof(err))) {
        say(app,err[0]?err:"Save failed");
        return 0;
    }
    say(app,"File saved");
    return 1;
}
static void open_path(LE_App *app,const char *path) {
    char err[256]={0};
    if(!*path) {say(app,"Filename required");return;}
    if(le_editor_open(&app->editor,path,err,sizeof(err))) say(app,err[0]?err:"Open failed");
    else {say(app,"");app->mode=MODE_EDIT;}
}
static void open_prompt(LE_App *app) {
    char name[4096];
    if(prompt(app,"Open file: ",name,sizeof(name))) open_path(app,name);
    else say(app,"Open cancelled");
}
static void picker_open(LE_App *app) {
    app->picker=1;app->picker_index=app->editor.active;app->picker_scroll=0;
    say(app,"Choose a file; Enter switches to it in EDIT mode");
}
static void ex_command(LE_App *app,const char *command) {
    char err[256]={0};
    while(*command==' ')command++;
    if(!strcmp(command,"q")) {
        if(le_editor_dirty(&app->editor)) say(app,"Unsaved changes; use :q! to discard");
        else app->running=0;
    } else if(!strcmp(command,"q!")) app->running=0;
    else if(!strcmp(command,"wq")||!strcmp(command,"x")) {
        if(save_current(app,NULL)) {
            if(!le_editor_dirty(&app->editor)) app->running=0;
            else if(!le_current(&app->editor)->dirty) say(app,"Other buffers have unsaved changes");
        }
    } else if(!strcmp(command,"w")) save_current(app,NULL);
    else if(!strncmp(command,"w ",2)) save_current(app,command+2);
    else if(!strncmp(command,"e ",2)) open_path(app,command+2);
    else if(!strcmp(command,"new")) {
        if(le_editor_new(&app->editor)) say(app,"Cannot create buffer");
        else {app->mode=MODE_EDIT;say(app,"");}
    } else if(!strcmp(command,"bn")||!strcmp(command,"bp")) {
        size_t n=app->editor.count,cur=app->editor.active;
        le_editor_switch(&app->editor,!strcmp(command,"bn")?(cur+1)%n:(cur+n-1)%n);
        say(app,"");
    } else if(!strcmp(command,"bd")||!strcmp(command,"bd!")) {
        if(le_editor_close(&app->editor,!strcmp(command,"bd!")))
            say(app,"Buffer modified; use :bd! to discard");
        else say(app,"Buffer closed");
    } else if(!strcmp(command,"buffers")||!strcmp(command,"ls")) picker_open(app);
    else if(!strcmp(command,"set number")) {app->numbers=1;say(app,"Line numbers enabled");}
    else if(!strcmp(command,"set nonumber")) {app->numbers=0;say(app,"Line numbers disabled");}
    else if(!strcmp(command,"help"))
        say(app,"i/a/I/A edit  h/j/k/l move  x/dd delete  o/O newline  :w :q :q! :buffers");
    else {snprintf(err,sizeof(err),"Unknown command: %.220s",command);say(app,err);}
}
static void handle_nav(LE_App *app,int k) {
    LE_Buffer *b=le_current(&app->editor);
    long x=(long)b->cx,y=(long)b->cy;
    switch(k) {
        case CONIO_KEY_LEFT:x--;break;
        case CONIO_KEY_RIGHT:x++;break;
        case CONIO_KEY_UP:y--;break;
        case CONIO_KEY_DOWN:y++;break;
        case CONIO_KEY_HOME:x=0;break;
        case CONIO_KEY_END:x=(long)le_line_columns(&b->lines[b->cy]);break;
        case CONIO_KEY_PAGEUP:y-=display_rows();break;
        case CONIO_KEY_PAGEDOWN:y+=display_rows();break;
        default:return;
    }
    le_move(b,x,y);
}
static void input_command(LE_App *app,int k) {
    LE_Buffer *b=le_current(&app->editor);
    if(app->pending_g) {
        app->pending_g=0;
        if(k=='g') {le_move(b,(long)b->cx,0);return;}
    }
    if(app->pending_d) {
        app->pending_d=0;
        if(k=='d') {le_delete_line(b);return;}
    }
    switch(k) {
        case 'i':app->mode=MODE_EDIT;return;
        case 'a':le_move(b,(long)b->cx+1,(long)b->cy);app->mode=MODE_EDIT;return;
        case 'I':le_move(b,0,(long)b->cy);app->mode=MODE_EDIT;return;
        case 'A':le_move(b,(long)le_line_columns(&b->lines[b->cy]),(long)b->cy);app->mode=MODE_EDIT;return;
        case 'h':le_move(b,(long)b->cx-1,(long)b->cy);return;
        case 'j':le_move(b,(long)b->cx,(long)b->cy+1);return;
        case 'k':le_move(b,(long)b->cx,(long)b->cy-1);return;
        case 'l':le_move(b,(long)b->cx+1,(long)b->cy);return;
        case '0':le_move(b,0,(long)b->cy);return;
        case '$':le_move(b,(long)le_line_columns(&b->lines[b->cy]),(long)b->cy);return;
        case 'G':le_move(b,(long)b->cx,(long)b->count-1);return;
        case 'g':app->pending_g=1;return;
        case 'd':app->pending_d=1;return;
        case 'x':(void)le_delete(b);return;
        case 'o':(void)le_open_line(b,0);app->mode=MODE_EDIT;return;
        case 'O':(void)le_open_line(b,1);app->mode=MODE_EDIT;return;
        case ':': {
            char cmd[4096];
            if(prompt(app,":",cmd,sizeof(cmd))) ex_command(app,cmd);
            return;
        }
        default:break;
    }
    handle_nav(app,k);
}
static void input_edit(LE_App *app,int k) {
    LE_Buffer *b=le_current(&app->editor);
    if(k==27) {app->mode=MODE_COMMAND;return;}
    if(k==8||k==127) {(void)le_backspace(b);return;}
    if(k==CONIO_KEY_DELETE) {(void)le_delete(b);return;}
    if(k==10||k==13) {(void)le_split(b);return;}
    if(k=='\t') {(void)le_insert(b,"\t",1);return;}
    if(k>=32&&k<CONIO_KEY_BASE) {
        char out[MB_LEN_MAX+1]={0};size_t n=0;
        if(append_wchar(out,sizeof(out),&n,k)==0) (void)le_insert(b,out,n);
        return;
    }
    handle_nav(app,k);
}
static void handle_picker(LE_App *app,int k) {
    if(k==27) {app->picker=0;app->mode=MODE_COMMAND;say(app,"");return;}
    if(k==CONIO_KEY_UP||k=='k') {
        if(app->picker_index)app->picker_index--;
    } else if(k==CONIO_KEY_DOWN||k=='j') {
        if(app->picker_index+1<app->editor.count)app->picker_index++;
    } else if(k=='\n'||k=='\r') {
        le_editor_switch(&app->editor,app->picker_index);
        app->picker=0;app->mode=MODE_EDIT;say(app,"");
    }
}
static void on_key(LE_App *app,int key) {
    if(app->picker) {handle_picker(app,key);return;}
    if(key==CONIO_KEY_RESIZE||key<0) return;
    if(key==20 && app->mode==MODE_COMMAND) {picker_open(app);return;}
    if(key==19) {save_current(app,NULL);return;}
    if(key==15) {open_prompt(app);return;}
    if(key==14) {
        if(le_editor_new(&app->editor)) say(app,"Cannot open new buffer");
        else {app->mode=MODE_EDIT;say(app,"");}
        return;
    }
    if(key==17) {
        if(le_editor_dirty(&app->editor)) say(app,"Unsaved buffers; use :q! in COMMAND mode to discard");
        else app->running=0;
        return;
    }
    if(app->mode==MODE_EDIT) input_edit(app,key);
    else input_command(app,key);
}
static void enable_editor_shortcuts(void) {
    /* POSIX IXON intercepts Ctrl+S as XOFF and freezes ncurses output. */
    struct termios t;
    if(tcgetattr(STDIN_FILENO,&t)==0) {
        t.c_iflag &= (tcflag_t)~(IXON|IXOFF);
        (void)tcsetattr(STDIN_FILENO,TCSANOW,&t);
    }
}
static void usage(const char *exe) {
    printf("LikeEdit - vi-inspired, multi-file, UTF-8 console editor (Unix)\n"
           "Usage: %s [--help] [file ...]\n"
           "COMMAND: i/a/I/A, h/j/k/l, gg/G, 0/$, x/dd, o/O, :w, :q, :q!, :buffers\n"
           "EDIT: typing, Enter, Backspace, Delete; Esc for COMMAND\n"
           "Both: Ctrl+S save, Ctrl+O open, Ctrl+N new, Ctrl+Q quit\n"
           "COMMAND: Ctrl+T (Ctrl+Shift+T when terminal forwards it) file selector\n",exe);
}
int main(int argc,char **argv) {
    LE_App app={0};le_editor_init(&app.editor);
    if(argc>1&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))) {usage(argv[0]);return 0;}
    app.running=1;app.numbers=1;
    int failures=0;
    for(int i=1;i<argc;i++) {
        char err[256]={0};
        if(le_editor_open(&app.editor,argv[i],err,sizeof(err))) {
            fprintf(stderr,"LikeEdit: %s: %s\n",argv[i],err[0]?err:"cannot open");
            failures++;
        }
    }
    if(!app.editor.count&&le_editor_new(&app.editor)) {fprintf(stderr,"Out of memory\n");return 1;}
    if(conio_init()!=CONIO_OK) {
        fprintf(stderr,"LikeEdit requires a TTY with a working TERM and ncurses-compatible terminal.\n");
        le_editor_free(&app.editor);return 1;
    }
    setlocale(LC_CTYPE,"");
    enable_editor_shortcuts();
    while(app.running) {
        redraw(&app);
        int key=conio_getwch();
        on_key(&app,key);
    }
    conio_end();le_editor_free(&app.editor);
    return failures?2:0;
}
