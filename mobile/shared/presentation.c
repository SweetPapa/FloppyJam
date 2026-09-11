#include "presentation.h"
#include "maglava.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define PI 3.14159265359f
#define CAPACITY 196608
#define GLOW_CAP 512
#define TRAIL_CAP 24
typedef struct { float x,y,z; } V;
typedef struct { float r,g,b; } C;
typedef struct { V p; float radius,alpha; C c; int sparkle; } Glow;
struct MLScene {
    MLVertex vertices[CAPACITY];
    int count,opaque,overflow,ng;
    Glow glows[GLOW_CAP];
    V trail[TRAIL_CAP]; int trail_count;
    float last_time,last_deaths,last_level;
    float burst_time,event_time,burst_x,burst_y; C burst_color;
    float camera,aspect,span,distance,roll,tilt,pan,px,py,lava,time;
    int reduced,flash,detail;
    float sin_pan,cos_pan,sin_tilt,cos_tilt,sin_roll,cos_roll;
};
static V v(float x,float y,float z) { return (V){x,y,z}; }
static V sub(V a,V b) { return v(a.x-b.x,a.y-b.y,a.z-b.z); }
static V cross(V a,V b) { return v(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
static V norm(V a) { float d=sqrtf(a.x*a.x+a.y*a.y+a.z*a.z); return d>0?v(a.x/d,a.y/d,a.z/d):v(0,0,1); }
static float clamp(float x,float lo,float hi) { return fmaxf(lo,fminf(hi,x)); }
static C color(unsigned n) { return (C){((n>>16)&255)/255.f,((n>>8)&255)/255.f,(n&255)/255.f}; }
static C mix(C a,C b,float t) { return (C){a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t}; }
static C scale(C c,float a) { return (C){c.r*a,c.g*a,c.b*a}; }
static const unsigned colors[4]={0xFF526C,0x499FFF,0xFFD34D,0x46EDAA};
unsigned ml_magnet_color(int c) { return colors[c>=0&&c<4?c:0]; }
unsigned ml_accent_color(int level) {
    static const unsigned accents[]={0x43DCEB,0xB78AFF,0xFFAF56,0x58E8B1,0xFC7BC1,0x7EA5FF};
    return accents[(level>0?(level-1)/4:0)%6];
}
const char *ml_chapter_name(int level) {
    static const char *names[]={"IGNITION","VIOLET CIRCUIT","MOLTEN FOUNDRY","ION GARDEN","AFTERBURN","BLUE SHIFT","REACTOR HEART","POLARITY","ESCAPE VELOCITY","THE FINAL ASCENT"};
    return names[level>0?(level-1)/4%10:0];
}
static uint32_t hash(uint32_t x) { x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16); }
static void vertex(MLScene *s,V p,C c,float alpha) {
    if(s->count>=CAPACITY) { s->overflow=1; return; }
    if(s->flash) {
        float d=hypotf(p.x-s->px,p.y-s->py);
        c=scale(c,1-.94f*clamp((d-225)/115,0,1));
    }
    float x=p.x-270, y=s->camera-p.y, z=p.z;
    // Gentle camera orbit around the gameplay plane reveals structural depth.
    float xx=x*s->cos_pan+z*s->sin_pan; z=-x*s->sin_pan+z*s->cos_pan; x=xx;
    float yy=y*s->cos_tilt-z*s->sin_tilt; z=y*s->sin_tilt+z*s->cos_tilt; y=yy;
    xx=x*s->cos_roll-y*s->sin_roll; y=x*s->sin_roll+y*s->cos_roll; x=xx;
    float w=s->distance-z;
    const float near=10,far=5000;
    float f=s->distance/s->span;
    s->vertices[s->count++]=(MLVertex){x*f/s->aspect,y*f,((far+near)/(far-near))*w-2*far*near/(far-near),w,c.r,c.g,c.b,alpha};
}
static void tri(MLScene *s,V a,V b,V c,C col,float emission) {
    V n=norm(cross(sub(c,a),sub(b,a)));
    float light=.40f+.60f*fmaxf(0,n.x*-.35f+n.y*-.45f+n.z*.82f);
    C lit=scale(col,light*(1-emission)+emission);
    // Warm bounce from lava and cool ambient light keep forms legible.
    float heat=clamp(1-fabsf(a.y-s->lava)/300,0,1)*.22f*(1-emission);
    lit=mix(lit,color(0xFF6A22),heat);
    vertex(s,a,lit,1); vertex(s,b,lit,1); vertex(s,c,lit,1);
}
static void quad(MLScene *s,V a,V b,V c,V d,C col,float e) { tri(s,a,b,c,col,e); tri(s,a,c,d,col,e); }
static void box(MLScene *s,float x,float y,float z,float w,float h,float depth,C c,float e) {
    float l=x-w/2,r=x+w/2,t=y-h/2,b=y+h/2,f=z+depth/2,k=z-depth/2;
    quad(s,v(l,t,f),v(l,b,f),v(r,b,f),v(r,t,f),c,e);
    quad(s,v(l,t,k),v(l,t,f),v(r,t,f),v(r,t,k),c,e);
    quad(s,v(r,t,k),v(r,t,f),v(r,b,f),v(r,b,k),c,e);
    quad(s,v(l,b,k),v(r,b,k),v(r,b,f),v(l,b,f),c,e);
    quad(s,v(l,t,k),v(l,b,k),v(l,b,f),v(l,t,f),c,e);
}
static void beam(MLScene *s,V a,V b,float radius,C c,float e) {
    V axis=norm(sub(b,a)), u=norm(cross(axis,fabsf(axis.z)<.8f?v(0,0,1):v(0,1,0))), w=cross(axis,u);
    for(int i=0;i<8;i++) {
        float aa=i*PI/4, bb=(i+1)*PI/4;
        V o=v((u.x*cosf(aa)+w.x*sinf(aa))*radius,(u.y*cosf(aa)+w.y*sinf(aa))*radius,(u.z*cosf(aa)+w.z*sinf(aa))*radius);
        V p=v((u.x*cosf(bb)+w.x*sinf(bb))*radius,(u.y*cosf(bb)+w.y*sinf(bb))*radius,(u.z*cosf(bb)+w.z*sinf(bb))*radius);
        quad(s,v(a.x+o.x,a.y+o.y,a.z+o.z),v(b.x+o.x,b.y+o.y,b.z+o.z),v(b.x+p.x,b.y+p.y,b.z+p.z),v(a.x+p.x,a.y+p.y,a.z+p.z),c,e);
    }
}
static C polished(V n,C c,float emission) {
    float diffuse=fmaxf(0,n.x*-.35f+n.y*-.45f+n.z*.82f);
    float h=fmaxf(0,n.x*-.185f+n.y*-.238f+n.z*.954f);
    float h2=h*h,h4=h2*h2,h8=h4*h4,h16=h8*h8;
    float rim=1-fabsf(n.z);rim=rim*rim*rim;
    C lit=scale(c,(.18f+.82f*diffuse)*(1-emission)+emission);
    lit=mix(lit,color(0xD5EDFF),rim*.24f*(1-emission));
    return mix(lit,color(0xFFFFFF),clamp((h16*h16*.9f+h8*.12f)*(1-emission),0,.94f));
}
static void smooth_quad(MLScene *s,V *p,V *n,C c,float e) {
    const int order[6]={0,1,2,0,2,3};
    C lit[4];for(int i=0;i<4;i++)lit[i]=polished(n[i],c,e);
    for(int i=0;i<6;i++)vertex(s,p[order[i]],lit[order[i]],1);
}
static void ring(MLScene *s,float x,float y,float z,float r,float tube,C c,float e) {
    int segments=s->detail?32:16,sides=s->detail?6:4;
    for(int i=0;i<segments;i++) for(int j=0;j<sides;j++) {
        V p[4],n[4];
        for(int k=0;k<4;k++) {
            float a=(i+(k==1||k==2))*2*PI/segments, b=(j+(k>=2))*2*PI/sides;
            float ca=cosf(a),sa=sinf(a),cb=cosf(b),sb=sinf(b);
            p[k]=v(x+(r+tube*cb)*ca,y+(r+tube*cb)*sa,z+tube*sb);
            n[k]=v(cb*ca,cb*sa,sb);
        }
        smooth_quad(s,p,n,c,e);
    }
}
static void sphere(MLScene *s,float x,float y,float z,float r,C c,float e) {
    int segments=r<10?8:(s->detail?24:12),rows=r<10?4:(s->detail?12:6);
    for(int j=0;j<rows;j++) for(int i=0;i<segments;i++) {
        V p[4],n[4];
        for(int k=0;k<4;k++) {
            float a=(i+(k==1||k==2))*2*PI/segments,b=-PI/2+(j+(k>=2))*PI/rows;
            n[k]=v(cosf(b)*cosf(a),cosf(b)*sinf(a),sinf(b));
            p[k]=v(x+r*n[k].x,y+r*n[k].y,z+r*n[k].z);
        }
        smooth_quad(s,p,n,c,e);
    }
}
static void glow(MLScene *s,float x,float y,float z,float r,C c,float a) {
    if(s->ng<GLOW_CAP) s->glows[s->ng++]=(Glow){v(x,y,z),r,a,c,0};
}
static void sparkle(MLScene *s,float x,float y,float z,float size,C c,float alpha) {
    if(s->ng<GLOW_CAP)s->glows[s->ng++]=(Glow){v(x,y,z),size,alpha,c,1};
}
static void ghost(MLScene *s,float x,float y,int id) {
    C green=color(0x54FBA1),mint=color(0xB2FFE3);
    float pulse=s->reduced?1:.92f+.08f*sinf(s->time*2.6f+id);
    // The original roamer is a luminous green eye. Its solid body matches the
    // canonical 22-unit circular collision radius; wisps are light only.
    sphere(s,x,y,0,ROAMER_SIZE,green,.18f);
    glow(s,x,y,-2,49,green,.42f*pulse);
    glow(s,x,y,-4,66,mint,.10f*pulse);
    float look=atan2f(s->py-y,s->px-x);
    float ex=x+cosf(look)*5,ey=y+sinf(look)*5;
    sphere(s,ex,ey,20,8.2f,color(0x071E28),.05f);
    sphere(s,ex+cosf(look)*1.5f,ey+sinf(look)*1.5f,27,3.8f,mint,.55f);
    sphere(s,ex-2,ey-2,27.8f,1.8f,color(0xFFFFFF),1);
    // Drifting spectral wisps cannot alter the collision silhouette.
    if(!s->reduced)for(int j=0;j<7;j++) {
        float a=s->time*.65f+id+j*2.4f;
        float r=25+j*2.5f,alpha=.26f*(1-j/8.f);
        float px=x+cosf(a)*r,py=y+12+j*4+sinf(a)*6;
        glow(s,px,py,-6-j,7-j*.5f,green,alpha);
        if(j%3==0)sparkle(s,px,py,2,3,mint,.55f);
    }
}
// Tiny geometric labels remain identical on both rasterizers; UI uses Barlow.
static unsigned glyph(char c) {
    switch(c) {
    case 'R':return 0x6BAD;
    case 'B':return 0x6BAE;
    case 'Y':return 0x5A92;
    case 'G':return 0x396B;
    case 'E':return 0x79A7;
    case 'X':return 0x5AAD;
    case 'I':return 0x7497;
    case 'T':return 0x7492;
    case 'C':return 0x3923;
    case 'P':return 0x6BA4;
    case '+':return 0x05D0;
    case '-':return 0x01C0;
    default:return 0;
    }
}
static void label(MLScene *s,const char *str,float x,float y,float z,float unit,C c) {
    int len=(int)strlen(str); x-=((len*4-1)*unit)/2;
    for(int k=0;k<len;k++) { unsigned bits=glyph(str[k]);
        for(int j=0;j<5;j++) for(int i=0;i<3;i++) if(bits&(1u<<(14-j*3-i)))
            box(s,x+(k*4+i)*unit,y+(j-2)*unit,z,unit*.85f,unit*.85f,1,c,1);
    }
}
static void ship(MLScene *s,float x,float y,C c) {
    V top=v(x,y-20,0),left=v(x-16,y+14,0),right=v(x+16,y+14,0),ridge=v(x,y+3,15);
    tri(s,top,left,ridge,color(0xDAF4FF),.25f); tri(s,top,ridge,right,color(0xFFFFFF),.3f);
    tri(s,left,right,ridge,color(0x658BA4),.1f);
    beam(s,top,left,1.5f,c,1); beam(s,left,right,1.5f,c,1); beam(s,right,top,1.5f,c,1);
    sphere(s,x,y+4,15,3.5f,c,1);
    glow(s,x,y+15,-2,32,c,.28f);
}
MLScene *ml_scene_create(void) { return calloc(1,sizeof(MLScene)); }
void ml_scene_destroy(MLScene *s) { free(s); }
const MLVertex *ml_scene_vertices(const MLScene *s) { return s->vertices; }
int ml_scene_vertex_count(const MLScene *s) { return s->count; }
int ml_scene_opaque_count(const MLScene *s) { return s->opaque; }
int ml_scene_overflowed(const MLScene *s) { return s->overflow; }
void ml_control_layout(float w,float h,float *out) {
    float size=fmaxf(1,fminf(68,fminf((w-32)/3,(h-8)/2))),gap=8;
    const int cols[4]={1,1,0,2},rows[4]={0,1,1,1};
    for(int i=0;i<4;i++) { out[i*4]=(w-(size*3+gap*2))/2+cols[i]*(size+gap); out[i*4+1]=(h-(size*2+gap))/2+rows[i]*(size+gap); out[i*4+2]=size; out[i*4+3]=size; }
}
int ml_music_count(void) { return 4; }
const char *ml_music_track(int index) {
    static const char *tracks[]={"magLava-main-theme","magLava-game-bg-1","magLava-game-bg-2","magLava-game-bg-3"};
    return tracks[(index%4+4)%4];
}
void ml_scene_build(MLScene *s,const float *d,float aspect,int reduced) {
    s->count=s->ng=s->overflow=0; s->camera=d[3]; s->aspect=clamp(aspect,.2f,5);
    s->span=fmaxf(360,320/s->aspect); s->distance=1100;
    s->reduced=reduced; s->time=reduced?0:(d[38]>0?d[38]:d[9]); s->px=d[0]; s->py=d[1]; s->lava=d[2]; s->flash=d[28]==1 && d[4]!=3;
    s->roll=reduced?0:-d[12]*PI/180*.45f; s->tilt=reduced?0:.055f;
    s->pan=reduced?0:clamp((d[0]-270)/270,-1,1)*.055f;
    float hit=d[37];
    if(!reduced && d[8]>0 && hit>=0 && hit<.32f) {
        float kick=1-hit/.32f; s->camera+=sinf(hit*79)*5*kick;
        s->roll+=sinf(hit*61)*.013f*kick;
    }
    s->sin_pan=sinf(s->pan);s->cos_pan=cosf(s->pan);
    s->sin_tilt=sinf(s->tilt);s->cos_tilt=cosf(s->tilt);
    s->sin_roll=sinf(s->roll);s->cos_roll=cosf(s->roll);
    int level=(int)d[34];
    if(d[9]<s->last_time || level!=s->last_level) {
        s->trail_count=0;s->burst_time=-1;s->event_time=-1;
    }
    if(d[8]!=s->last_deaths || reduced)s->trail_count=0;
    if(d[4]!=3 && d[9]-s->last_time>=1.f/60) {
        if(s->trail_count==TRAIL_CAP) { memmove(s->trail,s->trail+1,(TRAIL_CAP-1)*sizeof(V)); s->trail_count--; }
        s->trail[s->trail_count++]=v(d[0],d[1],0); s->last_time=d[9];
    }
    if(d[9]<s->last_time) s->last_time=d[9];
    s->last_deaths=d[8]; s->last_level=(float)level;
    int visible=0;
    for(int i=0;i<(int)d[13];i++)if(d[40+i*6+3]&&fabsf(d[40+i*6+1]-s->camera)<s->span*1.4f+80)visible++;
    for(int i=0;i<(int)d[14];i++)if(d[424+i*10+3]&&fabsf(d[424+i*10+2]-s->camera)<s->span*1.4f+180)visible++;
    s->detail=visible<=28;
    C accent=color(ml_accent_color(level)),wall=mix(color(0x283B55),accent,.14f),deep=mix(color(0x0B1426),accent,.045f);
    float half=s->span*1.4f, top=s->camera-half,bottom=s->camera+half;
    box(s,270,s->camera,-235,670,half*2,12,deep,0);
    // Recessed walls, structural ribs and light conduits.
    for(int side=0;side<2;side++) {
        float x=side?552:-12;
        box(s,x,s->camera,-113,26,half*2,236,wall,0);
        box(s,side?531:9,s->camera,-30,5,half*2,8,accent,.9f);
        box(s,side?508:32,s->camera,-190,7,half*2,12,scale(accent,.5f),.65f);
    }
    int first=(int)floorf(top/160),last=(int)ceilf(bottom/160);
    for(int row=first;row<=last;row++) {
        float y=row*160.f; uint32_t h=hash((uint32_t)row+(uint32_t)level*7919);
        box(s,270,y,-222,526,150,8,scale(wall,.34f+(h%5)*.035f),.15f);
        box(s,270,y-77,-202,540,5,22,wall,0);
        for(int side=0;side<2;side++) {
            float x=side?530:10;
            box(s,x,y-77,-115,30,12,225,wall,0);
            box(s,x,y,-90,14,52,26,color(0x111C31),0);
            box(s,x,y,-73,6,34,6,accent,1);
            glow(s,x,y,-60,35,accent,.16f);
            float hx=side?453:87;
            box(s,hx,y+28,-204,54,52,17,wall,0);
            for(int j=0;j<4;j++) box(s,hx,y+12+j*9,-193,35,3,2,deep,0);
            sphere(s,x,y-77,-2,3,color(0xC9D9E5),.15f);
        }
        if((row%4+4)%4==0) {
            float rot=s->time*.4f+(h%10);
            ring(s,270,y,-205,83,6,wall,.1f);
            for(int j=0;j<5;j++) { float a=rot+j*PI*2/5;
                beam(s,v(270+cosf(a)*20,y+sinf(a)*20,-196),v(270+cosf(a+.4f)*73,y+sinf(a+.4f)*73,-196),10,scale(wall,.7f),0);
            }
            sphere(s,270,y,-185,16,wall,0);
        }
        if((h%3)==0) for(int side=0;side<2;side++) {
            float x=side?382:158;
            beam(s,v(x-18,y+55,-198),v(x,y+35,-198),2,accent,.6f);
            beam(s,v(x,y+35,-198),v(x+18,y+55,-198),2,accent,.6f);
        }
    }
    for(int i=0;i<(int)d[15];i++) {
        int k=1064+i*3; float y=d[k]; if(fabsf(y-s->camera)>half)continue;
        C c=color(d[k+1]>0?0x56F2AF:0x526E92);
        beam(s,v(25,y,-14),v(515,y,-14),1.5f,c,1);
        if(s->detail)label(s,"CP",270,y-18,-10,2.5f,c);
    }
    // Visible collision geometry stays on the original simulation plane.
    for(int i=0;i<(int)d[14];i++) {
        int k=424+i*10,type=(int)d[k]; float x=d[k+1],y=d[k+2];
        if(!d[k+3]||fabsf(y-s->camera)>half+180)continue;
        C amber=color(0xFFAE46),danger=color(0xFF5365);
        if(type==0) {
            float a=d[k+4]*PI/180; V end=v(x+cosf(a)*d[k+7],y+sinf(a)*d[k+7],0);
            beam(s,v(x,y,0),end,6,amber,.2f); beam(s,v(x,y,6),v(end.x,end.y,6),1.5f,color(0xFFF0B6),1);
            sphere(s,x,y,0,12,wall,0); ring(s,x,y,10,9,2,amber,1);
        } else if(type==1) {
            ring(s,x,y,0,d[k+5],4,danger,1); sphere(s,x,y,0,7,danger,.5f);
            glow(s,x,y,-8,d[k+5]+12,danger,.06f);
        } else if(type==2) {
            C c=d[k+6]==2?danger:d[k+6]==1?amber:scale(wall,1.3f);
            beam(s,v(x,y,0),v(d[k+8],y,0),d[k+6]==2?3:1,c,1);
            box(s,x,y,0,12,24,20,wall,0);box(s,d[k+8],y,0,12,24,20,wall,0);
            if(d[k+6]>0) for(int j=0;j<8;j++)glow(s,x+(d[k+8]-x)*j/7,y,2,22,c,.18f);
        } else if(type==3) {
            ghost(s,x,y,i);
        } else {
            C c=color(colors[d[k+9]==0?1:0]);
            ring(s,x,y,-10,120,.7f,scale(c,.22f),1); sphere(s,x,y,0,18,wall,0);
            ring(s,x,y,8,16,2,c,1); label(s,d[k+9]==0?"-":"+",x,y,21,3,c);
        }
    }
    for(int i=0;i<(int)d[13];i++) {
        int k=40+i*6; float x=d[k],y=d[k+1]; if(!d[k+3]||fabsf(y-s->camera)>half+80)continue;
        int ci=(int)d[k+2]; C c=color(colors[ci]); int target=0;
        for(int j=30;j<34;j++) if((int)d[j]==i)target=1;
        // Recessed chrome collar and a smoothly lit, glassy colored orb.
        beam(s,v(x,y,-214),v(x,y,-23),7,scale(wall,.8f),0);
        box(s,x,y,-30,44,44,14,wall,0);
        ring(s,x,y,-12,22,4,color(0x7190AA),.05f);
        ring(s,x,y,-5,20,2.2f,c,.30f);
        sphere(s,x,y,-1,18,c,.08f);
        glow(s,x,y,-3,target?57:42,c,target?.32f:.18f);
        label(s,(const char*[]){"R","B","Y","G"}[ci],x,y,20,2.8f,color(0xE8F5FF));
        if(target)ring(s,x,y,0,30,1.2f,mix(c,color(0xFFFFFF),.25f),1);
        if(!reduced&&s->detail) {
            float phase=s->time*.8f+i*1.7f;
            sparkle(s,x-8,y-10,18,3.4f,mix(c,color(0xFFFFFF),.85f),.50f+.15f*sinf(phase));
            if(target||d[k+4])for(int j=0;j<3;j++) {
                float a=phase+j*PI*2/3;
                sparkle(s,x+cosf(a)*34,y+sinf(a)*24,4+cosf(a)*12,3.0f,c,.68f);
            }
        }
        if(d[k+4]) {
            C gold=color(0xFFE5A0);
            ring(s,x,y,-4,44,3.5f,gold,.30f);label(s,"EXIT",x,y-60,0,3,gold);
            glow(s,x,y,-8,82,color(0xFFD366),.24f);
        }
    }
    C active=color(colors[(int)clamp(d[25],0,3)]);
    if(!reduced) for(int i=1;i<s->trail_count;i++) beam(s,s->trail[i-1],s->trail[i],1+i*.055f,scale(active,.12f+.45f*i/s->trail_count),1);
    int tether=(int)(d[24]>=0?d[24]:d[23]);
    if(tether>=0&&d[4]!=3) {
        int k=40+tether*6;beam(s,v(d[0],d[1],0),v(d[k],d[k+1],0),1.4f,active,1);
        if(!reduced)for(int j=0;j<4;j++) {
            float t=fmodf(s->time*1.15f+j*.25f,1);
            sparkle(s,d[0]+(d[k]-d[0])*t,d[1]+(d[k+1]-d[1])*t,4,3.2f,mix(active,color(0xFFFFFF),.65f),.65f);
        }
    }
    if(d[18]) {
        C violet=color(0xBD8BFF);
        sphere(s,d[19],d[20],0,18,violet,.12f);ring(s,d[19],d[20],0,25,1.5f,color(0xD7B1FF),1);
        glow(s,d[19],d[20],-4,54,violet,.35f);
    }
    if(d[4]!=3) { ship(s,d[0],d[1],active); if(d[11]>0)ring(s,d[0],d[1],0,28,1.2f,color(0xA2E8FF),1); }
    // Impact origin and age come from the core, independent of renderer frame delivery.
    if(d[8]>0 && hit>=0 && hit<1.05f && s->detail) {
        float x=d[35],y=d[36],fade=1-hit/1.05f;
        C hot=color(0xFFC987),cool=color(0x8FEAFF);
        if(reduced) {
            ring(s,x,y,16,32,2.5f,scale(hot,.65f),1);
        } else {
            glow(s,x,y,20,48+hit*120,hot,fade*.65f);
            if(hit<.25f)glow(s,x,y,22,58,color(0xFFF1CE),(1-hit/.25f)*.85f);
            for(int j=0;j<2;j++) {
                float t=hit-j*.10f;
                if(t>=0 && t<.7f)ring(s,x,y,12-j*8,12+t*(165+j*40),2.5f*(1-t/.7f)+.3f,scale(j?cool:hot,(1-t/.7f)*.9f),1);
            }
            for(int i=0;i<36;i++) {
                uint32_t h=hash(i+771); float a=i*2*PI/36, speed=65+h%170;
                float dx=cosf(a)*hit*speed,dy=sinf(a)*hit*speed+hit*hit*95;
                float z=10+sinf(a*3)*hit*35;
                C c=i%3?hot:cool;
                sparkle(s,x+dx,y+dy,z,2.5f+fade*3,c,fade*.85f);
                if(i<14) {
                    float r=(2+h%4)*fade,spin=a+hit*(4+h%9);
                    V p=v(x+dx,y+dy,z);
                    tri(s,v(p.x+cosf(spin)*r*2,p.y+sinf(spin)*r*2,p.z),
                          v(p.x+cosf(spin+2.2f)*r,p.y+sinf(spin+2.2f)*r,p.z+3),
                          v(p.x+cosf(spin-2.2f)*r,p.y+sinf(spin-2.2f)*r,p.z-3),c,.4f);
                    beam(s,v(x+dx*.84f,y+dy*.84f,z),p,.7f*fade,scale(c,fade),1);
                }
            }
        }
    }
    // Lava is a rippling 3D surface extending into the shaft. All lava stays
    // behind z=-12 so it never hides player/hazard collision silhouettes.
    if(s->lava<bottom) {
        float lava=s->lava;
        box(s,270,(lava+4+bottom)/2,-122,540,fmaxf(1,bottom-lava-4),210,color(0xB82618),.8f);
        // Continuous molten currents across the front face, instead of a flat red slab.
        int rows=(int)ceilf(fminf(bottom-lava,2200)/32);
        for(int i=0;i<27;i++)for(int j=0;j<rows;j++) {
            V p[4];C c[4];
            for(int k=0;k<4;k++) {
                float x=(i+(k==1||k==2))*20,y=lava+5+(j+(k>=2))*32;
                float flow=sinf(x*.039f+sinf(y*.017f+s->time*.5f)*1.9f+s->time*.38f)
                          *cosf(y*.027f-x*.011f-s->time*.3f);
                c[k]=mix(color(0x761C1A),color(0xFF962F),clamp(.45f+flow*.62f,0,1));
                p[k]=v(x,y,-15);
            }
            const int order[6]={0,1,2,0,2,3};
            for(int k=0;k<6;k++)vertex(s,p[order[k]],c[order[k]],1);
        }
        for(int i=0;i<27;i++)for(int j=0;j<8;j++) {
            V p[4]; for(int k=0;k<4;k++) {
                float x=(i+(k==1||k==2))*20,z=-222+(j+(k>=2))*26;
                float wave=reduced?0:sinf(x*.036f+s->time*2.1f+z*.028f)*3;
                p[k]=v(x,lava+wave,z);
            }
            float h=.5f+.5f*sinf(i*.73f+j*1.4f-s->time*1.7f);
            quad(s,p[0],p[1],p[2],p[3],mix(color(0xED481C),color(0xFFE181),h),1);
        }
        for(int i=0;i<27;i++) {
            float x=i*20.f, y=lava+(reduced?0:sinf(x*.036f+s->time*2.1f-.336f)*3);
            beam(s,v(x,y,-12),v(x+20,lava+(reduced?0:sinf((x+20)*.036f+s->time*2.1f-.336f)*3),-12),2.5f,color(0xFFE8A5),1);
            glow(s,x,lava+7,-10,49,color(0xFF621C),.25f);
        }
        if(!reduced)for(int i=0;i<28;i++) {
            uint32_t h=hash(i+27); float age=fmodf(s->time*(.18f+(h%5)*.015f)+(h%100)/100.f,1);
            float x=(h%520)+10+sinf(age*5+i)*7, y=lava-age*170;
            glow(s,x,y,-20-(float)(h%130),3+(1-age)*3,color(0xFFAE48),(1-age)*.75f);
        }
    }
    if(!reduced) {
        // Slow twinkling motes at different depths reinforce shaft parallax.
        if(s->detail)for(int i=0;i<22;i++) {
            uint32_t h=hash((uint32_t)i+level*991u);
            float drift=fmodf(s->time*(7+h%13)+(h%2000),half*2);
            float x=35+(h%470),y=top+drift,z=-40-(float)(h%140);
            float alpha=.16f+.16f*(.5f+.5f*sinf(s->time*1.3f+i));
            sparkle(s,x,y,z,2+(h%3),accent,alpha);
        }
        int events=(int)d[22];
        if(d[9]!=s->event_time && (events&(ML_ATTACH|ML_CHECKPOINT|ML_COMPLETE))) {
            s->event_time=d[9];s->burst_time=d[9];s->burst_x=d[0];s->burst_y=d[1];
            s->burst_color=events&ML_DEATH?color(0xFF7048):events&(ML_CHECKPOINT|ML_COMPLETE)?color(0xFFE7AA):active;
        }
        float age=d[9]-s->burst_time;
        if(s->burst_time>0 && age>=0 && age<.65f)for(int i=0;i<20;i++) {
            float a=i*PI/10, radius=age*(55+(hash(i)%85));
            sparkle(s,s->burst_x+cosf(a)*radius,s->burst_y+sinf(a)*radius+age*age*50,12,4.5f,s->burst_color,(1-age/.65f)*.9f);
        }
    }
    // Additive soft halos share the depth buffer but never write to it.
    s->opaque=s->count;
    for(int i=0;i<s->ng;i++) {
        Glow g=s->glows[i];
        int segments=s->detail?20:12;
        // Decorative light is budgeted after every solid object. Dense scenes
        // keep all enemies/nodes; excess halo detail is allowed to drop.
        int needed=segments*15+(g.sparkle?12:0);
        if(s->count+needed>CAPACITY)break;
        const float radii[4]={0,.22f,.55f,1},opacity[4]={1,.60f,.15f,0};
        float radius=g.radius*(g.sparkle?2.2f:1);
        for(int band=0;band<3;band++)for(int j=0;j<segments;j++) {
            float a=j*2*PI/segments,b=(j+1)*2*PI/segments;
            V p[4]={v(g.p.x+cosf(a)*radius*radii[band],g.p.y+sinf(a)*radius*radii[band],g.p.z),
                    v(g.p.x+cosf(b)*radius*radii[band],g.p.y+sinf(b)*radius*radii[band],g.p.z),
                    v(g.p.x+cosf(b)*radius*radii[band+1],g.p.y+sinf(b)*radius*radii[band+1],g.p.z),
                    v(g.p.x+cosf(a)*radius*radii[band+1],g.p.y+sinf(a)*radius*radii[band+1],g.p.z)};
            if(band>0) {vertex(s,p[0],g.c,g.alpha*opacity[band]);vertex(s,p[1],g.c,g.alpha*opacity[band]);vertex(s,p[2],g.c,g.alpha*opacity[band+1]);}
            vertex(s,p[0],g.c,g.alpha*opacity[band]);vertex(s,p[2],g.c,g.alpha*opacity[band+1]);vertex(s,p[3],g.c,g.alpha*opacity[band+1]);
        }
        if(g.sparkle)for(int j=0;j<4;j++) {
            float a=j*PI/2;C white=mix(g.c,color(0xFFFFFF),.8f);
            vertex(s,v(g.p.x+cosf(a)*g.radius,g.p.y+sinf(a)*g.radius,g.p.z),white,0);
            vertex(s,v(g.p.x+cosf(a-PI/2)*g.radius*.17f,g.p.y+sinf(a-PI/2)*g.radius*.17f,g.p.z),white,g.alpha);
            vertex(s,v(g.p.x+cosf(a+PI/2)*g.radius*.17f,g.p.y+sinf(a+PI/2)*g.radius*.17f,g.p.z),white,g.alpha);
        }
    }
}

void ml_menu_snapshot(float seconds,float aspect,int reduced,float *d) {
    memset(d,0,sizeof(float)*ML_SNAPSHOT_SIZE);
    float t=reduced?0:fmaxf(0,seconds),span=fmaxf(360,320/clamp(aspect,.2f,5));
    d[0]=270;d[4]=3;d[9]=d[38]=t;d[34]=9;d[37]=-1;
    d[2]=span*.34f+sinf(t*.35f)*12;d[23]=d[24]=-1;
    for(int c=0;c<4;c++)d[30+c]=-1;
    d[13]=4;
    for(int i=0;i<4;i++) {
        int k=40+i*6;d[k]=(i%2)?458:82;d[k+1]=-span*.72f+i*span*.22f;
        d[k+2]=i;d[k+3]=1;
    }
    // A decorative swing uses the same ship, tether and magnets as gameplay.
    // It never advances a level or writes progress; reduced motion freezes it.
    float angle=sinf(t*1.4f)*.75f;
    d[40]=270;d[41]=-span*.45f;
    d[0]=270+sinf(angle)*80;d[1]=d[41]+cosf(angle)*80;
    d[4]=1;d[23]=0;d[25]=0;
}
