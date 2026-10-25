#include "editor.h"
#include <ctype.h>
#include <string.h>

static int isid(int c) {return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';}
static int keyword(const char *s,size_t n) {
    static const char *words[]={"auto","break","case","catch","char","class","const","continue","def","delete",
        "do","double","else","enum","except","export","extern","false","finally","float","for","foreach",
        "from","func","function","goto","if","import","in","int","interface","let","long","namespace",
        "new","null","nullptr","package","pass","private","protected","public","raise","return","short",
        "signed","sizeof","static","struct","switch","template","throw","true","try","typedef","type",
        "unsigned","using","var","virtual","void","volatile","while","yield","async","await","fn","mut",
        "pub","impl","self","select","where","insert","update","delete","create","table","join","union",
        "SELECT","FROM","WHERE","INSERT","UPDATE","DELETE","CREATE","TABLE","JOIN","AND","OR","NOT"};
    for(size_t i=0;i<sizeof(words)/sizeof(words[0]);i++) if(strlen(words[i])==n&&!memcmp(s,words[i],n)) return 1;
    return 0;
}
int le_lex_class(const char *lang,const char *s,size_t len,size_t at,
                 int *block,int *quoted,size_t *bytes) {
    if(!s||at>=len||!block||!quoted||!bytes) return 0;
    size_t rest=len-at;
    *bytes=1;
    int slash=!strcmp(lang,"c")||!strcmp(lang,"cpp")||!strcmp(lang,"csharp")||
         !strcmp(lang,"java")||!strcmp(lang,"javascript")||!strcmp(lang,"go")||
         !strcmp(lang,"rust")||!strcmp(lang,"css");
    int hash=!strcmp(lang,"python")||!strcmp(lang,"shell");
    int html=!strcmp(lang,"html");
    int sql=!strcmp(lang,"sql");
    if(!strcmp(lang,"plain")) return 0;
    if(*block) {
        size_t i=at;
        const char *stop=html?"-->":"*/";
        size_t sn=html?3:2;
        while(i<len) {
            if(len-i>=sn&&!memcmp(s+i,stop,sn)) {i+=sn;*block=0;break;}
            i++;
        }
        *bytes=i-at;return 1;
    }
    if(*quoted) {
        size_t i=at;
        for(;i<len;i++) {
            if(s[i]=='\\'&&i+1<len) {i++;continue;}
            if(s[i]==*quoted) {i++;*quoted=0;break;}
        }
        *bytes=i-at;return 2;
    }
    if((slash&&rest>=2&&!memcmp(s+at,"/*",2))||(html&&rest>=4&&!memcmp(s+at,"<!--",4))) {
        *block=1;return le_lex_class(lang,s,len,at,block,quoted,bytes);
    }
    if((slash&&rest>=2&&!memcmp(s+at,"//",2))||(hash&&s[at]=='#')||
       (sql&&rest>=2&&!memcmp(s+at,"--",2))) { *bytes=rest;return 1; }
    if(s[at]=='\''||s[at]=='"'||(s[at]=='`'&&!strcmp(lang,"javascript"))) {
        int quote=s[at];size_t i=at+1;
        for(;i<len;i++) {
            if(s[i]=='\\'&&i+1<len) {i++;continue;}
            if(s[i]==quote) {i++;break;}
        }
        *bytes=i-at;return 2;
    }
    if(isdigit((unsigned char)s[at])) {
        size_t i=at+1;
        while(i<len&&(isalnum((unsigned char)s[i])||s[i]=='.'||s[i]=='_')) i++;
        *bytes=i-at;return 4;
    }
    if((s[at]>='a'&&s[at]<='z')||(s[at]>='A'&&s[at]<='Z')||s[at]=='_') {
        size_t i=at+1;
        while(i<len&&isid((unsigned char)s[i])) i++;
        *bytes=i-at;
        return keyword(s+at,i-at)?3:0;
    }
    if((s[at]=='#'&&slash)||(s[at]=='@'&&html)) {*bytes=rest;return 6;}
    if(strchr("{}[]();=+-*/!%^&|<>?:,.~",s[at])) return 5;
    return 0;
}
