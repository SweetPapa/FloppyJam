#include "save.h"
#include <stdio.h>
#include <string.h>
static uint32_t sum(const SaveData*s){const unsigned char*p=(const unsigned char*)s;uint32_t h=2166136261u;for(size_t i=0;i<offsetof(SaveData,checksum);i++){h^=p[i];h*=16777619u;}return h;}
void sfbs_save_defaults(SaveData*s){memset(s,0,sizeof*s);memcpy(s->magic,"SBS3",4);memcpy(s->seed,"BAD234",7);s->mode=MODE_FLOW;s->volume=80;s->first_person=1;s->checksum=sum(s);}
bool sfbs_save_valid(const SaveData*s){return !memcmp(s->magic,"SBS3",4)&&s->seed[6]==0&&s->mode<=MODE_PULSE&&s->volume<=100&&s->language<4&&s->first_person<=1&&s->checksum==sum(s);}
bool sfbs_save_load(const char*p,SaveData*s){FILE*f=fopen(p,"rb");if(!f){sfbs_save_defaults(s);return false;}bool ok=fread(s,sizeof*s,1,f)==1&&sfbs_save_valid(s);fclose(f);if(!ok)sfbs_save_defaults(s);return ok;}
bool sfbs_save_write(const char*p,SaveData*s){s->checksum=sum(s);FILE*f=fopen(p,"wb");if(!f)return false;bool ok=fwrite(s,sizeof*s,1,f)==1;fclose(f);return ok;}
