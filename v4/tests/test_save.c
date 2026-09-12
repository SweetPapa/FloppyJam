#define _POSIX_C_SOURCE 200809L
#include "maglava.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);return 1;} } while(0)
int main(int argc,char **argv) {
    CHECK(argc==2);
#ifdef _WIN32
    _putenv_s("MAGLAVA_SAVE_PATH",argv[1]);
#else
    setenv("MAGLAVA_SAVE_PATH",argv[1],1);
#endif
    remove(argv[1]);
    SaveData s,t;save_load(&s);CHECK(s.unlocked==1 && s.lava_rate==1.5f && !*s.language);
    s.lava_rate=2.0f;snprintf(s.language,sizeof s.language,"zh-CN");
    s.best_time[3]=12.25f;s.best_score[3]=2200;s.reduced_motion=1;s.muted=1;
    save_record(&s,4,3);save_load(&t);
    CHECK(t.unlocked==5&&t.stars[3]==3&&t.best_time[3]==12.25f&&t.best_score[3]==2200);
    CHECK(t.reduced_motion&&t.muted && t.lava_rate==2.0f && !strcmp(t.language,"zh-CN"));
    /* Exact version-2 layout remains readable; new preferences get defaults. */
    FILE *oldfile=fopen(argv[1],"rb");CHECK(oldfile);
    unsigned char bytes[8192];size_t length=fread(bytes,1,sizeof bytes,oldfile);fclose(oldfile);
    CHECK(length>20);uint32_t v2=2;memcpy(bytes+4,&v2,4);
    oldfile=fopen(argv[1],"wb");CHECK(oldfile);CHECK(fwrite(bytes,1,length-20,oldfile)==length-20);fclose(oldfile);
    save_load(&t);CHECK(t.stars[3]==3 && t.best_time[3]==12.25f && t.lava_rate==1.5f && !*t.language);
    save_record(&s,4,1);save_load(&t);CHECK(t.stars[3]==3);
    /* The exact old binary layout: 25 star bytes followed by a 32-bit unlock. */
    FILE *f=fopen(argv[1],"wb");CHECK(f);
    uint32_t magic=0x4D4C4756u,ver=1;int32_t unlocked=18;
    unsigned char old[25];for(int i=0;i<25;i++)old[i]=i%4;
    fwrite(&magic,4,1,f);fwrite(&ver,4,1,f);fwrite(old,1,25,f);fwrite(&unlocked,4,1,f);fclose(f);
    save_load(&t);
    int mapped=0;
    for(int i=0;i<LEVEL_COUNT;i++) {
        int id=LEVELS[i].legacy_id;
        if(id){CHECK(t.stars[i]==old[id-1]);if(id<=18)mapped=i+1;}
        else CHECK(t.stars[i]==0);
    }
    CHECK(t.unlocked==mapped && t.lava_rate==1.5f && !*t.language);
    save_store(&t);save_load(&s);CHECK(!memcmp(&s,&t,sizeof s));
    /* A truncated file must not partially load stars or unlocks. */
    f=fopen(argv[1],"wb");CHECK(f);ver=2;
    fwrite(&magic,4,1,f);fwrite(&ver,4,1,f);fwrite(old,1,7,f);fclose(f);
    save_load(&t);CHECK(t.unlocked==1&&t.stars[0]==0);
    remove(argv[1]);
    puts("PASS: desktop save roundtrip, best-star retention, legacy migration, truncated-file recovery");
    return 0;
}
