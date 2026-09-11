#include "mobile_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void snapshot(MLGame *g,float *s) { ml_snapshot(g,s); for(int i=0;i<ML_SNAPSHOT_SIZE;i++)assert(isfinite(s[i])); }
static void rate(int hz,float *out) {
    MLGame *g=ml_create();
    for(int frame=0;frame<hz*20;frame++) {
        if(frame%(hz/2)==0) {
            float s[ML_SNAPSHOT_SIZE]; snapshot(g,s);
            int best=-1; float y=s[1];
            for(int c=0;c<4;c++) { int idx=(int)s[30+c]; if(idx>=0 && s[40+idx*6+1]<y) { y=s[40+idx*6+1];best=c; } }
            ml_input(g,best);
        }
        ml_advance(g,1.0/hz);
    }
    snapshot(g,out); ml_destroy(g);
}
int main(void) {
    assert(ml_level_count()==40);
    assert(strstr(ml_level_hint(1),"Tap the color") != NULL); /* Private mobile-generated header, not desktop fallback. */
    assert(!strcmp(ml_level_key(0),""));
    float a[ML_SNAPSHOT_SIZE],b[ML_SNAPSHOT_SIZE];
    rate(60,a);
    for(int hz=120;hz<=240;hz+=120) {
        rate(hz,b);
        for(int i=0;i<ML_SNAPSHOT_SIZE;i++) if(i!=22)assert(fabsf(a[i]-b[i])<0.002f);
    }
    MLGame *g=ml_create(); snapshot(g,a);
    int color=-1; for(int c=0;c<4;c++)if(a[30+c]>=0) { color=c;break; } assert(color>=0);
    ml_input(g,color); ml_advance(g,1.0/240); snapshot(g,b); assert(b[4]==0);
    for(int i=0;i<3;i++)ml_advance(g,1.0/240);
    snapshot(g,b); assert(b[4]==1); assert((int)b[22]&ML_SWING);
    ml_pause(g,1); snapshot(g,a); ml_input(g,0); ml_advance(g,60); snapshot(g,b); assert(!memcmp(a,b,sizeof(a)));
    ml_pause(g,0); ml_advance(g,NAN); ml_advance(g,-10); snapshot(g,b); assert(!memcmp(a,b,sizeof(a)));
    ml_start(g,1); snapshot(g,a); ml_advance(g,600); snapshot(g,b); assert(b[9]<=0.101f);
    for(int n=1;n<=40;n++) { ml_start(g,n); snapshot(g,a); assert(a[34]==n); assert(a[13]>=2 && a[13]<=64); assert(a[14]<=64); assert(a[15]<=64); assert(a[9]==0); }
    /* Death and checkpoint respawn must not interpolate across the whole shaft. */
    ml_start(g,1); int died=0,respawned=0;float hit_x=0,hit_y=0,elapsed=0;
    for(int i=0;i<60*120;i++) { ml_advance(g,1.0/60); snapshot(g,a); if((int)a[22]&ML_DEATH) {died=1;hit_x=a[35];hit_y=a[36];elapsed=a[9];assert(a[37]==0);}
        if(died && a[4]==3 && a[37]>.1f) { assert(a[9]==elapsed);assert(a[35]==hit_x && a[36]==hit_y); }

        if((int)a[22]&ML_RESPAWN) { respawned=1; assert(a[4]==0); assert(fabsf(a[1]-a[3]-150)<.001f); break; } }
    assert(died && respawned);assert(a[37]>0 && a[35]==hit_x && a[36]==hit_y); ml_start(g,1);snapshot(g,a);assert(a[37]==-1 && a[38]==0);ml_destroy(g); puts("Mobile frame, input, pause, respawn, and campaign contracts passed");
}
