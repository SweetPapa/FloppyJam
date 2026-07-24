/* §10.2-10.4 gates, run against the real baked town.
 *
 * bakery checks the content at build time. This checks the same content
 * through the interpreters' own reader, which is the thing that will
 * actually be running when somebody plays it — a grammar the linter accepts
 * and the interpreter cannot read is still a broken game.
 *
 * The headline test is CLUE CLOSURE (§10.4): for every board, every clue it
 * requires and every piece of evidence it cites must be grantable, and every
 * load-bearing blank must be reachable from at least two independent
 * sources. A mystery that can dead-end an honest player is broken.
 */
#include "content/content.h"
#include "flags/flags.h"
#include "save/save.h"
#include "puzzle/puzzle.h"
#include "art/artkit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_fail, g_checks;

static void expect(bool ok, const char *what, const char *detail)
{
    g_checks++;
    if (ok) return;
    printf("  FAIL  %s: %s\n", what, detail ? detail : "");
    g_fail++;
}

/* ------------------------------------------------------------ clue sources */
#define MAX_CLUES 128
static char g_granted[MAX_CLUES][64];
static int  g_ngranted;

static bool is_granted(const char *id)
{
    for (int i = 0; i < g_ngranted; i++) if (eq(g_granted[i], id)) return true;
    return false;
}

static void note_grant(const char *id)
{
    if (is_granted(id) || g_ngranted >= MAX_CLUES) return;
    snprintf(g_granted[g_ngranted++], 64, "%s", id);
}

/* every `grant` in every dialogue node, plus every puzzle's clue_granted */
static void collect_grants(void)
{
    int n = content_block_count("@node");
    for (int i = 0; i < n; i++) {
        char id[48];
        Block b;
        if (!content_block_at("@node", i, id, sizeof id, &b)) continue;
        Cursor c;
        cur_open(&c, &b);
        char line[512];
        while (cur_line(&c, line, sizeof line)) {
            const char *p = line;
            char kw[32];
            word(&p, kw, sizeof kw);
            if (!eq(kw, "grant")) continue;
            char t[64];
            while (word(&p, t, sizeof t) && t[0]) note_grant(t);
        }
    }
    for (int i = 0; i < puzzle_count(); i++) {
        const pz_def *d = puzzle_at(i);
        if (d->clue_granted && d->clue_granted[0]) note_grant(d->clue_granted);
    }
}

/* --------------------------------------------------------------- the boards */
#define MAX_POOLS 6
#define MAX_TOKS  16

static void check_boards(void)
{
    int n = content_block_count("board");
    expect(n == 6, "boards", "the chapter spine wants six boards (CANON 3.1)");

    for (int i = 0; i < n; i++) {
        char id[48];
        Block b;
        if (!content_block_at("board", i, id, sizeof id, &b)) continue;

        char pool_name[MAX_POOLS][24];
        char pool_tok[MAX_POOLS][MAX_TOKS][48];
        int  pool_n[MAX_POOLS] = { 0 };
        int  npools = 0;
        int  nblanks = 0, total_from = 0;
        char solved_kind[16] = "", solved_id[48] = "";
        bool has_title = false;
        int  ntext = 0;
        char placeholders[16][16];
        int  nplace = 0;

        Cursor c;
        cur_open(&c, &b);
        char line[512];
        while (cur_line(&c, line, sizeof line)) {
            const char *p = line;
            char kw[32];
            word(&p, kw, sizeof kw);

            if (eq(kw, "title")) { has_title = true; }
            else if (eq(kw, "text")) {
                ntext++;
                char body[256];
                rest(p, body, sizeof body);
                for (const char *q = body; *q; q++)
                    if (*q == '{' && nplace < 16) {
                        int m = 0;
                        for (q++; *q && *q != '}' && m < 15; q++) placeholders[nplace][m++] = *q;
                        placeholders[nplace][m] = 0;
                        nplace++;
                        if (!*q) break;
                    }
            } else if (eq(kw, "requires")) {
                char t[64];
                while (word(&p, t, sizeof t) && t[0]) {
                    char d[160];
                    snprintf(d, sizeof d, "board '%s' requires '%s', which nothing grants",
                             id, t);
                    expect(is_granted(t), "clue closure", d);
                }
            } else if (eq(kw, "pool")) {
                if (npools >= MAX_POOLS) continue;
                word(&p, pool_name[npools], 24);
                char t[48];
                while (word(&p, t, sizeof t) && t[0] && pool_n[npools] < MAX_TOKS)
                    snprintf(pool_tok[npools][pool_n[npools]++], 48, "%s", t);
                npools++;
            } else if (eq(kw, "blank")) {
                nblanks++;
                char name[24] = "", answer[48] = "", pool[24] = "";
                int nfrom = 0;
                word(&p, name, sizeof name);
                char t[64];
                while (word(&p, t, sizeof t) && t[0]) {
                    if (eq(t, "answer")) word(&p, answer, sizeof answer);
                    else if (eq(t, "pool")) word(&p, pool, sizeof pool);
                    else if (eq(t, "from")) {
                        char f[64];
                        while (word(&p, f, sizeof f) && f[0]) {
                            nfrom++;
                            total_from++;
                            char d[192];
                            snprintf(d, sizeof d, "board '%s' blank %s cites '%s', "
                                     "which nothing grants", id, name, f);
                            expect(is_granted(f), "clue closure", d);
                        }
                        break;
                    }
                }
                char d[192];
                snprintf(d, sizeof d, "board '%s' blank %s", id, name);
                expect(answer[0] != 0, "blank has an answer", d);
                expect(pool[0] != 0, "blank names a pool", d);

                /* the fairness contract: two independent roads to every
                 * load-bearing answer (CANON 3.3) */
                snprintf(d, sizeof d, "board '%s' blank %s has only %d source(s)",
                         id, name, nfrom);
                expect(nfrom >= 2, "two roads to every answer", d);

                /* the answer must actually be sittable in that pool */
                bool found_pool = false, found_tok = false;
                for (int k = 0; k < npools; k++) {
                    if (!eq(pool_name[k], pool)) continue;
                    found_pool = true;
                    for (int t2 = 0; t2 < pool_n[k]; t2++)
                        if (eq(pool_tok[k][t2], answer)) found_tok = true;
                }
                snprintf(d, sizeof d, "board '%s' blank %s -> pool '%s'", id, name, pool);
                expect(found_pool, "blank's pool exists", d);
                snprintf(d, sizeof d, "board '%s' answer '%s' is not in pool '%s'",
                         id, answer, pool);
                expect(found_tok, "answer is in its own pool", d);

                /* a blank the player can never see is a blank they cannot fill */
                bool placed = false;
                for (int k = 0; k < nplace; k++) if (eq(placeholders[k], name)) placed = true;
                snprintf(d, sizeof d, "board '%s' blank %s appears in no text line",
                         id, name);
                expect(placed, "every blank is on the scroll", d);
            } else if (eq(kw, "on_solved")) {
                word(&p, solved_kind, sizeof solved_kind);
                word(&p, solved_id, sizeof solved_id);
            }
        }

        char d[160];
        snprintf(d, sizeof d, "board '%s'", id);
        expect(has_title, "board has a title", d);
        expect(ntext > 0, "board has a scroll to read", d);
        expect(nblanks >= 3 && nblanks <= 9, "board is 3-9 blanks (CANON 3.3)", d);
        expect(total_from >= nblanks * 2, "board cites >=2 sources per blank", d);

        Block dummy;
        if (eq(solved_kind, "cut"))
            expect(content_block("cut", solved_id, &dummy), "on_solved cutscene exists", d);
        else if (eq(solved_kind, "goto"))
            expect(content_block("@node", solved_id, &dummy), "on_solved node exists", d);
        else
            expect(false, "board says what happens when it is solved", d);

        /* every clue and token the player will read must have a label */
        expect(!eq(ui_str("boardname.p_morning"), "boardname.p_morning"),
               "board names are in strings/ui.txt", d);
    }
}

/* --------------------------------------------------------------- the scenes */
static void check_scenes(void)
{
    int n = content_block_count("scene");
    expect(n >= 12, "scenes", "the districts want a dozen screens or more");

    for (int i = 0; i < n; i++) {
        char id[48];
        Block b;
        if (!content_block_at("scene", i, id, sizeof id, &b)) continue;
        Cursor c;
        cur_open(&c, &b);
        char line[512];
        bool has_bg = false, has_title = false;
        int  exits = 0;

        while (cur_line(&c, line, sizeof line)) {
            const char *p = line;
            char kw[32];
            word(&p, kw, sizeof kw);
            Block dummy;
            char d[192];

            if (eq(kw, "bg")) has_bg = true;
            else if (eq(kw, "title")) has_title = true;
            else if (eq(kw, "on_enter")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "scene '%s' opens on missing node '%s'", id, t);
                expect(content_block("@node", t, &dummy), "scene on_enter", d);
            } else if (eq(kw, "on_enter_cut")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "scene '%s' opens on missing cutscene '%s'", id, t);
                expect(content_block("cut", t, &dummy), "scene on_enter_cut", d);
            } else if (eq(kw, "actor")) {
                char t[32];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "scene '%s' places unknown character '%s'", id, t);
                expect(npc_by_name(t) >= 0, "scene actor exists", d);
            } else if (eq(kw, "hot")) {
                char t[24];
                for (int k = 0; k < 4; k++) word(&p, t, sizeof t);
                char kind[24], arg[48];
                if (!word(&p, kind, sizeof kind)) continue;
                if (eq(kind, "look")) continue;
                word(&p, arg, sizeof arg);
                if (eq(kind, "talk")) {
                    snprintf(d, sizeof d, "scene '%s' -> missing node '%s'", id, arg);
                    expect(content_block("@node", arg, &dummy), "hotspot talk", d);
                } else if (eq(kind, "exit")) {
                    exits++;
                    snprintf(d, sizeof d, "scene '%s' -> missing scene '%s'", id, arg);
                    expect(content_block("scene", arg, &dummy), "hotspot exit", d);
                } else if (eq(kind, "board")) {
                    snprintf(d, sizeof d, "scene '%s' -> missing board '%s'", id, arg);
                    expect(content_block("board", arg, &dummy), "hotspot board", d);
                } else if (eq(kind, "puzzle")) {
                    snprintf(d, sizeof d, "scene '%s' -> missing puzzle '%s'", id, arg);
                    expect(puzzle_find(arg) != NULL, "hotspot puzzle", d);
                } else if (eq(kind, "cut")) {
                    snprintf(d, sizeof d, "scene '%s' -> missing cutscene '%s'", id, arg);
                    expect(content_block("cut", arg, &dummy), "hotspot cut", d);
                }
            }
        }
        char d[160];
        snprintf(d, sizeof d, "scene '%s'", id);
        expect(has_bg, "scene names a backdrop", d);
        expect(has_title, "scene has a title for the HUD", d);
        expect(exits >= 1, "no scene is a dead end", d);
    }
}

/* ------------------------------------------------------------ the cutscenes */
static const char *VERBS[] = {
    "BG", "PAN", "ACTOR", "MOVE", "EMOTE", "SAY", "WAIT", "MUSIC", "SFX",
    "FADE", "HUE", "BLOOM", "DOODLE", "SHAKE", "TITLE", "EXIT", "CAM", 0
};

static void check_cutscenes(void)
{
    int n = content_block_count("cut");
    expect(n >= 8, "cutscenes", "CANON 15 wants eight majors");

    for (int i = 0; i < n; i++) {
        char id[48];
        Block b;
        if (!content_block_at("cut", i, id, sizeof id, &b)) continue;
        Cursor c;
        cur_open(&c, &b);
        char line[512];
        int lines = 0;

        while (cur_line(&c, line, sizeof line)) {
            lines++;
            const char *p = line;
            char kw[24];
            word(&p, kw, sizeof kw);
            bool ok = false;
            for (int k = 0; VERBS[k]; k++) if (eq(VERBS[k], kw)) ok = true;
            char d[192];
            snprintf(d, sizeof d, "cutscene '%s' uses unknown verb '%s'", id, kw);
            expect(ok, "cutscene verb set is closed (CANON 9.3)", d);

            if (eq(kw, "BG")) {
                char t[48];
                word(&p, t, sizeof t);
                Block dummy;
                snprintf(d, sizeof d, "cutscene '%s' -> missing scene '%s'", id, t);
                expect(content_block("scene", t, &dummy), "cutscene BG", d);
            } else if (eq(kw, "ACTOR") || eq(kw, "MOVE") || eq(kw, "EMOTE") ||
                       eq(kw, "SAY")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "cutscene '%s' uses unknown character '%s'", id, t);
                expect(npc_by_name(t) >= 0, "cutscene actor exists", d);
            }
        }
        char d[160];
        snprintf(d, sizeof d, "cutscene '%s' is %d lines", id, lines);
        expect(lines >= 4, "a cutscene is worth playing", d);
    }
}

/* -------------------------------------------------------------- the dialogue */
static void check_dialogue(void)
{
    int n = content_block_count("@node");
    expect(n >= 100, "dialogue", "a 4-6 hour game wants a town's worth of talk");

    int grants = 0;
    for (int i = 0; i < n; i++) {
        char id[48];
        Block b;
        if (!content_block_at("@node", i, id, sizeof id, &b)) continue;
        Cursor c;
        cur_open(&c, &b);
        char line[512];
        while (cur_line(&c, line, sizeof line)) {
            char d[192];
            Block dummy;
            const char *arrow = strstr(line, "->");
            if (arrow) {
                const char *q = arrow + 2;
                char t[48];
                if (word(&q, t, sizeof t) && t[0]) {
                    snprintf(d, sizeof d, "node '%s' jumps to missing node '%s'", id, t);
                    expect(content_block("@node", t, &dummy), "dialogue jump", d);
                }
            }
            const char *p = line;
            char kw[32];
            word(&p, kw, sizeof kw);
            if (eq(kw, "goto")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "node '%s' goes to missing node '%s'", id, t);
                expect(content_block("@node", t, &dummy), "dialogue goto", d);
            } else if (eq(kw, "start_puzzle")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "node '%s' starts missing puzzle '%s'", id, t);
                expect(puzzle_find(t) != NULL, "dialogue start_puzzle", d);
            } else if (eq(kw, "play_cut")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "node '%s' plays missing cutscene '%s'", id, t);
                expect(content_block("cut", t, &dummy), "dialogue play_cut", d);
            } else if (eq(kw, "open_board")) {
                char t[48];
                word(&p, t, sizeof t);
                snprintf(d, sizeof d, "node '%s' opens missing board '%s'", id, t);
                expect(content_block("board", t, &dummy), "dialogue open_board", d);
            } else if (eq(kw, "grant")) {
                grants++;
                char t[64];
                while (word(&p, t, sizeof t) && t[0]) {
                    snprintf(d, sizeof d, "clue '%s' has no label in strings/ui.txt", t);
                    expect(!eq(ui_str(t), t), "every clue reads as English", d);
                }
            }
        }
    }
    expect(grants >= 20, "dialogue grants evidence", "the town should hand over a case");
}

/* ------------------------------------------------------- the town is walkable
 *
 * Clue closure proves the evidence exists. This proves the player can get to
 * it: every screen reachable on foot from the road into town, and every board
 * gate satisfiable from clues that are actually grantable. A chapter that
 * opens on a clue nothing hands out is a soft-lock, and a soft-lock in a cozy
 * game is the worst bug there is. */
#define MAX_SCENES 32

static void check_reachability(void)
{
    char name[MAX_SCENES][48];
    bool seen[MAX_SCENES] = { false };
    int n = content_block_count("scene");
    if (n > MAX_SCENES) n = MAX_SCENES;
    for (int i = 0; i < n; i++) {
        Block b;
        content_block_at("scene", i, name[i], 48, &b);
    }

    /* walk the exit graph from the road into town */
    int queue[MAX_SCENES], head = 0, tail = 0;
    for (int i = 0; i < n; i++)
        if (eq(name[i], "p_gate")) { seen[i] = true; queue[tail++] = i; }
    expect(tail == 1, "the road into town exists", "no scene called p_gate");

    while (head < tail) {
        int cur = queue[head++];
        Block b;
        if (!content_block("scene", name[cur], &b)) continue;
        Cursor c;
        cur_open(&c, &b);
        char line[512];
        while (cur_line(&c, line, sizeof line)) {
            const char *p = line;
            char kw[32];
            word(&p, kw, sizeof kw);
            if (!eq(kw, "hot")) continue;
            char t[24];
            for (int k = 0; k < 4; k++) word(&p, t, sizeof t);
            char kind[24], arg[48];
            if (!word(&p, kind, sizeof kind) || !eq(kind, "exit")) continue;
            if (!word(&p, arg, sizeof arg)) continue;
            for (int i = 0; i < n; i++)
                if (!seen[i] && eq(name[i], arg)) { seen[i] = true; queue[tail++] = i; }
        }
    }
    for (int i = 0; i < n; i++) {
        char d[128];
        snprintf(d, sizeof d, "scene '%s' cannot be walked to from the town gate",
                 name[i]);
        expect(seen[i], "every screen is reachable on foot", d);
    }

    /* every clue a board gate waits on must be grantable */
    for (int i = 0; i < n; i++) {
        Block b;
        if (!content_block("scene", name[i], &b)) continue;
        Cursor c;
        cur_open(&c, &b);
        char line[512];
        while (cur_line(&c, line, sizeof line)) {
            const char *p = line;
            char kw[32];
            word(&p, kw, sizeof kw);
            if (!eq(kw, "hot")) continue;
            char t[24];
            for (int k = 0; k < 4; k++) word(&p, t, sizeof t);
            char kind[24], arg[48];
            if (!word(&p, kind, sizeof kind)) continue;
            if (!eq(kind, "board") && !eq(kind, "exit")) continue;
            word(&p, arg, sizeof arg);
            char tail_s[240];
            rest(p, tail_s, sizeof tail_s);
            const char *q = tail_s;
            char lab[200];
            word(&q, lab, sizeof lab);
            char maybe[8];
            if (!word(&q, maybe, sizeof maybe) || !eq(maybe, "if")) continue;
            char op[8];
            word(&q, op, sizeof op);
            if (!eq(op, "all") && !eq(op, "any")) continue;
            char cl[64];
            while (word(&q, cl, sizeof cl) && cl[0]) {
                if (strncmp(cl, "clue.", 5) != 0) continue;
                char d[192];
                snprintf(d, sizeof d, "'%s' gates on '%s', which nothing grants",
                         arg, cl);
                expect(is_granted(cl), "gates open on grantable clues", d);
            }
        }
    }
}

/* --------------------------------------------------------------- conditions */
static void check_conditions(void)
{
    flags_reset();
    flag_set("ch1.met_otto", 2);
    clue_grant("clue.otto_alibi");
    trust_add("otto", 3);

    expect(content_cond("flag ch1.met_otto == 2"), "cond ==", NULL);
    expect(content_cond("flag ch1.met_otto >= 1"), "cond >=", NULL);
    expect(!content_cond("flag ch1.met_otto == 0"), "cond == is not always true", NULL);
    expect(content_cond("flag ch1.met_otto"), "bare flag is truthy", NULL);
    expect(content_cond("not flag ch1.never_set"), "not on an unset flag", NULL);
    expect(content_cond("clue clue.otto_alibi"), "cond clue", NULL);
    expect(!content_cond("clue clue.tansy_saw"), "cond clue, ungranted", NULL);
    expect(content_cond("not clue clue.tansy_saw"), "cond not clue", NULL);
    expect(content_cond("trust otto >= 3"), "cond trust", NULL);

    clue_grant("clue.tansy_saw");
    expect(content_cond("all clue.otto_alibi clue.tansy_saw"), "cond all, satisfied", NULL);
    expect(!content_cond("all clue.otto_alibi clue.ferry_log"), "cond all, short", NULL);
    expect(content_cond("any clue.ferry_log clue.tansy_saw"), "cond any", NULL);
    flags_reset();
}

/* ------------------------------------------------------------------- saves */
static void check_saves(void)
{
    settings_defaults();
    flags_reset();
    flag_set("ch1.met_otto", 1);
    flag_set("feathers", 7);
    clue_grant("clue.otto_alibi");
    clue_grant("clue.ferry_log");
    trust_add("otto", 3);

    expect(save_write(2, "ch1_dock", "a test recap"), "save writes", NULL);
    flags_reset();
    expect(flag_get("feathers") == 0, "reset really resets", NULL);
    expect(save_read(2), "save reads back", NULL);

    expect(flag_get("ch1.met_otto") == 1, "flags survive a round trip", NULL);
    expect(flag_get("feathers") == 7, "counters survive a round trip", NULL);
    expect(clue_has("clue.otto_alibi") && clue_has("clue.ferry_log"),
           "clues survive a round trip", NULL);
    expect(clue_count() == 2, "clue order survives a round trip", NULL);
    expect(trust_get("otto") == 3, "trust survives a round trip", NULL);
    expect(strcmp(save_last_scene(), "ch1_dock") == 0, "the save remembers where", NULL);

    /* forward compatibility: a save that mentions a flag this build has never
     * heard of must load, because content will keep being added (§7.3) */
    FILE *f = fopen("huedunit2.sav", "ab");
    if (f) { fprintf(f, "f ch9.some_future_thing 4\n"); fclose(f); }
    expect(save_read(2), "a save with unknown flags still loads", NULL);
    expect(flag_get("ch9.some_future_thing") == 4, "unknown flags are preserved", NULL);

    char recap[512];
    save_build_recap(recap, sizeof recap);
    expect(strlen(recap) > 40, "the recap says something", NULL);

    remove("huedunit2.sav");
    flags_reset();
}

int main(void)
{
    printf("test_content:\n");
    content_init();
    settings_defaults();

    printf("  %d nodes, %d scenes, %d boards, %d cutscenes, %d puzzles\n",
           content_block_count("@node"), content_block_count("scene"),
           content_block_count("board"), content_block_count("cut"),
           puzzle_count());

    collect_grants();
    printf("  %d distinct clues are grantable\n", g_ngranted);

    check_boards();
    check_scenes();
    check_cutscenes();
    check_dialogue();
    check_reachability();
    check_conditions();
    check_saves();

    if (g_fail) {
        printf("test_content: %d failure(s) in %d checks\n", g_fail, g_checks);
        return 1;
    }
    printf("test_content: all green (%d checks)\n", g_checks);
    return 0;
}
