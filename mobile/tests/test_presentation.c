#include "presentation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int peak;
static void layout(float w,float h) {
    float r[16];ml_control_layout(w,h,r);
    assert(r[0]==r[4] && r[1]<r[5]);
    assert(r[8]<r[4] && r[4]<r[12]);
    assert(r[9]==r[5] && r[5]==r[13]);
    for(int i=0;i<4;i++) {int k=i*4;assert(r[k]>=0 && r[k+1]>=0);assert(r[k]+r[k+2]<=w);assert(r[k+1]+r[k+3]<=h);assert(r[k+2]>=48);}
}
static void validate(MLScene *scene,float *s,float aspect,int reduced) {
    float before[ML_SNAPSHOT_SIZE];memcpy(before,s,sizeof before);
    ml_scene_build(scene,s,aspect,reduced);
    int count=ml_scene_vertex_count(scene);
    assert(count>0 && count%3==0);assert(!ml_scene_overflowed(scene));
    assert(ml_scene_opaque_count(scene)>0 && ml_scene_opaque_count(scene)<=count);
    const MLVertex *vertices=ml_scene_vertices(scene);
    for(int i=0;i<count;i++) {
        const float *p=(const float*)&vertices[i];for(int j=0;j<8;j++)assert(isfinite(p[j]));
        assert(vertices[i].w>10);assert(vertices[i].a>=0 && vertices[i].a<=1);
    }
    assert(!memcmp(s,before,sizeof before));if(count>peak)peak=count;
}
static void motion_and_restart(MLGame *game) {
    MLScene *scene=ml_scene_create(),*fresh=ml_scene_create();assert(scene && fresh);
    float s[ML_SNAPSHOT_SIZE];ml_start(game,1);ml_snapshot(game,s);
    s[9]=2;s[22]=0;
    validate(scene,s,.75f,1);
    int count=ml_scene_vertex_count(scene);size_t bytes=count*sizeof(MLVertex);
    MLVertex *still=malloc(bytes);assert(still);memcpy(still,ml_scene_vertices(scene),bytes);
    s[9]=8;validate(scene,s,.75f,1);
    assert(count==ml_scene_vertex_count(scene));assert(!memcmp(still,ml_scene_vertices(scene),bytes));free(still);
    // A capture burst from an earlier stage must not reappear when a new run's
    // timer eventually reaches the old event time. Dead state excludes trails.
    s[4]=3;s[9]=3.2f;s[22]=ML_DEATH;validate(scene,s,.75f,0);
    s[34]=2;s[9]=0;s[22]=0;validate(scene,s,.75f,0);
    s[9]=3.4f;validate(scene,s,.75f,0);validate(fresh,s,.75f,0);
    assert(ml_scene_vertex_count(scene)==ml_scene_vertex_count(fresh));
    assert(!memcmp(ml_scene_vertices(scene),ml_scene_vertices(fresh),ml_scene_vertex_count(scene)*sizeof(MLVertex)));
    ml_scene_destroy(scene);ml_scene_destroy(fresh);
}
static void crowded_scene(MLGame *game,MLScene *scene) {
    float s[ML_SNAPSHOT_SIZE];ml_start(game,1);ml_snapshot(game,s);
    s[13]=64;s[14]=64;s[15]=64;s[9]=2;s[22]=ML_ATTACH;
    for(int i=0;i<64;i++) {
        float x=65+(i%8)*58,y=s[3]-280+(i/8)*80;
        int k=40+i*6;s[k]=x;s[k+1]=y;s[k+2]=i%4;s[k+3]=1;s[k+4]=0;
        k=424+i*10;s[k]=3;s[k+1]=x+10;s[k+2]=y+30;s[k+3]=1;
        k=1064+i*3;s[k]=s[3]-320+i*10;s[k+1]=0;
    }
    for(int reduced=0;reduced<2;reduced++)validate(scene,s,.46f,reduced);
}
int main(void) {
    layout(320,144);layout(390,144);layout(768,144);layout(1024,112);
    assert(sizeof(MLVertex)==32);assert(!strcmp(ml_music_track(0),"magLava-main-theme"));
    MLScene *scene=ml_scene_create();MLGame *game=ml_create();assert(scene && game);
    int ghost_stages=0;
    for(int n=1;n<=ml_level_count();n++) {
        ml_start(game,n);float s[ML_SNAPSHOT_SIZE];ml_snapshot(game,s);
        assert(!strcmp(ml_music_track(n),ml_music_track(n%4)));
        float cameras[4]={s[3],s[40+(int)(s[13]/2)*6+1],s[40+((int)s[13]-1)*6+1],s[3]};
        int probes=3;
        for(int i=0;i<(int)s[14];i++)if(s[424+i*10]==3) {cameras[3]=s[426+i*10];probes=4;ghost_stages++;break;}
        for(int p=0;p<probes;p++)for(int motion=0;motion<2;motion++)for(int a=0;a<3;a++) {
            s[3]=cameras[p];s[9]=4.2f;s[22]=ML_ATTACH;
            validate(scene,s,(float[]){.46f,.75f,1.8f}[a],motion);
        }
    }
    assert(ghost_stages>0);motion_and_restart(game);crowded_scene(game,scene);
    assert(ml_music_count()==4);assert(!strcmp(ml_music_track(-1),ml_music_track(3)));
    for(int a=0;a<3;a++)for(int reduced=0;reduced<2;reduced++) {
        float s[ML_SNAPSHOT_SIZE],aspect=(float[]){.46f,.75f,1.8f}[a];
        ml_menu_snapshot(2,aspect,reduced,s);validate(scene,s,aspect,reduced);
        size_t bytes=ml_scene_vertex_count(scene)*sizeof(MLVertex);
        MLVertex *before=malloc(bytes);memcpy(before,ml_scene_vertices(scene),bytes);
        ml_menu_snapshot(4,aspect,reduced,s);validate(scene,s,aspect,reduced);
        int same=bytes==ml_scene_vertex_count(scene)*sizeof(MLVertex) && !memcmp(before,ml_scene_vertices(scene),bytes);
        assert(same==reduced);free(before);
        ml_start(game,1);ml_snapshot(game,s);s[8]=1;s[4]=3;s[35]=s[0];s[36]=s[1];
        /* First frame received after a hit: no transient event bit required. */
        for(int i=0;i<12;i++) {s[37]=i*.1f;s[38]=3+s[37];validate(scene,s,aspect,reduced);}
    }
    ml_scene_destroy(scene);ml_destroy(game);
    printf("Shared 3D scene: 40 stages, multiple camera heights, %d ghost stages, 3 aspects, motion freeze, effect reset, max-capacity crowd; peak %d vertices.\n",ghost_stages,peak);
}
