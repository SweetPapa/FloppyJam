#include "ui.h"

#include "raylib.h"

static void panel(int width, int height)
{
    DrawRectangle(GetScreenWidth()/2-width/2,GetScreenHeight()/2-height/2,width,height,(Color){255,246,222,238});
    DrawRectangleLines(GetScreenWidth()/2-width/2,GetScreenHeight()/2-height/2,width,height,(Color){109,70,145,255});
}

static void item(const char *text, int y, bool selected)
{
    int width=MeasureText(text,24);
    if(selected) DrawRectangle(GetScreenWidth()/2-width/2-16,y-5,width+32,34,(Color){255,190,103,210});
    DrawText(text,GetScreenWidth()/2-width/2,y,24,selected?DARKPURPLE:DARKGRAY);
}

static void hint(bool controller)
{
    const char *text=controller?"D-PAD / STICK: SELECT    A: CONFIRM    B: BACK":"ARROWS / WASD: SELECT    ENTER / SPACE: CONFIRM    ESC: BACK";
    DrawText(text,GetScreenWidth()/2-MeasureText(text,16)/2,GetScreenHeight()-42,16,(Color){70,61,86,255});
}

void pb_ui_title(int selection, bool controller)
{
    panel(500,390);
    DrawText("POLYBLOOM",GetScreenWidth()/2-151,GetScreenHeight()/2-155,48,(Color){109,70,145,255});
    DrawText("A tiny geometric adventure",GetScreenWidth()/2-132,GetScreenHeight()/2-94,18,DARKGRAY);
    item("PLAY",GetScreenHeight()/2-35,selection==0);
    item("OPTIONS",GetScreenHeight()/2+20,selection==1);
    item("QUIT",GetScreenHeight()/2+75,selection==2);
    hint(controller);
}

void pb_ui_level_select(int selection, const PbSaveData *save, bool controller)
{
    int i;
    panel(760,420);
    DrawText("CHOOSE A BLOOM PATH",GetScreenWidth()/2-177,GetScreenHeight()/2-175,30,DARKPURPLE);
    for(i=0;i<2;++i) {
        int x=GetScreenWidth()/2-330+i*350;
        const char *name=i?"PRISMRUSH CASCADE":"PETALSTEP GARDENS";
        Color c=i?(Color){70,188,222,255}:(Color){105,205,165,255};
        DrawRectangle(x,GetScreenHeight()/2-105,310,220,selection==i?c:Fade(c,.55f));
        DrawText(name,x+155-MeasureText(name,21)/2,GetScreenHeight()/2-76,21,DARKPURPLE);
        DrawText(TextFormat("BEST  %s",save->best_time_ms[i]?TextFormat("%.2f",save->best_time_ms[i]/1000.0f):"--"),x+24,GetScreenHeight()/2-18,18,DARKGRAY);
        DrawText(TextFormat("GLINTS  %d/30",save->best_glints[i]),x+24,GetScreenHeight()/2+18,18,DARKGRAY);
        DrawText(TextFormat("SEEDS   %d/3",(save->seed_masks[i]&1)+((save->seed_masks[i]>>1)&1)+((save->seed_masks[i]>>2)&1)),x+24,GetScreenHeight()/2+54,18,DARKGRAY);
        if(save->flags&(1u<<i)) DrawText("COMPLETE",x+188,GetScreenHeight()/2+82,16,DARKPURPLE);
    }
    hint(controller);
}

void pb_ui_intro(const PbLevel *level, float timer)
{
    const char *name=level->level_id==PB_LEVEL_CASCADE?"PRISMRUSH CASCADE":"PETALSTEP GARDENS";
    float fade=timer<.3f?timer/.3f:timer>1.2f?(1.5f-timer)/.3f:1;
    DrawRectangle(0,GetScreenHeight()/2-48,GetScreenWidth(),96,Fade(BLACK,.45f*fade));
    DrawText(name,GetScreenWidth()/2-MeasureText(name,32)/2,GetScreenHeight()/2-18,32,Fade(WHITE,fade));
}

void pb_ui_pause(int selection, bool controller)
{
    panel(430,390); DrawText("PAUSED",GetScreenWidth()/2-69,GetScreenHeight()/2-160,34,DARKPURPLE);
    item("RESUME",GetScreenHeight()/2-82,selection==0);
    item("RESTART LEVEL",GetScreenHeight()/2-28,selection==1);
    item("OPTIONS",GetScreenHeight()/2+26,selection==2);
    item("LEVEL SELECT",GetScreenHeight()/2+80,selection==3);
    hint(controller);
}

void pb_ui_options(int selection, const PbSaveData *s, bool controller)
{
    panel(570,560); DrawText("OPTIONS",GetScreenWidth()/2-76,GetScreenHeight()/2-245,34,DARKPURPLE);
    item(TextFormat("MASTER VOLUME       %3d",s->volume_master),GetScreenHeight()/2-185,selection==0);
    item(TextFormat("MUSIC VOLUME        %3d",s->volume_music),GetScreenHeight()/2-137,selection==1);
    item(TextFormat("SOUND VOLUME        %3d",s->volume_sfx),GetScreenHeight()/2-89,selection==2);
    item(TextFormat("CAMERA SENSITIVITY  %3d",s->camera_sensitivity),GetScreenHeight()/2-41,selection==3);
    item(TextFormat("INVERT CAMERA       %s",s->option_flags&1?"YES":"NO"),GetScreenHeight()/2+7,selection==4);
    item(TextFormat("REDUCED EFFECTS     %s",s->option_flags&2?"YES":"NO"),GetScreenHeight()/2+55,selection==5);
    item(TextFormat("SCREEN MODE         %s",s->option_flags&4?"BORDERLESS":"WINDOWED"),GetScreenHeight()/2+103,selection==6);
    item("BACK",GetScreenHeight()/2+151,selection==7);
    hint(controller);
}

void pb_ui_results(const PbLevel *l, const PbGameplay *g, int selection, bool controller)
{
    const char *name=l->level_id==PB_LEVEL_CASCADE?"PRISMRUSH CASCADE":"PETALSTEP GARDENS";
    panel(620,520);
    DrawText("LEVEL COMPLETE",GetScreenWidth()/2-135,GetScreenHeight()/2-225,34,DARKPURPLE);
    DrawText(name,GetScreenWidth()/2-MeasureText(name,22)/2,GetScreenHeight()/2-172,22,DARKGRAY);
    DrawText(TextFormat("TIME       %.2f",pb_gameplay_result_ms(g)/1000.0f),GetScreenWidth()/2-120,GetScreenHeight()/2-116,22,DARKGRAY);
    DrawText(TextFormat("GLINTS     %d / 30",l->glint_count),GetScreenWidth()/2-120,GetScreenHeight()/2-82,22,DARKGRAY);
    DrawText(TextFormat("SEEDS      %d / 3",l->seed_count),GetScreenWidth()/2-120,GetScreenHeight()/2-48,22,DARKGRAY);
    DrawText(TextFormat("FALLS      %d",g->falls),GetScreenWidth()/2-120,GetScreenHeight()/2-14,22,DARKGRAY);
    if(l->glint_count==30&&l->seed_count==3) DrawText("FULL BLOOM",GetScreenWidth()/2+70,GetScreenHeight()/2-95,18,GOLD);
    if(pb_gameplay_flow_run(g)) DrawText("FLOW RUN",GetScreenWidth()/2+78,GetScreenHeight()/2-60,18,(Color){40,160,135,255});
    item("RETRY",GetScreenHeight()/2+56,selection==0);
    item("OTHER LEVEL",GetScreenHeight()/2+104,selection==1);
    item("LEVEL SELECT",GetScreenHeight()/2+152,selection==2);
    hint(controller);
}
