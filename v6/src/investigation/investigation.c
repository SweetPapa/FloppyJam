#include "investigation.h"
#include "board/board.h"
#include "content/content.h"
#include "flags/flags.h"
#include "puzzle/puzzle.h"
#include "scene/scene.h"
#include "save/save.h"
#include "audio/synth.h"
#include <stdio.h>
#include <string.h>

static const char *boards[] = {"p_morning", "ch1_midnight", "ch2_opened", "ch3_prism", "ch4_festival", "ch5_where"};
static const char *questions[] = {
    "What happened this morning?", "Who was at the tower at midnight?",
    "How was the tower opened?", "What was wrong with the Prism?",
    "Why did the festival stop?", "Where is Iris now?",
    "Go up the mended stair. Iris is waiting."
};
int investigation_chapter(void) {
    for (int i=0;i<6;i++) if (!board_is_solved(boards[i])) return i;
    return 6;
}
const char *investigation_board(void) { int c=investigation_chapter(); return c<6 ? boards[c] : ""; }
const char *investigation_question(void) { return palette_stage()==6 ? "Prismbrook is yours to explore." : questions[investigation_chapter()]; }
int investigation_requirements(const char *id, char clues[][48], int cap) {
    Block b; if (!content_block("board", id, &b)) return 0;
    Cursor c; cur_open(&c,&b); char line[TEXT_MAX]; int n=0;
    while (cur_line(&c,line,sizeof line)) {
        const char *p=line; char kw[32]; word(&p,kw,sizeof kw);
        if (eq(kw,"requires")) while (n<cap && word(&p,clues[n],48)) n++;
    }
    return n;
}
bool investigation_ready(const char *id) {
    char clues[32][48]; int n=investigation_requirements(id,clues,32);
    if (!n) return false;
    for(int i=0;i<n;i++) if(!clue_has(clues[i])) return false;
    return true;
}
void investigation_summary(char *out,int cap) {
    char clues[32][48]; int n=investigation_requirements(investigation_board(),clues,32),have=0;
    for(int i=0;i<n;i++) have+=clue_has(clues[i]);
    if(n) snprintf(out,(size_t)cap,"%s   %s %d/%d",investigation_question(),have==n?"Ready to deduce!":"Evidence",have,n);
    else snprintf(out,(size_t)cap,"%s",investigation_question());
}
static const TownPlace places[] = {
    {"p_gate","Town road","",160,520}, {"p_square","Town square","",370,356},
    {"tower","Prismworks stair","",760,190},
    {"ch1_dock","Harbour dock","board.p_morning",168,270},
    {"ch1_quay","Ferry quay","board.p_morning",136,150},
    {"ch2_row","Market Row","board.ch1_midnight",408,178},
    {"ch2_post","Post office","board.ch1_midnight",420,78},
    {"ch3_lane","Cogg's Lane","board.ch2_opened",615,426},
    {"ch3_library","Library","board.ch2_opened",880,520},
    {"ch4_garden","Gardens","board.ch3_prism",430,545},
    {"ch4_hives","Bee meadow","board.ch3_prism",630,624},
    {"ch5_green","Tower Green","board.ch4_festival",775,335},
    {"ch5_lodge","Nona's lodge","board.ch4_festival",880,420},
    {"f_lantern","Lantern room","board.ch5_where",930,100}
};
int town_place_count(void) {return (int)(sizeof places/sizeof *places);}
const TownPlace *town_place(int i) {return i>=0 && i<town_place_count()?&places[i]:NULL;}
bool town_unlocked(const char *id) {
    for(int i=0;i<town_place_count();i++) if(eq(id,places[i].id)) return !places[i].gate[0]||flag_get(places[i].gate);
    return false;
}

/* Optional, self-contained deductions. Objects are physical scene hotspots;
 * discoveries and rewards use the same durable flag store as the main case. */
static const SmallCase cases[] = {
 {"The midnight bun", "Who keeps leaving a bun on the empty bench?", "bruno", "A bread basket for the square",
  "Bruno admits the bench belonged to his first customer. You suggest leaving two buns, and sitting down. By lunch, the bench has a basket and company.",
  {{"p_gate","A folded paper bag","The bag is folded into a little boat. Flour on the crease; no postage, no name. Whoever brought it walked from the square.",D_BREAD,520,330},
   {"p_square","A baker's delivery slate","Bruno's slate lists every delivery, including one that says: ROAD BENCH. ONE BUN. NO CHARGE. The entry has forty little ticks beside it.",D_BOOK,710,350}},
  {"The mayor is paying for votes.","Bruno is keeping an old kindness going.","Pip has learned to bake."},1,D_BREAD},
 {"A boat called Tomorrow", "Why has the ferry's name been painted over?", "maribel", "A little boat mobile",
  "Maribel meant to rename the ferry after her daughter, then worried it would seem sentimental. Tansy paints the name herself. The captain calls the lettering adequate. She does not stop smiling.",
  {{"ch1_dock","A child's boat sketch","Tansy's drawing shows the ferry with TANSY on its side. Underneath: CAPTAIN SAYS MAYBE TOMORROW. The paper is soft from being unfolded.",D_ENVELOPE,660,280},
   {"ch1_quay","A freshly painted nameplate","Under TOMORROW, wet paint reveals the letters TAN. Maribel's brush is clean; her handprint is still in the varnish.",D_ANCHOR,870,285}},
  {"Maribel wants to surprise Tansy.","The ferry is being sold.","The name is a secret timetable."},0,D_ANCHOR},
 {"The undeliverable invitation", "Why does this envelope keep coming back?", "felix", "Paper bunting for the square",
  "Greta addressed an invitation to EVERYONE WHO THINKS THEY ARE NOT INVITED. Felix could not find that house. You pin it beside the fountain. People keep stopping to read it twice.",
  {{"ch2_row","An invitation offcut","Greta's scrap reads: THERE IS ROOM AT MY TABLE. A tiny spool is drawn in the corner. No date, no price, and definitely no guest list.",D_SPOOL,615,300},
   {"ch2_post","A much-stamped envelope","The envelope says EVERYONE WHO THINKS THEY ARE NOT INVITED. Felix has written ADDRESS INCOMPLETE, then crossed out INCOMPLETE and written DIFFICULT.",D_ENVELOPE,850,275}},
  {"Felix has lost his spectacles.","Greta forgot where she lives.","It belongs on the public noticeboard."},2,D_ENVELOPE},
 {"The clock that waits", "Why does one clock stop at four?", "bartleby", "A repaired reading clock",
  "The clock is Bartleby's tea reminder. Its missing winding key has been holding his place in a book. Edwina winds it; Bartleby brings a second cup. Four o'clock becomes an appointment again.",
  {{"ch3_lane","A clock with a note","One clock has stopped at four. Its repair tag reads: NO FAULT FOUND. OWNER REQUESTS COMPANY. There is a round tea stain under the word COMPANY.",D_CLOCK,650,285},
   {"ch3_library","An unusual bookmark","A little winding key lies in a book called TEA FOR TWO. The borrower's name is Bartleby Shelf. The only dog-eared page is about making a habit of asking.",D_KEY,860,275}},
  {"A missing key and a missed tea appointment.","All the town's clocks have lost an hour.","The clock is warning of a storm."},0,D_CLOCK},
 {"The flowers that moved", "What is carrying pollen into the empty bed?", "sage", "A pollinator garden",
  "The bees have found Sage's forgotten night flowers. You and Mo mark a path between the beds and leave a shallow drinking bowl. By evening, the supposedly empty patch is full of visitors.",
  {{"ch4_garden","A dusting of pollen","Pollen dusts an empty-looking bed. The stems have tightly folded flowers, and each leaf points away from the afternoon sun.",D_FLOWER,650,285},
   {"ch4_hives","Mo's evening field notes","Mo's notes: WORKERS RETURN LATE, DUSTED WITH PALE POLLEN. FOLLOWED THEM TO THE NIGHT-BLOOMING PATCH. REMIND SAGE THAT SLEEPING IS NOT DEAD.",D_BEE,850,285}},
  {"Someone is painting the leaves.","The flowers bloom after Sage goes home.","The wind carries it from the harbour."},1,D_FLOWER},
 {"One lamp left burning", "Why is there a lamp with no number?", "nona", "A welcome lantern",
  "Nona kept one lamp for anyone coming home late. Wick kept oil beside it. Neither knew the other was still doing it. You hang the lamp in the square, where nobody has to ask whose turn it is.",
  {{"ch5_green","An unnumbered lantern","Every stored lantern has a number except this one. Its handle is polished by use. A label reads: FOR THE LAST PERSON HOME.",D_LANTERN,650,260},
   {"ch5_lodge","A replenished oil tin","A fresh tin sits beside Nona's door. The receipt is signed WICK. In the margin: IF SHE KEEPS A LIGHT, I CAN KEEP THE OIL.",D_TEACUP,850,280}},
  {"It was left out of the inventory by mistake.","Nona is testing a new kind of oil.","Nona and Wick are keeping a welcome alive."},2,D_LANTERN}
};
int small_case_count(void) {return (int)(sizeof cases/sizeof *cases);}
const SmallCase *small_case(int i) {return i>=0&&i<small_case_count()?&cases[i]:NULL;}
static void obs_key(char *key,int i,int o) {snprintf(key,48,"little.%d.observation.%d",i,o);}
bool small_case_seen(int i,int o) {char key[48];obs_key(key,i,o);return flag_get(key)!=0;}
bool small_case_solved(int i) {char key[48];snprintf(key,sizeof key,"little.%d.solved",i);return flag_get(key)!=0;}
bool small_case_observe(int i,int o) {
    if(!small_case(i)||o<0||o>1||!town_unlocked(cases[i].obs[o].scene)||small_case_seen(i,o))return false;
    char key[48];obs_key(key,i,o);flag_set(key,1);return true;
}
bool small_case_answer(int i,int answer) {
    const SmallCase *c=small_case(i);
    if(!c||answer!=c->correct||small_case_solved(i)||!small_case_seen(i,0)||!small_case_seen(i,1))return false;
    char key[48];snprintf(key,sizeof key,"little.%d.solved",i);flag_set(key,1);
    flag_add("feathers",2);trust_add(c->npc,1);return true;
}
int small_case_completed(void){int n=0;for(int i=0;i<small_case_count();i++)n+=small_case_solved(i);return n;}

typedef struct {const char *scene,*prompt,*clues;} Source;
static const Source sources[] = {
 {"p_gate","Ask the mayor about the missing Prism","clue.town_gray clue.prism_missing"},
 {"p_square","Talk to Bruno about the magpie","clue.magpie_blamed"},
 {"p_square","Ask Wick about Iris and the locked door","clue.iris_missing clue.door_locked"},
 {"ch1_dock","Hear Otto's story and help with his scales","clue.otto_alibi clue.fish_pail clue.midnight_figure clue.pip_hungry"},
 {"ch1_dock","Ask Tansy what she saw; play her shell game","clue.tansy_saw"},
 {"ch1_quay","Help Maribel tie up the ferry; check her log","clue.ferry_log"},
 {"ch2_row","Help Bruno with the order book","clue.bruno_order"},
 {"ch2_row","Ask Greta about the door and the shiny thefts","clue.no_forced_lock clue.greta_gossip clue.shiny_thefts"},
 {"ch2_post","Help Felix sort the post; ask about Iris's letter","clue.felix_letters clue.iris_letter"},
 {"ch3_lane","Help Edwina and ask about the repair order","clue.cogg_ledger clue.repair_braces"},
 {"ch3_lane","Ask Petra who borrowed her picks","clue.picks_borrowed clue.iris_secret"},
 {"ch3_library","Read with Bartleby; ask what powers the Prism","clue.prism_dimming clue.festival_lapsed"},
 {"ch4_garden","Water with Sage; ask about past festivals","clue.garden_gray clue.three_years"},
 {"ch4_garden","Help Poppy; ask why the town stopped celebrating","clue.town_lowspirits clue.mayor_dodges"},
 {"ch4_hives","Follow Mo's bees and ask about the lanterns","clue.bees_remember clue.lanterns_stored"},
 {"ch5_green","Ask the mayor about the council's decision","clue.budget_cut clue.mayor_shame"},
 {"ch5_green","Help Wick examine the stair and spare key","clue.stair_rotten clue.spare_key"},
 {"ch5_lodge","Listen to Nona and help with her lamps","clue.nona_key clue.nona_grief"},
 {"ch5_green","Return to Wick; wait for Pip on the low wall","clue.pip_trusts"}
};
static int g_tab,g_case;static char g_dest[48];static bool g_wrong;
void casebook_open(int tab){g_tab=tab;g_case=0;g_wrong=false;g_dest[0]=0;sfx_play(SFX_PAGE);}
const char *casebook_destination(void){return g_dest;}
static Rectangle tab_rect(int i){return (Rectangle){74+i*245.0f,44,228,44};}
static Rectangle close_rect(void){return (Rectangle){1030,44,175,44};}
static Rectangle case_rect(int i){return (Rectangle){82,142+i*76.0f,290,64};}
static Rectangle map_rect(int i){const TownPlace *p=&places[i];return (Rectangle){70+p->x*.77f-74,124+p->y*.77f-24,156,57};}
static int leads(int *out) {
    char req[32][48];int n=investigation_requirements(investigation_board(),req,32),count=0;
    for(int i=0;i<(int)(sizeof sources/sizeof *sources);i++) {
        bool needed=false;
        for(int j=0;j<n;j++) if(!clue_has(req[j]) && strstr(sources[i].clues,req[j]))needed=true;
        if(needed)out[count++]=i;
    }
    return count;
}
static void line_text(const char *s,float x,float y,float w,float h,float size,Color c){art_text_fit(s,(Rectangle){x,y,w,h},size,c);}
void casebook_draw(void) {
    DrawRectangle(0,0,VW,VH,(Color){25,28,32,195});
    paper_panel((Rectangle){50,28,1180,655},4,19000);
    const char *tabs[]={"Case leads","Town map","Little mysteries"};
    for(int i=0;i<3;i++){pz_button(tab_rect(i),tabs[i],true,19100+i*2);if(g_tab==i)ink_line(86+i*245,93,285+i*245,93,3,0,19120+i,col_ink());}
    pz_button(close_rect(),"Back to town",true,19130);
    if(g_tab==0) {
        char summary[220];investigation_summary(summary,sizeof summary);
        line_text(summary,86,117,1100,58,25,col_ink());
        line_text("Follow a lead in any order. You draw the conclusions; your notebook remembers the errands.",86,181,1100,40,18,col_ink_soft());
        int list[24],n=leads(list);
        for(int i=0;i<n&&i<6;i++) {
            Rectangle r={88,234+i*57.0f,1100,48};
            pz_button(r,sources[list[i]].prompt,town_unlocked(sources[list[i]].scene),19200+i*2);
        }
        if(!n) {
            doodle(D_PRISM,250,350,72,0,col_accent_a());
            line_text(investigation_chapter()<6?"You have enough evidence. Turn your notes into a theory.":palette_stage()<6?"The mystery has a person at its heart. Go and bring her home.":"The big case is closed. Explore, finish little mysteries, or replay your favourite puzzles in the journal.",420,270,690,150,25,col_ink());
            pz_button((Rectangle){430,452,610,58},investigation_chapter()<6?"Open the deduction board":palette_stage()<6?"Go to the lantern room":"Visit the square",true,19250);
        }
        line_text("M: map and leads     J: people, evidence and puzzle replays",88,630,1000,24,15,col_ink_soft());
    } else if(g_tab==1) {
        /* Draw the routes first so labels remain legible. */
        for(int i=0;i<town_place_count();i++) {
            int parent=i==0?1:i==4?3:i==6?5:i==8?7:i==10?9:i==12?11:i==13?2:1;
            if(i==1)continue;
            Rectangle a=map_rect(i),b=map_rect(parent);
            ink_line(a.x+78,a.y+26,b.x+78,b.y+26,2,2,19300+i,col_paper_dark());
        }
        doodle(D_WAVE,100,475,38,0,col_sky());doodle(D_TREE,582,594,29,0,col_cool());
        for(int i=0;i<town_place_count();i++) {
            bool open=town_unlocked(places[i].id);Rectangle r=map_rect(i);
            pz_button(r,places[i].name,open,19400+i*2);
            if(eq(scene_current(),places[i].id))doodle(D_FEATHER,r.x+78,r.y-14,12,0,col_accent_b());
        }
        line_text("PRISMBROOK",928,157,265,48,27,col_ink());
        line_text("A town worth getting to know.",928,214,250,66,20,col_ink_soft());
        line_text("Click any open place to travel. Solve the main case to open new neighbourhoods.",928,310,250,145,19,col_ink());
        char s[100];snprintf(s,sizeof s,"%d / 6 little mysteries solved",small_case_completed());
        line_text(s,928,491,250,55,18,col_ink());
        line_text("Your square fills with keepsakes as you help people.",928,560,245,75,17,col_ink_soft());
    } else {
        for(int i=0;i<small_case_count();i++) {
            bool open=town_unlocked(cases[i].obs[0].scene);
            char title[96];snprintf(title,sizeof title,"%s%s",small_case_solved(i)?"Solved: ":"",open?cases[i].title:"A neighbourhood to discover");
            pz_button(case_rect(i),title,open,19500+i*2);
            if(g_case==i)ink_line(380,146+i*76,380,198+i*76,3,0,19550+i,col_ink());
        }
        const SmallCase *c=&cases[g_case];
        line_text(c->title,414,119,760,40,28,col_ink());
        line_text(c->question,414,166,760,46,21,col_ink_soft());
        for(int o=0;o<2;o++) {
            Rectangle r={416,228+o*113.0f,750,102};paper_panel(r,2,19600+o);
            bool seen=small_case_seen(g_case,o);
            doodle(seen?c->obs[o].doodle:D_BOOK,444,r.y+34,18,0,col_ink_soft());
            line_text(seen?c->obs[o].text:"An observation is waiting here. Click to visit, then look for the object marked with a magnifying circle.",480,r.y+12,667,78,18,col_ink());
            if(!seen) {const char *name=c->obs[o].scene;for(int j=0;j<town_place_count();j++)if(eq(name,places[j].id))name=places[j].name;
                line_text(name,422,r.y+80,180,19,12,col_ink_soft());}
        }
        if(small_case_solved(g_case)) {
            line_text(c->ending,416,462,755,135,22,col_ink());
            char reward[140];snprintf(reward,sizeof reward,"%s   +2 feathers   +1 friendship",c->reward);
            line_text(reward,416,609,750,38,17,col_cool());
        } else {
            bool ready=small_case_seen(g_case,0)&&small_case_seen(g_case,1);
            for(int a=0;a<3;a++)pz_button((Rectangle){416,466+a*52.0f,750,44},c->answer[a],ready,19700+a*2);
            line_text(g_wrong?"That does not explain both observations. Read them together; there is no penalty.":ready?"What do the two observations tell you?":"Find both observations before drawing a conclusion.",416,628,755,30,15,col_ink_soft());
        }
    }
}
int casebook_update(void) {
    if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_M)||pz_button_clicked(close_rect(),true))return 1;
    for(int i=0;i<3;i++)if(pz_button_clicked(tab_rect(i),true)){g_tab=i;g_wrong=false;return 0;}
    if(g_tab==0) {
        int list[24],n=leads(list);
        for(int i=0;i<n&&i<6;i++)if(pz_button_clicked((Rectangle){88,234+i*57.0f,1100,48},town_unlocked(sources[list[i]].scene))){snprintf(g_dest,sizeof g_dest,"%s",sources[list[i]].scene);return 2;}
        if(!n&&pz_button_clicked((Rectangle){430,452,610,58},true)) {
            if(investigation_chapter()<6)return 3;
            snprintf(g_dest,sizeof g_dest,"%s",palette_stage()<6?"f_lantern":"p_square");return 2;
        }
    }else if(g_tab==1) {
        for(int i=0;i<town_place_count();i++)if(pz_button_clicked(map_rect(i),town_unlocked(places[i].id))){snprintf(g_dest,sizeof g_dest,"%s",places[i].id);return 2;}
    }else {
        for(int i=0;i<small_case_count();i++)if(pz_button_clicked(case_rect(i),town_unlocked(cases[i].obs[0].scene))){g_case=i;g_wrong=false;return 0;}
        const SmallCase *c=&cases[g_case];
        for(int o=0;o<2;o++)if(pz_button_clicked((Rectangle){416,228+o*113.0f,750,102},!small_case_seen(g_case,o))){snprintf(g_dest,sizeof g_dest,"%s",c->obs[o].scene);return 2;}
        for(int a=0;a<3;a++)if(pz_button_clicked((Rectangle){416,466+a*52.0f,750,44},!small_case_solved(g_case)&&small_case_seen(g_case,0)&&small_case_seen(g_case,1))) {
            if(small_case_answer(g_case,a)){sfx_play(SFX_SOLVE);save_autosave(scene_current());}else{g_wrong=true;sfx_play(SFX_CLUNK);}return 0;
        }
    }
    return 0;
}
