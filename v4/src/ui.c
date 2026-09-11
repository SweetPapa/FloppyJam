/* Desktop UI: a quiet frame around the climb. */
#include "app.h"
#include <stdio.h>
#include <math.h>
#include "fonts_gen.h"

static Font medium_font, bold_font;
void ui_init(void) {
    medium_font = LoadFontFromMemory(".ttf",font_medium,sizeof font_medium,48,0,0);
    bold_font = LoadFontFromMemory(".ttf",font_bold,sizeof font_bold,96,0,0);
    SetTextureFilter(medium_font.texture,TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(bold_font.texture,TEXTURE_FILTER_BILINEAR);
}
void ui_shutdown(void) { UnloadFont(medium_font); UnloadFont(bold_font); }
void ui_text(const char *text, int x, int y, int size, Color color) {
    DrawTextEx(size>=24?bold_font:medium_font,text,(Vector2){(float)x,(float)y},size,.6f,color);
}
int ui_text_width(const char *text,int size) {
    return (int)ceilf(MeasureTextEx(size>=24?bold_font:medium_font,text,size,.6f).x);
}

static const Color INK = {229,238,244,255};
static const Color DIM = {135,158,177,255};
static const Color AMBER = {255,155,83,255};
static const Color TEAL = {116,224,211,255};

static void centered(const char *s, int x, int y, int size, Color c) {
    ui_text(s,x-ui_text_width(s,size)/2,y,size,c);
}
static void panel(Rectangle r) {
    DrawRectangleRounded(r,.12f,6,(Color){12,21,31,245});
    DrawRectangleRoundedLines(r,.12f,6,(Color){40,58,73,255});
}
static void stars(int x, int y, int filled, int size) {
    for (int i=0;i<3;i++) {
        Vector2 pts[10], center={(float)x+(i-1)*size*2.6f,(float)y};
        for (int k=0;k<10;k++) {
            float ang=-PI/2+k*PI/5, r=(k&1)?size*.43f:(float)size;
            pts[k]=(Vector2){center.x+cosf(ang)*r,center.y+sinf(ang)*r};
        }
        for (int k=0;k<10;k++)
            DrawTriangle(center,pts[(k+1)%10],pts[k],i<filled?AMBER:(Color){53,70,85,255});
    }
}
static void keycap(int x,int y,const char *key,MagColor color,int available,int active) {
    Color c=mag_color(color,0);
    Rectangle r={(float)x,(float)y,32,30};
    DrawRectangleRounded(r,.18f,4,active?c:(Color){18,29,41,255});
    Color border=c; border.a=available?255:140;
    DrawRectangleRoundedLines(r,.18f,4,active?INK:border);
    Color ink=active?(Color){10,20,30,255}:INK;
    ui_text(key,x+5,y+7,16,ink);
    /* Draw arrow glyphs explicitly so every font shows the keyboard mapping. */
    Vector2 tip={(float)x+24,(float)y+10},tail={(float)x+24,(float)y+21};
    if(color==COL_BLUE) { tip.y=y+21;tail.y=y+10; }
    if(color==COL_YELLOW) { tip=(Vector2){x+19,y+15};tail=(Vector2){x+29,y+15}; }
    if(color==COL_GREEN) { tip=(Vector2){x+29,y+15};tail=(Vector2){x+19,y+15}; }
    DrawLineEx(tail,tip,1.5f,ink);
    float dx=(tail.x-tip.x)*.4f,dy=(tail.y-tip.y)*.4f;
    DrawLineEx(tip,(Vector2){tip.x+dx-dy*.8f,tip.y+dy+dx*.8f},1.5f,ink);
    DrawLineEx(tip,(Vector2){tip.x+dx+dy*.8f,tip.y+dy-dx*.8f},1.5f,ink);
    DrawRectangle(x+8,y+27,16,2,c);
}
static void menu_background(void) {
    int w=GetScreenWidth(),h=GetScreenHeight();
    DrawRectangleGradientV(0,0,w,h,(Color){8,16,25,255},(Color){17,29,40,255});
    for (int x=32;x<w;x+=64) DrawLine(x,0,x,h,(Color){35,57,72,35});
    for (int y=32;y<h;y+=64) DrawLine(0,y,w,y,(Color){35,57,72,35});
    DrawRectangleGradientV(0,h-160,w,160,(Color){96,37,18,0},(Color){96,37,18,80});
}

void ui_hud(App *a) {
    GameSim *g=&a->sim; int w=GetScreenWidth(),h=GetScreenHeight(); char b[160];
    panel((Rectangle){20,18,330,76});
    snprintf(b,sizeof b,"%s / 40    %s",LEVEL_LABELS[g->level_id-1],g->lv->name);
    ui_text(b,34,30,18,INK);
    snprintf(b,sizeof b,"%06d",(int)(a->score_shown+.5f));
    ui_text(b,34,57,24,AMBER);
    if (g->combo>1) {
        int combo=g->combo>COMBO_MAX?COMBO_MAX:g->combo;
        snprintf(b,sizeof b,"x%d FLOW",combo); ui_text(b,170,60,18,TEAL);
        DrawRectangle(170,83,150,2,(Color){41,61,72,255});
        DrawRectangle(170,83,(int)(150*fmaxf(0,g->combo_timer)/COMBO_TIMEOUT),2,TEAL);
    }
    panel((Rectangle){w-204,18,184,76});
    snprintf(b,sizeof b,"%05.1f",g->elapsed); ui_text(b,w-188,30,26,INK);
    snprintf(b,sizeof b,"PAR %.0fs   %d RETRIES",g->lv->par_time,g->deaths);
    ui_text(b,w-188,68,12,DIM);
    float start=g->lv->mag[0].y,top=g->lv->mag[g->n_mag-1].y,span=fmaxf(1,start-top);
    float prog=g->won?1:fminf(1,fmaxf(0,(start-g->py)/span));
    int railx=w-31, rtop=124, rbottom=h-124;
    DrawRectangle(railx,rtop,3,rbottom-rtop,(Color){48,66,78,255});
    for(int i=0;i<g->lv->n_cp;i++) {
        int y=rbottom-(int)((start-g->lv->cp[i].y)/span*(rbottom-rtop));
        DrawLine(railx-4,y,railx+7,y,i<=g->cur_cp?TEAL:DIM);
    }
    int py=rbottom-(int)(prog*(rbottom-rtop));
    DrawRectangle(railx,py,3,rbottom-py,TEAL); DrawCircle(railx+1,py,4,INK);
    float lava=fminf(1,fmaxf(0,(start-g->lava_y)/span));
    DrawCircle(railx+1,rbottom-(int)(lava*(rbottom-rtop)),4,AMBER);
    snprintf(b,sizeof b,"%.0f%%",prog*100); ui_text(b,w-51,rtop-22,14,DIM);

    /* Preserve the physical WASD / arrow-key layout: W above A S D. */
    panel((Rectangle){w/2-70,h-100,140,84});
    int exclude=g->attached_idx>=0?g->attached_idx:g->target_idx;
    static const char *keys[]={"W","S","A","D"};
    static const int offsets_x[]={0,0,-36,36};
    static const int offsets_y[]={0,36,36,36};
    for(int c=0;c<4;c++) keycap(w/2-16+offsets_x[c],h-91+offsets_y[c],keys[c],(MagColor)c,
        sim_find_magnet(g,g->px,g->py,(MagColor)c,exclude)>=0,g->color==(MagColor)c);
    ui_text("ESC pause   R retry",24,h-34,14,DIM);
    if(a->save.reduced_motion) ui_text("REDUCED MOTION",w-175,h-34,14,TEAL);
    else ui_text("V reduced motion",w-175,h-34,14,DIM);
    if(g->state==PS_DEAD) centered(g->cur_cp>=0?"RECONNECTING AT CHECKPOINT":"RECONNECTING AT START",w/2,h/2,22,INK);
    else if(g->lava_y-g->py<LAVA_WARNING_DIST && !g->game_over)
        centered("LAVA CLOSE  /  KEEP CLIMBING",w/2,h-130,18,AMBER);
    if(g->ai.active && !g->game_over)
        centered(g->race_lost?"RIVAL FINISHED - TRY AGAIN":g->ai.y<g->py?"RIVAL AHEAD":"YOU ARE AHEAD",w/2,110,18,TEAL);
    if(a->intro_t<5 && !g->game_over && g->lava_y-g->py>=LAVA_WARNING_DIST) {
        int y=h-142;
        panel((Rectangle){w/2-390,y-9,780,36});
        centered(g->lv->hint,w/2,y,14,INK);
    }
}

void ui_title(App *a) {
    menu_background();
    int w=GetScreenWidth(),h=GetScreenHeight(),x=w/2;
    centered("S W E E T   P A P A   T E C H N O L O G I E S",x,65,14,DIM);
    /* A magnetic climb drawn as a restrained title emblem. */
    Vector2 points[4]={{x-125,h*.29f+25},{x-40,h*.29f-26},{x+46,h*.29f+15},{x+128,h*.29f-48}};
    for(int i=0;i<3;i++) DrawLineEx(points[i],points[i+1],2,(Color){72,104,121,255});
    for(int i=0;i<4;i++) {
        Color c=mag_color((MagColor)i,0);
        DrawCircleV(points[i],13,(Color){17,30,42,255});
        DrawCircleLinesV(points[i],13,c); DrawCircleV(points[i],5,c);
    }
    centered("MAGLAVA",x,(int)(h*.40f),86,INK);
    centered("R I S E   O R   B U R N",x,(int)(h*.40f)+92,22,AMBER);
    centered("One tether. Four colors. Nowhere to go but up.",x,(int)(h*.40f)+140,18,DIM);
    Rectangle button={x-180,h*.70f,360,54}; panel(button);
    centered(a->save.unlocked>1?"ENTER  /  CONTINUE CLIMBING":"ENTER  /  BEGIN THE CLIMB",x,(int)button.y+18,20,TEAL);
    centered("W red    S blue    A yellow    D green",x,(int)button.y+75,16,DIM);
    centered("40 handcrafted stages   /   Magnetic momentum   /   An ever-changing soundtrack",x,h-42,14,DIM);
}

Rectangle ui_level_cell(int index) {
    int w=GetScreenWidth(),h=GetScreenHeight();
    float grid=fminf(w-96,1100), cw=(grid-4*12)/5;
    float ch=fminf(100,(h-310)/4.0f), x=(w-grid)/2;
    int local=index%20;
    return (Rectangle){x+(local%5)*(cw+12),132+(local/5)*(ch+12),cw,ch};
}
void ui_select(App *a) {
    menu_background(); int w=GetScreenWidth(),h=GetScreenHeight(); char b[160];
    ui_text("THE ASCENT",48,34,38,INK);
    int total=0;for(int i=0;i<LEVEL_COUNT;i++)total+=a->save.stars[i];
    snprintf(b,sizeof b,"%d / 120 STARS",total);ui_text(b,w-225,48,18,AMBER);
    ui_text("Arrows / WASD to choose   Enter or click to play   PgUp / PgDn to change page",48,89,16,DIM);
    int first=(a->select_cursor/20)*20;
    for(int i=first;i<first+20&&i<LEVEL_COUNT;i++) {
        Rectangle r=ui_level_cell(i); int unlocked=i<a->save.unlocked,sel=i==a->select_cursor;
        panel(r);
        if(sel) DrawRectangleRoundedLinesEx(r,.12f,6,2,TEAL);
        if(!LEVELS[i].legacy_id) DrawRectangle((int)(r.x+r.width)-39,(int)r.y+12,27,3,TEAL);
        Color c=unlocked?INK:DIM;
        ui_text(LEVEL_LABELS[i],r.x+13,r.y+12,22,c);
        int size=14;while(size>10&&ui_text_width(LEVELS[i].name,size)>r.width-24)size--;
        ui_text(LEVELS[i].name,r.x+12,r.y+40,size,c);
        if(unlocked) stars(r.x+r.width/2,r.y+r.height-16,a->save.stars[i],7);
        else centered("LOCKED",r.x+r.width/2,r.y+r.height-24,12,DIM);
    }
    int i=a->select_cursor; const LevelDef *lv=&LEVELS[i];
    centered(lv->name,w/2,h-119,24,INK);
    if(a->save.best_time[i]>0)
        snprintf(b,sizeof b,"BEST %.2fs   /   PAR %.0fs   /   HIGH SCORE %d",a->save.best_time[i],lv->par_time,a->save.best_score[i]);
    else snprintf(b,sizeof b,"PAR %.0fs   /   %s",lv->par_time,lv->legacy_id?"ORIGINAL STAGE":"NEW STAGE");
    centered(b,w/2,h-84,16,TEAL);
    centered(lv->hint,w/2,h-58,14,DIM);
    snprintf(b,sizeof b,"PAGE %d / 2",first/20+1); ui_text(b,w-140,h-29,14,DIM);
    ui_text("ESC back",48,h-29,14,DIM);
}
void ui_pause(App *a) {
    int w=GetScreenWidth(),h=GetScreenHeight();
    DrawRectangle(0,0,w,h,(Color){4,10,17,195});
    panel((Rectangle){w/2-255,h/2-191,510,382});
    centered("TAKE A BREATH",w/2,h/2-157,34,INK);
    centered("The climb will wait.",w/2,h/2-109,18,DIM);
    centered("ESC   Resume",w/2,h/2-51,22,TEAL);
    centered("R   Restart level",w/2,h/2-9,20,INK);
    centered(a->muted?"M   Sound off":"M   Sound on",w/2,h/2+28,18,DIM);
    centered(a->save.reduced_motion?"V   Reduced motion on":"V   Reduced motion off",w/2,h/2+61,18,DIM);
    centered("F11   Fullscreen",w/2,h/2+94,18,DIM);
    centered("Q   Level select",w/2,h/2+139,18,DIM);
}
void ui_complete(App *a) {
    GameSim *g=&a->sim;int w=GetScreenWidth(),h=GetScreenHeight(); char b[96];
    DrawRectangle(0,0,w,h,(Color){4,10,17,205});
    panel((Rectangle){w/2-340,h/2-230,680,460});
    centered(g->level_id==LEVEL_COUNT?"YOU ROSE ABOVE IT ALL":"ASCENT COMPLETE",w/2,h/2-193,36,INK);
    centered(g->lv->name,w/2,h/2-145,20,DIM);
    int shown=(int)(a->complete_t/.28f); if(shown>sim_stars(g))shown=sim_stars(g);
    stars(w/2,h/2-83,shown,24);
    snprintf(b,sizeof b,"%.2fs  /  PAR %.0fs",g->elapsed,g->lv->par_time);
    centered(b,w/2,h/2-26,28,INK);
    snprintf(b,sizeof b,"%d POINTS     %d RETRIES",g->score,g->deaths);
    centered(b,w/2,h/2+18,20,AMBER);
    if(a->new_best) centered("PERSONAL BEST",w/2,h/2+56,16,TEAL);
    centered("1 star: finish   /   2: beat par   /   3: no deaths",w/2,h/2+98,14,DIM);
    if(a->complete_t>1.2f) {
        centered(g->level_id<LEVEL_COUNT?"ENTER  /  NEXT ASCENT":"40 STAGES. ONE UNSTOPPABLE CLIMBER.",w/2,h/2+147,22,TEAL);
        centered("R retry   /   ESC level select",w/2,h/2+188,16,DIM);
    }
}
