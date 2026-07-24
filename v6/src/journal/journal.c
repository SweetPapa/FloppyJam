#include "journal.h"
#include "content/content.h"
#include "flags/flags.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "board/board.h"
#include "puzzle/puzzle.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

enum { TAB_PEOPLE = 0, TAB_CLUES, TAB_BOARDS, TAB_TOWN, TAB_COUNT };

static bool  g_open;
static int   g_tab;
static float g_t;
static int   g_scroll;
static char  g_replay[48];

static const char *TAB_NAME[TAB_COUNT] = { "People", "Clues", "Boards", "Town" };

void journal_open(void) { g_open = true; g_scroll = 0; sfx_play(SFX_PAGE); }
void journal_close(void) { g_open = false; sfx_play(SFX_PAGE); }
bool journal_is_open(void) { return g_open; }
const char *journal_replay_request(void) { return g_replay; }
void journal_clear_replay(void) { g_replay[0] = 0; }

static Rectangle tab_rect(int i)
{
    return (Rectangle){ 96 + i * 150.0f, 52, 140, 44 };
}
static Rectangle close_rect(void) { return (Rectangle){ 1074, 52, 110, 44 }; }

bool journal_update(float dt)
{
    g_t += dt;
    if (!g_open) return true;

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_J) || IsKeyPressed(KEY_TAB)) {
        journal_close();
        return true;
    }
    Vector2 m = art_mouse();
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (click && CheckCollisionPointRec(m, close_rect())) { journal_close(); return true; }
    for (int i = 0; i < TAB_COUNT; i++)
        if (click && CheckCollisionPointRec(m, tab_rect(i))) {
            if (g_tab != i) sfx_play(SFX_PAGE);
            g_tab = i;
            g_scroll = 0;
        }

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        g_scroll -= (int)wheel;
        if (g_scroll < 0) g_scroll = 0;
    }
    if (IsKeyPressed(KEY_DOWN)) g_scroll++;
    if (IsKeyPressed(KEY_UP) && g_scroll > 0) g_scroll--;

    /* Town tab: puzzle replays, purely for fun (§5.1) */
    if (g_tab == TAB_TOWN && click) {
        int n = puzzle_count();
        int shown = 0;
        for (int i = 0; i < n; i++) {
            const pz_def *d = puzzle_at(i);
            char key[FLAG_MAX_KEY];
            snprintf(key, sizeof key, "pzdone.%s", d->id);
            if (!flag_get(key)) continue;
            Rectangle r = { 660, 250 + shown * 34.0f, 480, 30 };
            shown++;
            if (CheckCollisionPointRec(m, r)) {
                snprintf(g_replay, sizeof g_replay, "%s", d->id);
                journal_close();
                return true;
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------------ draw */
static void draw_people(Rectangle page)
{
    float y = page.y + 16;
    int shown = 0;
    for (int i = 0; i < npc_count(); i++) {
        char key[FLAG_MAX_KEY];
        snprintf(key, sizeof key, "met.%s", npc_key(i));
        if (!flag_get(key)) continue;
        if (shown++ < g_scroll) continue;
        if (y > page.y + page.height - 110) break;

        Rectangle port = { page.x + 12, y, 84, 84 };
        npc_bust(i, port, EM_NEUTRAL, g_t);
        ink_rect(port, 1.8f, 0.7f, 2000 + i, col_ink_soft());
        art_text(npc_display(i), page.x + 110, y + 2, 19, col_ink());

        char skey[FLAG_MAX_KEY];
        snprintf(skey, sizeof skey, "who.%s", npc_key(i));
        art_text_wrap(ui_str(skey), page.x + 110, y + 30, page.width - 130, 15,
                      col_ink_soft(), -1);

        int tr = trust_get(npc_key(i));
        for (int f = 0; f < 5; f++)
            doodle(D_FEATHER, page.x + 112 + f * 22.0f, y + 76, 8, 0.4f,
                   f < tr ? col_accent_a() : col_paper_dark());
        y += 100;
    }
    if (shown == 0)
        art_text("Nobody yet. Go and knock on a door.", page.x + 16, page.y + 20,
                 18, col_ink_soft());
}

static void draw_clues(Rectangle page)
{
    char head[64];
    snprintf(head, sizeof head, "%d pieces of evidence", clue_count());
    art_text(head, page.x + 14, page.y + 10, 16, col_ink_soft());

    float y = page.y + 42;
    for (int i = g_scroll; i < clue_count(); i++) {
        if (y > page.y + page.height - 40) break;
        const char *id = clue_at(i);
        doodle(D_STAR, page.x + 24, y + 12, 10, 0, col_accent_a());
        art_text(ui_str(id), page.x + 44, y, 17, col_ink());

        char skey[FLAG_MAX_KEY];
        snprintf(skey, sizeof skey, "src.%s", id);
        int src = flag_get(skey) - 1;
        char from[128];
        if (src >= 0) snprintf(from, sizeof from, "from %s", npc_display(src));
        else          snprintf(from, sizeof from, "%s", "worked out on your own");
        art_text(from, page.x + 46, y + 22, 13, col_ink_soft());
        y += 46;
    }
    if (clue_count() == 0)
        art_text("Nothing yet. That is what mornings are for.", page.x + 16,
                 page.y + 46, 18, col_ink_soft());
}

static void draw_boards(Rectangle page)
{
    float y = page.y + 16;
    int n = content_block_count("board");
    for (int i = 0; i < n; i++) {
        char id[48];
        Block b;
        if (!content_block_at("board", i, id, sizeof id, &b)) continue;
        if (y > page.y + page.height - 40) break;

        bool solved = board_is_solved(id);
        char tkey[64];
        snprintf(tkey, sizeof tkey, "boardname.%s", id);
        doodle(solved ? D_STAR : D_BOOK, page.x + 26, y + 12, 12, 0,
               solved ? col_cool() : col_ink_soft());
        art_text(ui_str(tkey), page.x + 50, y, 18, solved ? col_ink() : col_ink_soft());
        art_text(solved ? "solved" : "not yet", page.x + 52, y + 24, 13,
                 col_ink_soft());
        y += 48;
    }
}

static void draw_town(Rectangle page)
{
    static const char *district[7] = {
        "Town Gate & Square", "The Harbor", "Market Row", "The Old Quarter",
        "The Gardens", "Tower Green", "The Prismworks"
    };
    static const char *hue[7] = {
        "gray", "blue", "yellow", "red", "green", "violet", "full spectrum"
    };
    int stage = palette_stage();

    art_text("Prismbrook", page.x + 14, page.y + 8, 20, col_ink());
    float y = page.y + 44;
    for (int i = 0; i < 7; i++) {
        bool open = i <= stage;
        doodle(i == 6 ? D_PRISM : D_HOUSE, page.x + 30, y + 10, 13, 0,
               open ? col_accent_b() : col_paper_dark());
        art_text(district[i], page.x + 56, y, 17, open ? col_ink() : col_ink_soft());
        art_text(i <= stage ? hue[i] : "still gray", page.x + 300, y + 2, 14,
                 col_ink_soft());
        y += 30;
    }

    char fb[64];
    snprintf(fb, sizeof fb, "Feathers: %d", flag_get("feathers"));
    art_text(fb, page.x + 14, y + 14, 18, col_ink());
    doodle(D_FEATHER, page.x + 190, y + 24, 14, 0.3f, col_accent_a());

    /* hue ribbon: the progress bar you can feel (§1.3.2) */
    for (int i = 1; i <= 6; i++) {
        static const Color band[7] = {
            { 0, 0, 0, 0 }, { 96, 150, 210, 255 }, { 232, 200, 84, 255 },
            { 200, 88, 82, 255 }, { 104, 168, 116, 255 }, { 152, 112, 196, 255 },
            { 232, 186, 92, 255 }
        };
        Rectangle r = { page.x + 14 + (i - 1) * 46.0f, y + 54, 40, 18 };
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 3000 + i, i <= stage ? band[i] : col_paper_dark());
        ink_rect(r, 1.6f, 0.6f, 3010 + i, col_ink_soft());
    }

    /* replays */
    art_text("Puzzles, to play again", 660, 214, 17, col_ink());
    int shown = 0;
    for (int i = 0; i < puzzle_count(); i++) {
        const pz_def *d = puzzle_at(i);
        char key[FLAG_MAX_KEY];
        snprintf(key, sizeof key, "pzdone.%s", d->id);
        if (!flag_get(key)) continue;
        Rectangle r = { 660, 250 + shown * 34.0f, 480, 30 };
        bool hot = art_hover(r);
        art_text(d->title ? d->title : d->id, r.x + 8, r.y + 4, 16,
                 hot ? col_accent_b() : col_ink_soft());
        shown++;
        if (shown > 9) break;
    }
    if (shown == 0)
        art_text("Solve one and it lives here forever.", 660, 250, 15, col_ink_soft());
}

void journal_draw(void)
{
    if (!g_open) return;
    DrawRectangle(0, 0, VW, VH, (Color){ 26, 22, 26, 150 });

    Rectangle book = { 80, 40, VW - 160, VH - 90 };
    paper_panel(book, 6.0f, 5151);
    ink_line(book.x + book.width * 0.5f, book.y + 70,
             book.x + book.width * 0.5f, book.y + book.height - 20, 2.0f, 2.0f,
             5152, col_paper_dark());

    for (int i = 0; i < TAB_COUNT; i++) {
        Rectangle r = tab_rect(i);
        bool on = (i == g_tab);
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width - 6, r.y + r.height }, { r.x + 6, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 5160 + i,
                 on ? col_accent_a() : (Color){ 226, 214, 192, 255 });
        ink_stroke((Vector2[]){ q[0], q[1], q[2], q[3], q[0] }, 5, 2.0f, 0.7f,
                   5170 + i, col_ink());
        float w = art_text_w(TAB_NAME[i], 17);
        art_text(TAB_NAME[i], r.x + (r.width - w) * 0.5f, r.y + 11, 17, col_ink());
    }
    Rectangle cr = close_rect();
    pz_button(cr, ui_str("journal.close"), true, 5180);

    Rectangle page = { book.x + 24, book.y + 74, book.width - 48, book.height - 100 };
    switch (g_tab) {
    case TAB_PEOPLE: draw_people(page); break;
    case TAB_CLUES:  draw_clues(page);  break;
    case TAB_BOARDS: draw_boards(page); break;
    default:         draw_town(page);   break;
    }

    /* margin doodles — the book is charming or it is not a book */
    doodle(D_TEACUP, book.x + book.width - 60, book.y + book.height - 40, 16, 0.2f,
           col_ink_soft());
    doodle(D_FEATHER, book.x + 40, book.y + book.height - 42, 14, -0.4f, col_ink_soft());
}
