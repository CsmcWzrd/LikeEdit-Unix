#define _POSIX_C_SOURCE 200809L
#include "editor.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int checks=0;
#define CHECK(x) do {checks++; if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static void write_bytes(const char *p,const void *data,size_t n) {
    FILE *f=fopen(p,"wb"); CHECK(f!=NULL);
    CHECK(fwrite(data,1,n,f)==n);
    CHECK(fclose(f)==0);
}
static size_t read_bytes(const char *p,char *data,size_t cap) {
    FILE *f=fopen(p,"rb");CHECK(f!=NULL);
    size_t n=fread(data,1,cap,f);CHECK(!ferror(f));CHECK(fclose(f)==0);return n;
}
int main(void) {
    CHECK(le_utf8_validate("hello",5));
    CHECK(le_utf8_validate("\xe2\x82\xac",3));
    CHECK(le_utf8_validate("\xf0\x9f\x9a\x80",4));
    CHECK(!le_utf8_validate("\xc0\xaf",2));
    CHECK(!le_utf8_validate("\xf0\x90\x80",3));
    CHECK(!le_utf8_validate("\xed\xa0\x80",3));
    CHECK(!le_utf8_validate("a\0b",3));
    CHECK(!strcmp(le_language("a.cpp"),"cpp"));
    CHECK(!strcmp(le_language("b.py"),"python"));
    CHECK(!strcmp(le_language("c.json"),"json"));
    CHECK(!strcmp(le_language("file.noext"),"plain"));
    LE_Editor e;le_editor_init(&e);
    CHECK(le_editor_new(&e)==0);
    LE_Buffer *b=le_current(&e);
    CHECK(b && b->count==1);
    CHECK(le_insert(b,"a\xe2\x82\xac\xf0\x9f\x9a\x80",8)==0);
    CHECK(le_line_columns(&b->lines[0])==3);
    CHECK(b->cx==3);
    CHECK(le_line_byte_at(&b->lines[0],2)==4);
    CHECK(le_backspace(b)==0);
    CHECK(b->cx==2 && b->lines[0].bytes==4);
    CHECK(le_split(b)==0);
    CHECK(b->count==2 && b->cy==1 && b->cx==0);
    CHECK(le_insert(b,"tail",4)==0);
    le_move(b,2,0);
    CHECK(le_delete(b)==0);
    CHECK(b->count==1 && !strcmp(b->lines[0].text,"a\xe2\x82\xac" "tail"));
    CHECK(le_open_line(b,1)==0);
    CHECK(b->cy==0 && b->count==2);
    CHECK(le_delete_line(b)==0);
    CHECK(b->count==1);
    CHECK(le_insert(b,"!",1)==0);
    CHECK(le_editor_dirty(&e));
    CHECK(le_editor_new(&e)==0);
    CHECK(e.count==2);
    CHECK(le_editor_switch(&e,0)==0);
    CHECK(le_editor_close(&e,0)==-1);
    CHECK(le_editor_close(&e,1)==0);
    CHECK(e.count==1);
    le_editor_free(&e);

    char folder[]="/tmp/likeedit-core-XXXXXX";
    CHECK(mkdtemp(folder)!=NULL);
    char path[512],badpath[512],newpath[512],raw[256];
    snprintf(path,sizeof(path),"%s/sample.txt",folder);
    snprintf(badpath,sizeof(badpath),"%s/binary",folder);
    snprintf(newpath,sizeof(newpath),"%s/copy.txt",folder);
    const char data[]="\xef\xbb\xbf" "Hello\r\n" "World\r\n";
    write_bytes(path,data,sizeof(data)-1);
    LE_Editor files;le_editor_init(&files);
    char error[256]={0};
    CHECK(le_editor_open(&files,path,error,sizeof(error))==0);
    LE_Buffer *loaded=le_current(&files);
    CHECK(loaded->bom==1 && loaded->crlf==1);
    CHECK(loaded->count==3 && loaded->lines[2].bytes==0);
    CHECK(le_buffer_save(loaded,NULL,error,sizeof(error))==0);
    size_t n=read_bytes(path,raw,sizeof(raw));
    CHECK(n==sizeof(data)-1&&memcmp(raw,data,n)==0);
    CHECK(le_buffer_save(loaded,newpath,error,sizeof(error))==0);
    CHECK(!strcmp(loaded->path,newpath));
    CHECK(le_editor_open(&files,newpath,error,sizeof(error))==0);
    CHECK(files.count==1); /* switches to already-open buffer after Save As */
    CHECK(le_insert(loaded,"z",1)==0);
    CHECK(loaded->dirty);
    CHECK(le_buffer_save(loaded,NULL,error,sizeof(error))==0);
    CHECK(!loaded->dirty);
    const char nul[]={'a','\0','b'};
    write_bytes(badpath,nul,sizeof(nul));
    CHECK(le_editor_open(&files,badpath,error,sizeof(error))==-1);
    CHECK(files.count==1);
    le_editor_free(&files);
    CHECK(unlink(path)==0 && unlink(badpath)==0 && unlink(newpath)==0);
    CHECK(rmdir(folder)==0);

    const char *s="alpha // comment";int block=0,quote=0;size_t tok=0;
    CHECK(le_lex_class("c",s,strlen(s),0,&block,&quote,&tok)==0);
    CHECK(tok==5);
    CHECK(le_lex_class("c",s,strlen(s),6,&block,&quote,&tok)==1);
    CHECK(tok==strlen(s)-6);
    const char *k="return 123";
    CHECK(le_lex_class("c",k,strlen(k),0,&block,&quote,&tok)==3);
    CHECK(le_lex_class("c",k,strlen(k),7,&block,&quote,&tok)==4);
    fprintf(stdout,"LikeEditCoreTests passed %d checks\n",checks);
    return 0;
}
