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

static const char *level_name(int id)
{
    if(id==PB_LEVEL_CASCADE) return "PRISMRUSH CASCADE";
#if POLYBLOOM_INCLUDE_LEVEL3
    if(id==PB_LEVEL_FOUNDRY) return "AURORA FOUNDRY";
#endif
#if POLYBLOOM_INCLUDE_LEVEL4
    if(id==PB_LEVEL_CROWN) return "SOVEREIGN CROWN";
#endif
    return "PETALSTEP GARDENS";
}

static Color level_color(int id)
{
    if(id==PB_LEVEL_CASCADE) return (Color){70,188,222,255};
#if POLYBLOOM_INCLUDE_LEVEL3
    if(id==PB_LEVEL_FOUNDRY) return (Color){241,137,75,255};
#endif
#if POLYBLOOM_INCLUDE_LEVEL4
    if(id==PB_LEVEL_CROWN) return (Color){166,134,226,255};
#endif
    return (Color){105,205,165,255};
}

void pb_ui_hud(const PbLevel *l, const PbGameplay *g, bool controller)
{
    int i;
    Color ink=(Color){74,47,101,255};
    DrawRectangle(22,20,350,82,(Color){255,246,222,220});
    DrawRectangle(22,20,6,82,level_color(l->level_id));
    DrawText(pb_level_section_name(l),40,29,16,ink);
    DrawPoly((Vector2){48,73},4,10,45,(Color){112,222,214,255});
    DrawText(TextFormat("%02d / 30",l->glint_count),66,63,20,ink);
    for(i=0;i<3;++i)
        DrawPoly((Vector2){178+i*26,73},5,9,-90,i<l->seed_count?(Color){255,190,70,255}:Fade(ink,.25f));
    DrawText(TextFormat("%.2f",pb_gameplay_result_ms(g)/1000.0f),282,63,18,ink);
    DrawRectangle(GetScreenWidth()-164,20,142,50,(Color){255,246,222,220});
    for(i=0;i<3;++i)
        DrawPoly((Vector2){GetScreenWidth()-135+i*38,45},5,13,-90,
                 i<g->health?(Color){247,103,137,255}:(Color){133,112,145,100});
    if(g->glint_chain>1&&g->glint_chain_timer>0) {
        float pulse=1+g->pickup_flash*.18f;
        const char *chain=TextFormat("%d GLINT FLOW!",g->glint_chain);
        int size=(int)(22*pulse);
        DrawText(chain,GetScreenWidth()/2-MeasureText(chain,size)/2,112,size,
                 (Color){255,209,72,255});
    }
#if POLYBLOOM_INCLUDE_LEVEL4
    if(l->level_id==PB_LEVEL_CROWN&&g->boss_active) {
        int x=GetScreenWidth()/2-170;
        DrawRectangle(x,26,340,34,(Color){47,28,73,220});
        DrawRectangle(x+5,50,330*g->boss_health/5,5,(Color){255,104,185,255});
        DrawText("PRISM SOVEREIGN",GetScreenWidth()/2-MeasureText("PRISM SOVEREIGN",17)/2,31,17,WHITE);
    }
#endif
    if(g->run_time<8&&l->section==PB_SECTION_AWAKENING) {
        const char *tip=controller?"LEFT STICK / D-PAD MOVE   A JUMP   X BURST":
                                   "WASD / ARROWS MOVE   AUTO CAMERA   SPACE JUMP   SHIFT BURST";
        int width=MeasureText(tip,16);
        DrawRectangle(GetScreenWidth()/2-width/2-14,GetScreenHeight()-58,width+28,36,(Color){255,246,222,210});
        DrawText(tip,GetScreenWidth()/2-width/2,GetScreenHeight()-48,16,(Color){75,57,102,230});
    }
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
    panel(800,PB_AVAILABLE_LEVELS>2?590:420);
    DrawText("CHOOSE A BLOOM PATH",GetScreenWidth()/2-177,
             GetScreenHeight()/2-(PB_AVAILABLE_LEVELS>2?265:175),30,DARKPURPLE);
    for(i=0;i<PB_AVAILABLE_LEVELS;++i) {
        int id=pb_level_id_for_slot(i);
        const char *name=level_name(id);
        Color color=level_color(id);
        int x=GetScreenWidth()/2-365+(i%2)*375;
        int y=GetScreenHeight()/2-(PB_AVAILABLE_LEVELS>2?210:105)+(i/2)*235;
        DrawRectangle(x,y,345,205,selection==i?color:Fade(color,.55f));
        DrawText(name,x+172-MeasureText(name,20)/2,y+22,20,DARKPURPLE);
        DrawText(TextFormat("BEST  %s",save->best_time_ms[id]?TextFormat("%.2f",save->best_time_ms[id]/1000.0f):"--"),x+22,y+70,17,DARKGRAY);
        DrawText(TextFormat("GLINTS %d/30   SEEDS %d/3",save->best_glints[id],
                 (save->seed_masks[id]&1)+((save->seed_masks[id]>>1)&1)+((save->seed_masks[id]>>2)&1)),x+22,y+104,17,DARKGRAY);
        if(save->flags&(1u<<id)) DrawText("COMPLETE",x+225,y+153,16,DARKPURPLE);
    }
    hint(controller);
}

void pb_ui_intro(const PbLevel *level, float timer)
{
    const char *name=level_name(level->level_id);
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
    panel(570,620); DrawText("OPTIONS",GetScreenWidth()/2-76,GetScreenHeight()/2-275,34,DARKPURPLE);
    item(TextFormat("MASTER VOLUME       %3d",s->volume_master),GetScreenHeight()/2-215,selection==0);
    item(TextFormat("MUSIC VOLUME        %3d",s->volume_music),GetScreenHeight()/2-171,selection==1);
    item(TextFormat("SOUND VOLUME        %3d",s->volume_sfx),GetScreenHeight()/2-127,selection==2);
    item(TextFormat("CAMERA SENSITIVITY  %3d",s->camera_sensitivity),GetScreenHeight()/2-83,selection==3);
    item(TextFormat("INVERT CAMERA       %s",s->option_flags&1?"YES":"NO"),GetScreenHeight()/2-39,selection==4);
    item(TextFormat("REDUCED EFFECTS     %s",s->option_flags&2?"YES":"NO"),GetScreenHeight()/2+5,selection==5);
    item(TextFormat("SCREEN MODE         %s",s->option_flags&4?"BORDERLESS":"WINDOWED"),GetScreenHeight()/2+49,selection==6);
    item(TextFormat("FLOW AURA           %s",s->option_flags&8?"NO":"YES"),GetScreenHeight()/2+93,selection==7);
    item("BACK",GetScreenHeight()/2+137,selection==8);
    hint(controller);
}

void pb_ui_results(const PbLevel *l, const PbGameplay *g, int selection, bool controller)
{
    const char *name=level_name(l->level_id);
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
