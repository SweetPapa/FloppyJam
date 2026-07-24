/* ui.c — 2D HUD and menu screens drawn over the 3D world. */
#include "app.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static Color COL_UI = (Color){230, 235, 255, 255};
static Color COL_DIM = (Color){120, 130, 165, 255};

static void text_c(const char *s, int cx, int y, int size, Color c) {
    int w = MeasureText(s, size);
    DrawText(s, cx - w / 2, y, size, c);
}

/* small colored key hint for the 4 directions */
static void draw_key(int x, int y, const char *k, MagColor mc, int active) {
    Color c = mag_color(mc, active);
    DrawRectangle(x, y, 26, 26, (Color){c.r, c.g, c.b, active ? 255 : 90});
    DrawRectangleLines(x, y, 26, 26, COL_UI);
    int w = MeasureText(k, 14);
    DrawText(k, x + 13 - w / 2, y + 6, 14, (Color){10, 10, 20, 255});
}

void ui_hud(App *a) {
    GameSim *g = &a->sim;
    char buf[64];

    /* top-left: level + score */
    snprintf(buf, sizeof buf, "LEVEL %s  %s", LEVEL_LABELS[g->level_id - 1], g->lv->name);
    DrawText(buf, 16, 12, 20, COL_UI);
    snprintf(buf, sizeof buf, "SCORE %d", g->score);
    DrawText(buf, 16, 38, 24, (Color){255, 221, 100, 255});

    /* combo */
    if (g->combo > 1) {
        int mult = g->combo < COMBO_MAX ? g->combo : COMBO_MAX;
        snprintf(buf, sizeof buf, "COMBO x%d", mult);
        float pulse = 1.0f + 0.1f * sinf(a->t * 12.0f);
        int sz = (int)(22 * pulse);
        DrawText(buf, 16, 66, sz, (Color){255, 120, 200, 255});
    }

    /* top-right: timer + height + par */
    int sw = GetScreenWidth();
    snprintf(buf, sizeof buf, "%.1fs", g->elapsed);
    int tw = MeasureText(buf, 24);
    DrawText(buf, sw - 16 - tw, 12, 24, g->elapsed <= g->lv->par_time ? COL_UI : (Color){255,120,80,255});
    snprintf(buf, sizeof buf, "PAR %.0fs", g->lv->par_time);
    tw = MeasureText(buf, 16);
    DrawText(buf, sw - 16 - tw, 40, 16, COL_DIM);
    snprintf(buf, sizeof buf, "HEIGHT %.0f", sim_height(g));
    tw = MeasureText(buf, 16);
    DrawText(buf, sw - 16 - tw, 60, 16, COL_DIM);
    if (g->deaths > 0) {
        snprintf(buf, sizeof buf, "DEATHS %d", g->deaths);
        tw = MeasureText(buf, 16);
        DrawText(buf, sw - 16 - tw, 80, 16, (Color){255,120,120,255});
    }

    /* bottom-center: control key hints, highlighting current color */
    int cy = GetScreenHeight() - 40;
    int bx = sw / 2 - 74;
    draw_key(bx,      cy, "W", COL_RED,    g->color == COL_RED);
    draw_key(bx + 38, cy, "S", COL_BLUE,   g->color == COL_BLUE);
    draw_key(bx + 76, cy, "A", COL_YELLOW, g->color == COL_YELLOW);
    draw_key(bx + 114, cy, "D", COL_GREEN, g->color == COL_GREEN);

    /* lava warning banner */
    if (g->lava_y - g->py < LAVA_WARNING_DIST && g->state != PS_DEAD) {
        if ((int)(a->t * 4) & 1)
            text_c("!! LAVA RISING !!", sw / 2, 100, 26, (Color){255, 60, 30, 255});
    }
    /* AI race banner */
    if (g->ai.active) {
        const char *msg = (g->ai.y < g->py) ? "RIVAL AHEAD - CLIMB!" : "YOU'RE AHEAD";
        Color c = (g->ai.y < g->py) ? (Color){255,120,120,255} : (Color){120,255,160,255};
        text_c(msg, sw / 2, 130, 20, c);
    }
    if (g->race_lost) text_c("RIVAL REACHED THE TOP!", sw / 2, GetScreenHeight()/2 - 40, 30, (Color){255,80,120,255});
}

/* ---- star row ---------------------------------------------------- */
static void draw_stars(int cx, int y, int filled, int size) {
    for (int i = 0; i < 3; i++) {
        int x = cx - (size + 6) + i * (size + 6);
        Color c = i < filled ? (Color){255, 220, 90, 255} : (Color){70, 75, 100, 255};
        /* simple 5-point star via triangles around center */
        Vector2 pts[10];
        for (int k = 0; k < 10; k++) {
            float ang = -1.5708f + k * 0.62831f;
            float rr = (k & 1) ? size * 0.4f : size * 0.9f;
            pts[k] = (Vector2){x + cosf(ang) * rr, y + sinf(ang) * rr};
        }
        for (int k = 0; k < 10; k++)
            DrawTriangle((Vector2){x, y}, pts[k], pts[(k + 1) % 10], c);
    }
}

void ui_title(App *a) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float glow = 0.6f + 0.4f * sinf(a->t * 2.0f);
    text_c("MAGLAVA", sw / 2, sh / 2 - 150, 92, (Color){255, (unsigned char)(80 + glow*80), 30, 255});
    text_c("R I S E   O R   B U R N", sw / 2, sh / 2 - 66, 24, (Color){255, 180, 120, 255});

    if ((int)(a->t * 1.5f) & 1 ? 1 : 1)
        text_c("PRESS  ENTER  TO  PLAY", sw / 2, sh / 2 + 20,
               28, (Color){(unsigned char)(180+glow*70),230,255,255});
    text_c("Swing between magnetic nodes to climb above the rising lava.", sw/2, sh/2 + 70, 18, COL_DIM);
    text_c("W/Up = RED     S/Down = BLUE     A/Left = YELLOW     D/Right = GREEN", sw/2, sh/2 + 96, 18, COL_UI);
    text_c("Match a node's color to tether toward it.  Chain swings for combos.", sw/2, sh/2 + 122, 16, COL_DIM);
    text_c("v4 C/Raylib port  -  original by Sweet Papa Technologies", sw/2, sh - 30, 14, COL_DIM);
}

void ui_select(App *a) {
    int sw = GetScreenWidth();
    text_c("SELECT LEVEL", sw / 2, 40, 40, COL_UI);
    text_c("Arrow keys / WASD to choose    ENTER to play    ESC to go back", sw / 2, 88, 18, COL_DIM);

    int cols = 5;
    int cell = 96, pad = 16;
    int gridw = cols * cell + (cols - 1) * pad;
    int x0 = sw / 2 - gridw / 2;
    int y0 = 130;
    for (int i = 0; i < LEVEL_COUNT; i++) {
        int r = i / cols, c = i % cols;
        int x = x0 + c * (cell + pad);
        int y = y0 + r * (cell + pad);
        int unlocked = (i + 1) <= a->save.unlocked;
        int sel = (i == a->select_cursor);
        Color box = unlocked ? (Color){40, 46, 80, 255} : (Color){24, 26, 38, 255};
        DrawRectangle(x, y, cell, cell, box);
        if (sel) DrawRectangleLinesEx((Rectangle){x-2, y-2, cell+4, cell+4}, 3, (Color){255,200,80,255});
        else DrawRectangleLines(x, y, cell, cell, COL_DIM);

        char buf[16];
        snprintf(buf, sizeof buf, "%s", LEVEL_LABELS[i]);
        Color tc = unlocked ? COL_UI : COL_DIM;
        int tw = MeasureText(buf, 26);
        DrawText(buf, x + cell/2 - tw/2, y + 16, 26, tc);
        if (unlocked) draw_stars(x + cell/2, y + 66, a->save.stars[i], 12);
        else text_c("LOCKED", x + cell/2, y + 52, 16, COL_DIM);
    }
    /* selected level name */
    if (a->select_cursor >= 0 && a->select_cursor < LEVEL_COUNT) {
        const char *nm = LEVELS[a->select_cursor].name;
        text_c(nm, sw / 2, y0 + 5 * (cell + pad) + 6, 22, (Color){255,220,120,255});
    }
}

void ui_pause(App *a) {
    (void)a;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 150});
    text_c("PAUSED", sw / 2, sh / 2 - 80, 60, COL_UI);
    text_c("ESC  resume", sw / 2, sh / 2 + 10, 24, COL_UI);
    text_c("R  restart level", sw / 2, sh / 2 + 44, 22, COL_DIM);
    text_c("M  toggle sound", sw / 2, sh / 2 + 74, 22, COL_DIM);
    text_c("Q  quit to level select", sw / 2, sh / 2 + 104, 22, COL_DIM);
}

void ui_complete(App *a) {
    GameSim *g = &a->sim;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 170});
    text_c("LEVEL COMPLETE", sw / 2, sh / 2 - 170, 56, (Color){120, 255, 170, 255});

    int stars = sim_stars(g);
    /* reveal stars one at a time */
    int shown = (int)(a->complete_t / 0.4f);
    if (shown > stars) shown = stars;
    draw_stars(sw / 2, sh / 2 - 80, shown, 34);

    char buf[96];
    snprintf(buf, sizeof buf, "SCORE  %d", g->score);
    text_c(buf, sw / 2, sh / 2 - 10, 30, (Color){255, 221, 100, 255});
    snprintf(buf, sizeof buf, "TIME  %.1fs   (PAR %.0fs)", g->elapsed, g->lv->par_time);
    text_c(buf, sw / 2, sh / 2 + 28, 22, g->elapsed <= g->lv->par_time ? (Color){120,255,160,255} : COL_DIM);
    snprintf(buf, sizeof buf, "DEATHS  %d", g->deaths);
    text_c(buf, sw / 2, sh / 2 + 56, 22, g->deaths == 0 ? (Color){120,255,160,255} : COL_DIM);

    /* star criteria hint */
    text_c("1star finish   2star beat par   3star no deaths", sw/2, sh/2 + 92, 16, COL_DIM);

    int last = (g->level_id >= LEVEL_COUNT);
    if (a->complete_t > 1.2f) {
        if (!last) text_c("ENTER  next level     R  retry     ESC  level select",
                          sw / 2, sh / 2 + 140, 22, COL_UI);
        else text_c("You conquered the facility!   R  retry     ESC  level select",
                    sw / 2, sh / 2 + 140, 22, (Color){255,220,120,255});
    }
}
