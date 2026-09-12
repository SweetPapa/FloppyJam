#include "i18n.h"
#include <string.h>
#include <ctype.h>
#include "i18n_gen.h"
static int language=0;
int ml_language_count(void) { return 7; }
int ml_language_index(void) { return language; }
const char *ml_language_code(int i) { return language_codes[i>=0&&i<7?i:0]; }
const char *ml_language_name(int i) { return language_names[i>=0&&i<7?i:0]; }
void ml_set_language(const char *code) {
    language=0;
    if(!code || strlen(code)<2)return;
    for(int i=0;i<7;i++) if(tolower((unsigned char)code[0])==language_codes[i][0] &&
        tolower((unsigned char)code[1])==language_codes[i][1]) { language=i;return; }
}
const char *ml_text(const char *s) {
    if(!s)return "";
    int lo=0,hi=(int)(sizeof messages/sizeof messages[0])-1;
    while(lo<=hi) { int mid=lo+(hi-lo)/2,c=strcmp(s,messages[mid][0]);
        if(!c)return messages[mid][language];
        if(c<0)hi=mid-1;else lo=mid+1;
    }
    return s;
}
const char *ml_locale_characters(void) { return locale_characters; }
