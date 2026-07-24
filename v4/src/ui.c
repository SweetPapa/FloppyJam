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

/* One key cap. Always sits on an opaque dark plate so it stays readable over
 * any background; the active key fills with its magnet color. */
static void draw_key(int x, int y, const char *k, MagColor mc, int active) {
    Color c = mag_color(mc, active);
    Rectangle r = {(float)x, (float)y, 26, 26};
    /* solid dark cap so busy 3D behind it never bleeds through */
    DrawRectangleRounded(r, 0.25f, 4, (Color){12, 14, 24, 255});
    if (active) {
        DrawRectangleRounded(r, 0.25f, 4, (Color){c.r, c.g, c.b, 255});
    } else {
        /* dim tint + saturated border keeps the color legible when idle */
        DrawRectangleRounded(r, 0.25f, 4, (Color){c.r, c.g, c.b, 55});
    }
    DrawRectangleRoundedLines(r, 0.25f, 4, active ? (Color){255,255,255,255} : c);
    int w = MeasureText(k, 14);
    Color tc = active ? (Color){10, 10, 20, 255} : (Color){235, 240, 255, 255};
    DrawText(k, x + 13 - w / 2, y + 6, 14, tc);
}

/* soft dark plate behind HUD text so it reads over the bright 3D scene */
static void plate(int x, int y, int w, int h, float alpha) {
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h},
                         0.28f, 5, (Color){6, 8, 16, (unsigned char)(alpha * 255)});
}

void ui_hud(App *a) {
    GameSim *g = &a->sim;
    char buf[64];
    int sw = GetScreenWidth();

    /* top-left: level + score on a dark plate */
    plate(10, 8, 300, g->combo > 1 ? 92 : 62, 0.55f);
    snprintf(buf, sizeof buf, "LEVEL %s  %s", LEVEL_LABELS[g->level_id - 1], g->lv->name);
    DrawText(buf, 18, 12, 20, COL_UI);
    snprintf(buf, sizeof buf, "SCORE %d", (int)(a->score_shown + 0.5f));
    DrawText(buf, 18, 36, 24, (Color){255, 221, 100, 255});

    /* combo with a draining timeout bar */
    if (g->combo > 1) {
        int mult = g->combo < COMBO_MAX ? g->combo : COMBO_MAX;
        snprintf(buf, sizeof buf, "COMBO x%d", mult);
        float pulse = 1.0f + 0.1f * sinf(a->t * 12.0f);
        DrawText(buf, 18, 66, (int)(22 * pulse), (Color){255, 120, 200, 255});
        float frac = g->combo_timer / COMBO_TIMEOUT;
        if (frac < 0) frac = 0; else if (frac > 1) frac = 1;
        DrawRectangle(140, 74, 150, 6, (Color){40, 20, 40, 200});
        DrawRectangle(140, 74, (int)(150 * frac), 6, (Color){255, 120, 200, 230});
    }

    /* top-right: timer + height + par */
    plate(sw - 190, 8, 180, g->deaths > 0 ? 96 : 76, 0.55f);
    snprintf(buf, sizeof buf, "%.1fs", g->elapsed);
    int tw = MeasureText(buf, 24);
    DrawText(buf, sw - 18 - tw, 12, 24, g->elapsed <= g->lv->par_time ? COL_UI : (Color){255,120,80,255});
    snprintf(buf, sizeof buf, "PAR %.0fs", g->lv->par_time);
    tw = MeasureText(buf, 16);
    DrawText(buf, sw - 18 - tw, 40, 16, COL_DIM);
    snprintf(buf, sizeof buf, "HEIGHT %.0f", sim_height(g));
    tw = MeasureText(buf, 16);
    DrawText(buf, sw - 18 - tw, 60, 16, COL_DIM);
    if (g->deaths > 0) {
        snprintf(buf, sizeof buf, "DEATHS %d", g->deaths);
        tw = MeasureText(buf, 16);
        DrawText(buf, sw - 18 - tw, 80, 16, (Color){255,120,120,255});
    }

    /* climb progress rail down the right edge */
    {
        float start_y = g->lv->mag[0].y, top_y = g->lv->mag[g->lv->n_mag - 1].y;
        float span = start_y - top_y;
        float prog = span > 1 ? (start_y - g->py) / span : 0;
        if (prog < 0) prog = 0; else if (prog > 1) prog = 1;
        int rx = sw - 16, ry0 = 120, ry1 = GetScreenHeight() - 120;
        DrawRectangle(rx, ry0, 4, ry1 - ry0, (Color){30, 34, 54, 180});
        int py = ry1 - (int)((ry1 - ry0) * prog);
        DrawRectangle(rx, py, 4, ry1 - py, (Color){120, 200, 255, 200});
        DrawCircle(rx + 2, py, 5, (Color){190, 230, 255, 255});
        /* lava marker on the same rail */
        float lprog = span > 1 ? (start_y - g->lava_y) / span : 0;
        if (lprog < -0.1f) lprog = -0.1f; else if (lprog > 1) lprog = 1;
        int ly = ry1 - (int)((ry1 - ry0) * lprog);
        if (ly >= ry0 - 10 && ly <= ry1 + 10)
            DrawCircle(rx + 2, ly, 5, (Color){255, 90, 30, 230});
    }

    /* bottom-center: control keys laid out in the real WASD keyboard shape
     * (W on top, A/S/D below) so the direction->color mapping reads at a
     * glance. Key box is 26px; step 30 leaves a 4px gap. */
    int step = 30;
    int cx = sw / 2;
    int by = GetScreenHeight() - 36; /* bottom row */
    int ty = by - step;              /* top row (W) */
    int lft = cx - 13;               /* left edge of a centered key */
    /* backdrop so the cluster stays legible over a bright/busy scene */
    plate(cx - 60, ty - 8, 120, 2 * step + 16, 0.72f);
    draw_key(lft,        ty, "W", COL_RED,    g->color == COL_RED);    /* up    */
    draw_key(lft - step, by, "A", COL_YELLOW, g->color == COL_YELLOW); /* left  */
    draw_key(lft,        by, "S", COL_BLUE,   g->color == COL_BLUE);   /* down  */
    draw_key(lft + step, by, "D", COL_GREEN,  g->color == COL_GREEN);  /* right */

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

    /* level intro card: slides in, holds, then fades */
    if (a->intro_t < 2.6f) {
        float t = a->intro_t;
        float alpha = (t < 0.35f) ? (t / 0.35f) : (t > 2.0f ? 1.0f - (t - 2.0f) / 0.6f : 1.0f);
        if (alpha < 0) alpha = 0; else if (alpha > 1) alpha = 1;
        float slide = (t < 0.35f) ? (1.0f - t / 0.35f) * 40.0f : 0.0f;
        int cy2 = GetScreenHeight() / 2 - 70;
        unsigned char al = (unsigned char)(alpha * 255);
        plate(sw / 2 - 250, cy2 - 14, 500, 96, alpha * 0.6f);
        snprintf(buf, sizeof buf, "LEVEL %s", LEVEL_LABELS[g->level_id - 1]);
        text_c(buf, sw / 2 + (int)slide, cy2, 26, (Color){180, 210, 255, al});
        text_c(g->lv->name, sw / 2 - (int)slide, cy2 + 30, 40,
               (Color){255, 235, 200, al});
        text_c("RISE OR BURN", sw / 2, cy2 + 74, 16, (Color){255, 140, 90, al});
    }
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
