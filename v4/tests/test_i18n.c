#include "i18n.h"
#include "maglava.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    assert(ml_language_count()==7);
    for(int i=0;i<7;i++) {
        ml_set_language(ml_language_code(i));assert(ml_language_index()==i);
        assert(*ml_language_name(i));
        const char *keys[]={"Settings","Exit","Language","Lava rise rate","Play","Locked","RED"};
        for(unsigned k=0;k<sizeof keys/sizeof keys[0];k++) {
            assert(*ml_text(keys[k]));if(i)assert(strcmp(ml_text(keys[k]),keys[k]));
        }
        for(int n=0;n<LEVEL_COUNT;n++) {
            assert(*ml_text(LEVELS[n].name));assert(*ml_text(LEVELS[n].hint));
            if(i) {assert(strcmp(ml_text(LEVELS[n].name),LEVELS[n].name));assert(strcmp(ml_text(LEVELS[n].hint),LEVELS[n].hint));}
        }
    }
    ml_set_language("pt_PT.UTF-8");assert(ml_language_index()==2);
    ml_set_language("es-MX");assert(ml_language_index()==1);
    ml_set_language("zh-Hans-CN");assert(ml_language_index()==6);
    ml_set_language("unsupported");assert(ml_language_index()==0);
    ml_set_language(NULL);assert(ml_language_index()==0);
    assert(!strcmp(ml_text("untranslated future key"),"untranslated future key"));
    puts("Seven-language UI/campaign coverage and system-locale fallback passed.");
}
