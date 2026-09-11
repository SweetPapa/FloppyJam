/* Exercise the interpreters, including their runtime capacity limits, not just
 * the authored text. Puzzle fixtures solve the mini-games; this verifies that
 * real conversations, gates, deductions and rewards form a complete campaign. */
#include "content/content.h"
#include "investigation/investigation.h"
#include "board/board.h"
#include "dlg/dlg.h"
#include "cut/cut.h"
#include "scene/scene.h"
#include "puzzle/puzzle.h"
#include "flags/flags.h"
#include "save/save.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures,checks,tone,puzzles;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)
static void conversation(const char *node);
static void deduce(const char *id) {
    CHECK(board_start(id));
    Block b;CHECK(content_block("board",id,&b));Cursor cur;cur_open(&cur,&b);
    char line[TEXT_MAX];int blanks=0,pools=0,lines=0;
    while(cur_line(&cur,line,sizeof line)) {
        const char *p=line;char kw[32];word(&p,kw,sizeof kw);
        if(eq(kw,"pool"))pools++;
        if(eq(kw,"text"))lines++;
        if(!eq(kw,"blank"))continue;
        char name[24],answer[48];word(&p,name,sizeof name);word(&p,kw,sizeof kw);word(&p,answer,sizeof answer);
        CHECK(board_place(name,answer));blanks++;
    }
    CHECK(board_blank_count()==blanks);CHECK(board_pool_count()==pools);CHECK(board_line_count()==lines);
    int before=flag_get("boards_solved");
    CHECK(board_deduce()==blanks);CHECK(board_is_solved(id));
    CHECK(flag_get("boards_solved")==before+1);
    CHECK(board_deduce()==-1);CHECK(flag_get("boards_solved")==before+1);
    char kind[24],target[48];snprintf(kind,sizeof kind,"%s",board_solved_kind());snprintf(target,sizeof target,"%s",board_solved_id());
    if(eq(kind,"cut")){CHECK(cut_play(target));cut_skip();}
    else if(eq(kind,"goto"))conversation(target);
    CHECK(board_start(id));CHECK(board_deduce()==-1); /* revisiting cannot repeat a reward or a bloom */
}
static void conversation(const char *node) {
    CHECK(dlg_start(node));int steps;
    for(steps=0;steps<300;steps++) {
        dlg_status state=dlg_update(0);
        if(state==DLG_DONE)break;
        if(state==DLG_WANT_PUZZLE) {
            const pz_def *d=puzzle_find(dlg_request());CHECK(d!=NULL);if(!d)break;
            pz_ctx ctx={0};ctx.seed=1234u+(unsigned)tone;ctx.difficulty=palette_stage()>=4?2:palette_stage()>=2?1:0;
            ctx.area=(Rectangle){170,168,940,392};CHECK(d->solve_replay(&ctx));puzzles++;
            if(d->clue_granted)clue_grant(d->clue_granted);
            char key[48];snprintf(key,sizeof key,"pzdone.%s",d->id);flag_set(key,1);dlg_resume();
        }else if(state==DLG_WANT_CUT){CHECK(cut_play(dlg_request()));cut_skip();dlg_resume();}
        else if(state==DLG_WANT_BOARD){char id[48];snprintf(id,sizeof id,"%s",dlg_request());deduce(id);dlg_resume();}
        else if(dlg_choice_count())CHECK(dlg_choose(tone%dlg_choice_count()));
        else dlg_advance_beat();
    }
    CHECK(steps<300);
}
static void visit(const char *id) {
    CHECK(scene_load(id));
    for(int i=0;i<2;i++) {
        scene_request req=scene_update(0);
        if(req==SC_TALK)conversation(scene_request_id());
        else if(req==SC_CUT){CHECK(cut_play(scene_request_id()));cut_skip();}
    }
    Block b;CHECK(content_block("scene",id,&b));Cursor cur;cur_open(&cur,&b);char line[TEXT_MAX];
    while(cur_line(&cur,line,sizeof line)) {
        const char *p=line;char kw[32];word(&p,kw,sizeof kw);if(!eq(kw,"hot"))continue;
        for(int i=0;i<4;i++)word(&p,kw,sizeof kw);
        word(&p,kw,sizeof kw);if(!eq(kw,"talk"))continue;
        char node[48],label[TEXT_MAX];word(&p,node,sizeof node);word(&p,label,sizeof label);
        if(word(&p,kw,sizeof kw)&&eq(kw,"if")&&!content_cond(p))continue;
        conversation(node);
    }
}
static void campaign(void) {
    flags_reset();palette_set_stage(0);flag_set("save_seed",1234);flag_set("feathers",2);
    CHECK(investigation_chapter()==0);CHECK(!town_unlocked("ch1_dock"));
    for(int chapter=0;chapter<6;chapter++) {
        char id[48];snprintf(id,sizeof id,"%s",investigation_board());
        CHECK(!investigation_ready(id));
        for(int pass=0;pass<4&&!investigation_ready(id);pass++) {
            for(int p=0;p<town_place_count();p++)if(town_unlocked(town_place(p)->id))visit(town_place(p)->id);
        }
        if(!investigation_ready(id)) {
            char req[32][48];int n=investigation_requirements(id,req,32);
            for(int i=0;i<n;i++)if(!clue_has(req[i]))fprintf(stderr,"MISSING tone %d %s: %s\n",tone,id,req[i]);
            CHECK(investigation_ready(id));return;
        }
        deduce(id);CHECK(investigation_chapter()==chapter+1);
        printf("  tone %d: %s complete\n",tone,id);
    }
    CHECK(town_unlocked("f_lantern"));visit("f_lantern");CHECK(palette_stage()==6);
    for(int i=0;i<small_case_count();i++) {
        CHECK(!small_case_answer(i,small_case(i)->correct));
        CHECK(small_case_observe(i,0));CHECK(!small_case_observe(i,0));
        CHECK(!small_case_answer(i,small_case(i)->correct));CHECK(small_case_observe(i,1));
        int before=flag_get("feathers"),tr=trust_get(small_case(i)->npc);
        CHECK(!small_case_answer(i,(small_case(i)->correct+1)%3));CHECK(flag_get("feathers")==before);
        CHECK(small_case_answer(i,small_case(i)->correct));CHECK(flag_get("feathers")==before+2);
        CHECK(trust_get(small_case(i)->npc)==tr+1);
        CHECK(!small_case_answer(i,small_case(i)->correct));CHECK(flag_get("feathers")==before+2);
    }
    CHECK(small_case_completed()==6);
}
static void saves(void) {
    save_allow_writes(true);flags_reset();palette_set_stage(0);
    clue_grant("clue.town_gray");clue_grant("clue.prism_missing");flag_set("draft.p_morning.p1",1);flag_set("phint.rope_knot",2);
    CHECK(save_write(2,"p_square","Testing safe saves"));flags_reset();CHECK(save_read(2));
    CHECK(eq(clue_at(0),"clue.town_gray"));CHECK(eq(clue_at(1),"clue.prism_missing"));
    CHECK(flag_get("draft.p_morning.p1")==1);CHECK(flag_get("phint.rope_knot")==2);
    FILE *f=fopen("huedunit2.sav","wb");CHECK(f!=NULL);if(f){fputs("HUEDUNIT 2\nscene p_square\nstage 0\nf feathers 99\n",f);fclose(f);}
    CHECK(!save_read(2));CHECK(clue_count()==2); /* truncated save cannot clear the current case */
    f=fopen("huedunit2.sav","wb");if(f){fputs("HUEDUNIT 1\nscene ch1_dock\nstage 0\nf board.p_morning 1\nf future.flag 42\n",f);fclose(f);}
    CHECK(save_read(2));CHECK(investigation_chapter()==1);CHECK(flag_get("future.flag")==42);
    snprintf(settings()->detective,sizeof settings()->detective,"Ada Finch");settings_store();settings_defaults();settings_load();CHECK(eq(settings()->detective,"Ada Finch"));
    f=fopen("huedunit.cfg","wb");if(f){fputs("text -3\nmusic 123\nsfx -2\n",f);fclose(f);}settings_load();CHECK(settings()->text_size==0);CHECK(settings()->music_vol==10);CHECK(settings()->sfx_vol==0);
    save_allow_writes(false);CHECK(save_write(2,"p_gate","must not overwrite"));CHECK(save_read(2));CHECK(eq(save_last_scene(),"ch1_dock"));
}
int main(void) {
    content_init();settings_defaults();save_allow_writes(false);
    for(tone=0;tone<3;tone++)campaign();
    saves();
    printf("test_campaign: %d checks, %d puzzle completions, %d failures\n",checks,puzzles,failures);
    return failures?1:0;
}
