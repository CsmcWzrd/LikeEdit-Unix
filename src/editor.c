#define _POSIX_C_SOURCE 200809L
#include "editor.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void errmsg(char *msg, size_t cap, const char *action) {
    if (msg && cap) snprintf(msg, cap, "%s: %s", action, strerror(errno));
}
static char *strcopy(const char *str) {
    size_t n = strlen(str);
    char *r = (char *)malloc(n+1);
    if(r) memcpy(r,str,n+1);
    return r;
}
int le_utf8_decode(const char *str, size_t n, uint32_t *code, size_t *width) {
    const unsigned char *s = (const unsigned char *)str;
    uint32_t c; size_t k;
    if (!n) return 0;
    if (s[0] < 0x80) {c=s[0]; k=1;}
    else if(s[0]>=0xc2 && s[0]<=0xdf) {c=s[0]&31; k=2;}
    else if(s[0]>=0xe0 && s[0]<=0xef) {c=s[0]&15; k=3;}
    else if(s[0]>=0xf0 && s[0]<=0xf4) {c=s[0]&7; k=4;}
    else return 0;
    if(n<k) return 0;
    for(size_t i=1;i<k;i++) {
        if((s[i]&0xc0)!=0x80) return 0;
        c=(c<<6)|(s[i]&63);
    }
    if((k==2&&c<0x80)||(k==3&&c<0x800)||(k==4&&c<0x10000)||
       (c>=0xd800&&c<=0xdfff)||c>0x10ffff) return 0;
    if(code) *code=c;
    if(width) *width=k;
    return 1;
}
int le_utf8_validate(const char *s, size_t n) {
    size_t k=0, step=0;
    while(k<n) {
        if(!le_utf8_decode(s+k,n-k,NULL,&step)||s[k]=='\0') return 0;
        k+=step;
    }
    return 1;
}
size_t le_line_columns(const LE_Line *ln) {
    size_t i=0, columns=0, step;
    while(i<ln->bytes && le_utf8_decode(ln->text+i,ln->bytes-i,NULL,&step)) {i+=step;columns++;}
    return columns;
}
size_t le_line_byte_at(const LE_Line *ln, size_t column) {
    size_t i=0, step;
    while(i<ln->bytes && column && le_utf8_decode(ln->text+i,ln->bytes-i,NULL,&step)) {i+=step;column--;}
    return i;
}
static int line_init(LE_Line *ln, const char *text, size_t bytes) {
    ln->text=(char *)malloc(bytes+1);
    if(!ln->text) return -1;
    if(bytes) memcpy(ln->text,text,bytes);
    ln->text[bytes]=0;
    ln->bytes=bytes; ln->capacity=bytes+1;
    return 0;
}
static void line_free(LE_Line *ln) {free(ln->text); memset(ln,0,sizeof(*ln));}
static int line_reserve(LE_Line *ln, size_t need) {
    if(need<=ln->capacity) return 0;
    size_t c=ln->capacity?ln->capacity:16;
    while(c<need) {if(c>SIZE_MAX/2) return -1;c*=2;}
    char *p=(char *)realloc(ln->text,c);
    if(!p) return -1;
    ln->text=p;ln->capacity=c;
    return 0;
}
static int buf_reserve(LE_Buffer *b, size_t need) {
    if(need<=b->capacity) return 0;
    size_t c=b->capacity?b->capacity:8;
    while(c<need) {if(c>SIZE_MAX/2) return -1;c*=2;}
    if(c>SIZE_MAX/sizeof(LE_Line)) return -1;
    LE_Line *p=(LE_Line *)realloc(b->lines,c*sizeof(LE_Line));
    if(!p) return -1;
    b->lines=p;b->capacity=c;
    return 0;
}
static int buf_push(LE_Buffer *b, const char *s, size_t bytes) {
    if(buf_reserve(b,b->count+1)) return -1;
    if(line_init(&b->lines[b->count],s,bytes)) return -1;
    b->count++;
    return 0;
}
static LE_Buffer *buf_new(void) {
    LE_Buffer *b=(LE_Buffer *)calloc(1,sizeof(*b));
    if(!b) return NULL;
    if(buf_push(b,"",0)) {free(b);return NULL;}
    return b;
}
static void buf_free(LE_Buffer *b) {
    if(!b) return;
    for(size_t i=0;i<b->count;i++) line_free(&b->lines[i]);
    free(b->lines);free(b->path);free(b);
}
void le_editor_init(LE_Editor *ed) {memset(ed,0,sizeof(*ed));}
void le_editor_free(LE_Editor *ed) {
    for(size_t i=0;i<ed->count;i++) buf_free(ed->buffers[i]);
    free(ed->buffers);memset(ed,0,sizeof(*ed));
}
LE_Buffer *le_current(LE_Editor *ed) {return ed->count?ed->buffers[ed->active]:NULL;}
static int append_buffer(LE_Editor *ed, LE_Buffer *buf) {
    if(ed->count>=LIKEEDIT_MAX_BUFFERS) return -1;
    if(ed->count>=ed->capacity) {
        size_t c=ed->capacity?ed->capacity*2:8;
        LE_Buffer **p=(LE_Buffer **)realloc(ed->buffers,c*sizeof(*p));
        if(!p) return -1;
        ed->buffers=p;ed->capacity=c;
    }
    ed->active=ed->count;
    ed->buffers[ed->count++]=buf;
    return 0;
}
int le_editor_new(LE_Editor *ed) {
    LE_Buffer *b=buf_new();
    if(!b) return -1;
    if(append_buffer(ed,b)) {buf_free(b);return -1;}
    return 0;
}
int le_editor_switch(LE_Editor *ed,size_t index) {
    if(index>=ed->count) return -1;
    ed->active=index;return 0;
}
int le_editor_close(LE_Editor *ed,int force) {
    if(!ed->count||(!force&&le_current(ed)->dirty)) return -1;
    buf_free(ed->buffers[ed->active]);
    memmove(ed->buffers+ed->active,ed->buffers+ed->active+1,
        (ed->count-ed->active-1)*sizeof(*ed->buffers));
    ed->count--;
    if(ed->active>=ed->count && ed->count) ed->active=ed->count-1;
    if(ed->count==0) return le_editor_new(ed);
    return 0;
}
int le_editor_dirty(const LE_Editor *ed) {
    for(size_t i=0;i<ed->count;i++) if(ed->buffers[i]->dirty) return 1;
    return 0;
}
int le_editor_open(LE_Editor *ed,const char *path,char *error,size_t errlen) {
    if(!path||!*path) {if(error&&errlen) snprintf(error,errlen,"Empty path");return -1;}
    for(size_t i=0;i<ed->count;i++) {
        if(ed->buffers[i]->path && strcmp(ed->buffers[i]->path,path)==0) {ed->active=i;return 0;}
    }
    LE_Buffer *b=buf_new();
    if(!b) return -1;
    b->path=strcopy(path);
    if(!b->path) {buf_free(b);return -1;}
    FILE *fp=fopen(path,"rb");
    if(!fp) {
        if(errno!=ENOENT) {errmsg(error,errlen,"Open");buf_free(b);return -1;}
        if(append_buffer(ed,b)) {buf_free(b);return -1;}
        return 0;
    }
    struct stat st;
    if(fstat(fileno(fp),&st)||!S_ISREG(st.st_mode)||st.st_size<0||
       (uintmax_t)st.st_size>LIKEEDIT_MAX_FILE_SIZE) {
        if(error&&errlen) snprintf(error,errlen,"Unsupported file or file exceeds 128 MiB");
        fclose(fp);buf_free(b);return -1;
    }
    size_t size=(size_t)st.st_size;
    char *raw=(char *)malloc(size+1);
    if(!raw) {fclose(fp);buf_free(b);return -1;}
    size_t readn=fread(raw,1,size,fp);
    int ioerror=ferror(fp);
    fclose(fp);
    if(ioerror||readn!=size) {
        if(error&&errlen) snprintf(error,errlen,"Short or failed read");
        free(raw);buf_free(b);return -1;
    }
    size_t offset=0;
    if(size>=3&&(unsigned char)raw[0]==0xef&&(unsigned char)raw[1]==0xbb&&
       (unsigned char)raw[2]==0xbf) {b->bom=1;offset=3;}
    if(!le_utf8_validate(raw+offset,size-offset)) {
        if(error&&errlen) snprintf(error,errlen,"Invalid UTF-8 or NUL; refusing to edit");
        free(raw);buf_free(b);return -1;
    }
    size_t crlf=0,lf=0;
    for(size_t i=offset;i<size;i++) if(raw[i]=='\n') {
        if(i>offset&&raw[i-1]=='\r') crlf++;
        else lf++;
    }
    b->crlf=(crlf>lf);
    line_free(&b->lines[0]);b->count=0;
    size_t start=offset;
    for(size_t i=offset;i<size;i++) if(raw[i]=='\n') {
        size_t end=i;
        if(end>start&&raw[end-1]=='\r') end--;
        if(buf_push(b,raw+start,end-start)) goto badalloc;
        start=i+1;
    }
    if(buf_push(b,raw+start,size-start)) goto badalloc;
    free(raw);
    if(append_buffer(ed,b)) {buf_free(b);return -1;}
    return 0;
badalloc:
    if(error&&errlen) snprintf(error,errlen,"Out of memory");
    free(raw);buf_free(b);return -1;
}

int le_buffer_save(LE_Buffer *b,const char *path,char *error,size_t errlen) {
    const char *dest=path&&*path?path:b->path;
    if(!dest||!*dest) {if(error&&errlen) snprintf(error,errlen,"Filename required");return -1;}
    size_t n=strlen(dest);
    if(n>SIZE_MAX-30) return -1;
    char *tmp=(char *)malloc(n+25);
    if(!tmp) return -1;
    sprintf(tmp,"%s.likeedit-XXXXXX",dest);
    struct stat st;
    int existing=(stat(dest,&st)==0);
    int fd=mkstemp(tmp);
    if(fd<0) {errmsg(error,errlen,"Create temporary file");free(tmp);return -1;}
    if(existing) (void)fchmod(fd,st.st_mode&07777);
    FILE *f=fdopen(fd,"wb");
    if(!f) {errmsg(error,errlen,"Stream");close(fd);unlink(tmp);free(tmp);return -1;}
    int failed=0;
    if(b->bom && fwrite("\xef\xbb\xbf",1,3,f)!=3) failed=1;
    for(size_t i=0;i<b->count&&!failed;i++) {
        if(b->lines[i].bytes && fwrite(b->lines[i].text,1,b->lines[i].bytes,f)!=b->lines[i].bytes) failed=1;
        if(i+1<b->count) {
            const char *eol=b->crlf?"\r\n":"\n";
            size_t bytes=b->crlf?2:1;
            if(fwrite(eol,1,bytes,f)!=bytes) failed=1;
        }
    }
    if(fflush(f)) failed=1;
    if(fsync(fileno(f))) failed=1;
    if(fclose(f)) failed=1;
    if(!failed && rename(tmp,dest)) failed=1;
    if(failed) {
        errmsg(error,errlen,"Save");unlink(tmp);free(tmp);return -1;
    }
    free(tmp);
    if(!b->path || strcmp(dest,b->path)!=0) {
        char *fresh=strcopy(dest);
        if(fresh) {free(b->path);b->path=fresh;}
    }
    b->dirty=0;
    return 0;
}

void le_move(LE_Buffer *b,long x,long y) {
    if(!b||!b->count) return;
    if(y<0) b->cy=0;
    else if((unsigned long)y>=b->count) b->cy=b->count-1;
    else b->cy=(size_t)y;
    size_t max=le_line_columns(&b->lines[b->cy]);
    if(x<0) b->cx=0;
    else b->cx=(size_t)x>max?max:(size_t)x;
}
int le_insert(LE_Buffer *b,const char *text,size_t n) {
    if(!b||!n||!le_utf8_validate(text,n)) return -1;
    for(size_t i=0;i<n;i++) if(text[i]=='\n'||text[i]=='\r') return -1;
    LE_Line *ln=&b->lines[b->cy];
    size_t off=le_line_byte_at(ln,b->cx);
    if(n>SIZE_MAX-ln->bytes-1||line_reserve(ln,ln->bytes+n+1)) return -1;
    memmove(ln->text+off+n,ln->text+off,ln->bytes-off+1);
    memcpy(ln->text+off,text,n);
    ln->bytes+=n;
    LE_Line fragment={.text=(char *)text,.bytes=n};
    b->cx+=le_line_columns(&fragment);
    b->dirty=1;return 0;
}
int le_split(LE_Buffer *b) {
    if(!b||buf_reserve(b,b->count+1)) return -1;
    LE_Line *ln=&b->lines[b->cy];
    size_t off=le_line_byte_at(ln,b->cx);
    LE_Line next={0};
    if(line_init(&next,ln->text+off,ln->bytes-off)) return -1;
    memmove(b->lines+b->cy+2,b->lines+b->cy+1,
        (b->count-b->cy-1)*sizeof(LE_Line));
    b->lines[b->cy+1]=next;b->count++;
    ln=&b->lines[b->cy];ln->bytes=off;ln->text[off]=0;
    b->cy++;b->cx=0;b->dirty=1;return 0;
}
int le_backspace(LE_Buffer *b) {
    if(!b) return -1;
    LE_Line *ln=&b->lines[b->cy];
    if(b->cx>0) {
        size_t end=le_line_byte_at(ln,b->cx);
        size_t start=le_line_byte_at(ln,b->cx-1);
        memmove(ln->text+start,ln->text+end,ln->bytes-end+1);
        ln->bytes-=end-start;b->cx--;b->dirty=1;return 0;
    }
    if(b->cy==0) return 0;
    LE_Line *prev=&b->lines[b->cy-1];
    size_t prevcol=le_line_columns(prev);
    if(prev->bytes>SIZE_MAX-ln->bytes-1||line_reserve(prev,prev->bytes+ln->bytes+1)) return -1;
    memcpy(prev->text+prev->bytes,ln->text,ln->bytes+1);
    prev->bytes+=ln->bytes;
    line_free(ln);
    memmove(b->lines+b->cy,b->lines+b->cy+1,(b->count-b->cy-1)*sizeof(LE_Line));
    b->count--;b->cy--;b->cx=prevcol;b->dirty=1;return 0;
}
int le_delete(LE_Buffer *b) {
    if(!b) return -1;
    LE_Line *ln=&b->lines[b->cy];
    if(b->cx<le_line_columns(ln)) {
        size_t start=le_line_byte_at(ln,b->cx),end=le_line_byte_at(ln,b->cx+1);
        memmove(ln->text+start,ln->text+end,ln->bytes-end+1);
        ln->bytes-=end-start;b->dirty=1;return 0;
    }
    if(b->cy+1>=b->count) return 0;
    LE_Line *next=&b->lines[b->cy+1];
    if(ln->bytes>SIZE_MAX-next->bytes-1||line_reserve(ln,ln->bytes+next->bytes+1)) return -1;
    memcpy(ln->text+ln->bytes,next->text,next->bytes+1);
    ln->bytes+=next->bytes;
    line_free(next);
    memmove(b->lines+b->cy+1,b->lines+b->cy+2,(b->count-b->cy-2)*sizeof(LE_Line));
    b->count--;b->dirty=1;return 0;
}
int le_delete_line(LE_Buffer *b) {
    if(!b) return -1;
    if(b->count==1) {
        if(b->lines[0].bytes) {b->lines[0].text[0]=0;b->lines[0].bytes=0;b->dirty=1;}
        b->cx=0;return 0;
    }
    line_free(&b->lines[b->cy]);
    memmove(b->lines+b->cy,b->lines+b->cy+1,
        (b->count-b->cy-1)*sizeof(LE_Line));b->count--;
    if(b->cy>=b->count) b->cy=b->count-1;
    b->cx=0;b->dirty=1;return 0;
}
int le_open_line(LE_Buffer *b,int above) {
    if(!b||buf_reserve(b,b->count+1)) return -1;
    size_t at=b->cy+(above?0:1);
    LE_Line line={0};
    if(line_init(&line,"",0)) return -1;
    memmove(b->lines+at+1,b->lines+at,(b->count-at)*sizeof(LE_Line));
    b->lines[at]=line;b->count++;b->cy=at;b->cx=0;b->dirty=1;return 0;
}

/* Extension routing is kept separate from UI to support unit tests. */
const char *le_language(const char *path) {
    if(!path) return "plain";
    const char *dot=strrchr(path,'.');
    if(!dot) return "plain";
    if(!strcmp(dot,".c")||!strcmp(dot,".h")) return "c";
    if(!strcmp(dot,".cc")||!strcmp(dot,".cpp")||!strcmp(dot,".cxx")||!strcmp(dot,".hpp")||!strcmp(dot,".hh")) return "cpp";
    if(!strcmp(dot,".cs")) return "csharp";
    if(!strcmp(dot,".py")) return "python";
    if(!strcmp(dot,".js")||!strcmp(dot,".jsx")||!strcmp(dot,".ts")||!strcmp(dot,".tsx")) return "javascript";
    if(!strcmp(dot,".json")) return "json";
    if(!strcmp(dot,".sh")||!strcmp(dot,".bash")||!strcmp(dot,".zsh")) return "shell";
    if(!strcmp(dot,".html")||!strcmp(dot,".htm")||!strcmp(dot,".xml")) return "html";
    if(!strcmp(dot,".css")) return "css";
    if(!strcmp(dot,".rs")) return "rust";
    if(!strcmp(dot,".go")) return "go";
    if(!strcmp(dot,".java")) return "java";
    if(!strcmp(dot,".sql")) return "sql";
    return "plain";
}
