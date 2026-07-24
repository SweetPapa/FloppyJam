/* npc — the paper-puppet shape grammar (§8.1, §8.3).
 *
 * Seventeen townsfolk, one bird and a detective, all built from the same
 * seven parameters. Upgrading the grammar upgrades the whole cast at once,
 * which is exactly why Wave 3 can make everyone prettier in one card.
 */
#include "artkit.h"
#include "save/save.h"

#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

enum { HAT_NONE = 0, HAT_CAP, HAT_BRIM, HAT_TALL, HAT_KERCHIEF, HAT_BUN,
       HAT_TOP, HAT_VEIL };

typedef struct {
    const char *key;
    const char *display;
    float  height;        /* 0.85 short .. 1.15 tall */
    float  girth;
    int    hat;
    int    prop;          /* doodle id, -1 for none */
    float  stoop;
    Color  coat;
    Color  trim;
    Color  hair;
} Puppet;

/* Colours are authored at full spectrum. The palette decides how much of
 * each one the player has earned back (§8.2) — Otto's blue coat is the
 * first thing in town to come back, and that is on purpose. */
static const Puppet g_cast[] = {
/*  key         display                 h     g   hat           prop        stoop  coat                      trim                      hair                  */
  { "you",      "You",                1.02f, 1.00f, HAT_TALL,     D_BOOK,     0.00f, {  84,  92, 116, 255 }, { 196, 176, 132, 255 }, {  74,  56,  46, 255 } },
  { "maribel",  "Capt. Maribel Sorge",1.04f, 1.12f, HAT_CAP,      D_ANCHOR,   0.02f, {  52, 104, 142, 255 }, { 226, 220, 206, 255 }, { 156, 152, 148, 255 } },
  { "otto",     "Otto Brine",         1.06f, 1.22f, HAT_KERCHIEF, D_FISH,     0.06f, {  70, 128, 168, 255 }, { 208, 196, 172, 255 }, { 122, 104,  86, 255 } },
  { "tansy",    "Tansy",              0.66f, 0.86f, HAT_BUN,      D_FISH,     0.00f, { 214, 118, 128, 255 }, { 244, 236, 216, 255 }, { 168, 112,  62, 255 } },
  { "bruno",    "Bruno Crumb",        0.98f, 1.28f, HAT_CAP,      D_BREAD,    0.03f, { 226, 196, 152, 255 }, { 196,  86,  74, 255 }, {  86,  66,  54, 255 } },
  { "greta",    "Greta Spool",        0.97f, 1.02f, HAT_BUN,      D_SPOOL,    0.00f, { 172, 108, 168, 255 }, { 238, 224, 200, 255 }, { 108,  78,  64, 255 } },
  { "felix",    "Felix Route",        1.05f, 0.94f, HAT_CAP,      D_ENVELOPE, 0.00f, { 176,  92,  78, 255 }, { 232, 210, 160, 255 }, {  60,  52,  48, 255 } },
  { "edwina",   "Edwina Cogg",        0.92f, 0.98f, HAT_BUN,      D_CLOCK,    0.09f, { 118, 116, 142, 255 }, { 214, 176,  72, 255 }, { 202, 198, 194, 255 } },
  { "bartleby", "Bartleby Shelf",     1.12f, 0.86f, HAT_NONE,     D_BOOK,     0.11f, {  92,  84, 118, 255 }, { 208, 198, 176, 255 }, { 132, 128, 126, 255 } },
  { "petra",    "Petra Ward",         1.00f, 1.02f, HAT_CAP,      D_KEY,      0.00f, {  96, 106,  98, 255 }, { 198, 168,  96, 255 }, {  46,  40,  38, 255 } },
  { "sage",     "Sage Fern",          0.99f, 1.04f, HAT_BRIM,     D_FLOWER,   0.05f, {  98, 152, 108, 255 }, { 224, 212, 182, 255 }, { 118,  96,  70, 255 } },
  { "mo",       "Mo Hum",             1.01f, 1.08f, HAT_VEIL,     D_BEE,      0.00f, { 236, 226, 202, 255 }, { 224, 178,  62, 255 }, {  92,  76,  62, 255 } },
  { "poppy",    "Dr. Poppy Bloom",    0.96f, 0.98f, HAT_BUN,      D_TEACUP,   0.00f, { 200, 208, 214, 255 }, { 132, 168, 128, 255 }, {  70,  58,  52, 255 } },
  { "mayor",    "Mayor Aurelius Grand",1.03f,1.24f, HAT_TOP,      -1,         0.00f, {  74,  70,  92, 255 }, { 190,  78,  74, 255 }, { 178, 174, 170, 255 } },
  { "nona",     "Nona Ember",         0.88f, 1.00f, HAT_KERCHIEF, D_LANTERN,  0.14f, { 158, 122,  86, 255 }, { 226, 214, 190, 255 }, { 226, 222, 218, 255 } },
  { "wick",     "Wick",               1.08f, 1.00f, HAT_CAP,      D_LOCK,     0.07f, { 108,  98,  86, 255 }, { 176, 166, 148, 255 }, { 116, 110, 104, 255 } },
  { "iris",     "Iris Marlow",        1.00f, 0.96f, HAT_BRIM,     D_PRISM,    0.00f, { 128,  96, 176, 255 }, { 236, 208, 118, 255 }, {  58,  48,  56, 255 } },
  { "pip",      "Pip",                0.34f, 0.80f, HAT_NONE,     -1,         0.00f, {  52,  48,  58, 255 }, { 238, 236, 232, 255 }, { 236, 190,  70, 255 } },
};

#define CAST_N ((int)(sizeof g_cast / sizeof g_cast[0]))

int npc_count(void) { return CAST_N; }

int npc_by_name(const char *name)
{
    for (int i = 0; i < CAST_N; i++)
        if (strcasecmp(g_cast[i].key, name) == 0) return i;
    return -1;
}

const char *npc_display(int id)
{
    return (id >= 0 && id < CAST_N) ? g_cast[id].display : "?";
}

const char *npc_key(int id)
{
    return (id >= 0 && id < CAST_N) ? g_cast[id].key : "?";
}

int npc_emote_by_name(const char *name)
{
    static const char *n[EM_COUNT] = { "neutral", "happy", "sad", "worried",
                                       "shifty", "laughing", "moved" };
    for (int i = 0; i < EM_COUNT; i++)
        if (strcasecmp(n[i], name) == 0) return i;
    return EM_NEUTRAL;
}

/* --- the face ------------------------------------------------------------
 * Emotes are brow angle + eye shape + mouth curve. Seven states from three
 * numbers is why 18 characters can each have all seven. */
static void face(float x, float y, float r, int emote, int seed, float t)
{
    Color ink = col_ink();
    float brow = 0, mouth = 0, lid = 0, shift = 0;
    switch (emote) {
    case EM_HAPPY:   brow = -0.18f; mouth =  0.45f; break;
    case EM_SAD:     brow =  0.30f; mouth = -0.42f; lid = 0.30f; break;
    case EM_WORRIED: brow =  0.36f; mouth = -0.16f; break;
    case EM_SHIFTY:  brow = -0.10f; mouth = -0.08f; lid = 0.45f; shift = 0.28f; break;
    case EM_LAUGH:   brow = -0.26f; mouth =  0.80f; lid = 0.55f; break;
    case EM_MOVED:   brow =  0.22f; mouth =  0.24f; lid = 0.20f; break;
    default: break;
    }

    /* a blink every few seconds, never at the same instant for two people */
    float phase = fmodf(t * 0.6f + (float)(seed & 7) * 0.37f, 1.0f);
    bool blink = !settings()->reduce_motion && phase > 0.965f;

    float ex = r * 0.34f, ey = -r * 0.06f;
    for (int s = -1; s <= 1; s += 2) {
        float px = x + s * ex + shift * r * 0.5f, py = y + ey;
        if (blink || lid > 0.5f) {
            DrawLineEx((Vector2){ px - r * 0.13f, py }, (Vector2){ px + r * 0.13f, py },
                       2.2f, ink);
        } else {
            DrawCircleV((Vector2){ px, py }, r * 0.09f, ink);
            if (lid > 0.05f)
                DrawLineEx((Vector2){ px - r * 0.15f, py - r * 0.09f },
                           (Vector2){ px + r * 0.15f, py - r * 0.09f }, 2.0f, ink);
        }
        /* brow */
        float by = y - r * 0.34f + s * 0 + brow * r * 0.22f;
        Vector2 a = { px - r * 0.17f, by + brow * r * 0.20f * (float)(-s) };
        Vector2 b = { px + r * 0.17f, by - brow * r * 0.20f * (float)(-s) };
        DrawLineEx(a, b, 2.4f, ink);
    }

    /* mouth: a single arc, sampled */
    Vector2 mp[7];
    for (int i = 0; i < 7; i++) {
        float u = i / 6.0f;
        mp[i].x = x - r * 0.26f + u * r * 0.52f;
        mp[i].y = y + r * 0.34f + sinf(u * PI) * mouth * r * 0.26f;
    }
    ink_stroke(mp, 7, 2.3f, 0.35f, seed + 17, ink);
}

/* --- Pip is not a puppet; he is a bird ------------------------------------ */
static void draw_pip(float x, float y, float scale, int pose, float t)
{
    float s = scale * 46.0f;
    float hop = (pose == POSE_WALK && !settings()->reduce_motion)
                ? fabsf(sinf(t * 6.0f)) * s * 0.22f : 0.0f;
    float cy = y - s * 0.55f - hop;

    ink_blob(x, cy, s * 0.52f, 0.86f, 3301, g_cast[17].coat);          /* body  */
    ink_blob(x - s * 0.30f, cy + s * 0.05f, s * 0.30f, 0.9f, 3302, g_cast[17].trim);
    ink_blob(x + s * 0.34f, cy - s * 0.30f, s * 0.30f, 0.95f, 3303, g_cast[17].coat);
    doodle(D_FEATHER, x - s * 0.75f, cy + s * 0.10f, s * 0.5f, 0.7f, g_cast[17].coat);

    Color ink = col_ink();
    DrawCircleV((Vector2){ x + s * 0.40f, cy - s * 0.36f }, s * 0.06f, ink);
    Vector2 beak[4] = {
        { x + s * 0.56f, cy - s * 0.30f }, { x + s * 0.88f, cy - s * 0.24f },
        { x + s * 0.56f, cy - s * 0.16f }, { x + s * 0.56f, cy - s * 0.30f }
    };
    ink_fill(beak, 4, HATCH_NONE, 3304, g_cast[17].hair);
    ink_line(x - s * 0.12f, y - hop, x - s * 0.12f, y - s * 0.16f - hop, 2.4f, 0.4f, 3305, ink);
    ink_line(x + s * 0.14f, y - hop, x + s * 0.14f, y - s * 0.16f - hop, 2.4f, 0.4f, 3306, ink);
}

void npc_draw(int id, float x, float y, float scale, int pose, int emote, float t)
{
    if (id < 0 || id >= CAST_N) return;
    if (id == 17) { draw_pip(x, y, scale, pose, t); return; }
    const Puppet *P = &g_cast[id];
    if (settings()->reduce_motion) t = 1.0f;

    float H = 150.0f * scale * P->height;
    float W = 42.0f * scale * P->girth;
    int seed = id * 313 + 7;

    /* idle sway and walk bounce — small, so the town feels alive, not busy */
    float sway = sinf(t * 1.6f + id) * (pose == POSE_STAND ? 1.6f : 0.0f);
    float bounce = (pose == POSE_WALK) ? fabsf(sinf(t * 7.0f)) * H * 0.02f : 0.0f;
    float lean = P->stoop * 12.0f;
    float base = y - bounce;

    float hipY = base - H * 0.42f;
    float shY  = base - H * 0.80f;
    float headY = base - H * 0.92f;
    float headR = H * 0.10f;
    float topX = x + sway - lean;

    Color ink = col_ink();

    /* legs */
    float legSwing = (pose == POSE_WALK) ? sinf(t * 7.0f) * W * 0.42f : W * 0.16f;
    if (pose == POSE_SIT) {
        ink_line(x - W * 0.2f, hipY, x + W * 0.7f, hipY + H * 0.16f, 5.0f, 0.6f, seed, ink);
        ink_line(x + W * 0.7f, hipY + H * 0.16f, x + W * 0.7f, base, 5.0f, 0.6f, seed + 1, ink);
    } else {
        ink_line(x - W * 0.18f, hipY, x - legSwing * 0.5f, base, 5.0f, 0.6f, seed, ink);
        ink_line(x + W * 0.18f, hipY, x + legSwing * 0.5f, base, 5.0f, 0.6f, seed + 1, ink);
    }

    /* coat: a tapering blob from shoulders to hem */
    Vector2 coat[12];
    coat[0]  = (Vector2){ topX - W * 0.52f, shY + H * 0.03f };
    coat[1]  = (Vector2){ topX - W * 0.60f, shY + H * 0.16f };
    coat[2]  = (Vector2){ x - W * 0.66f, hipY + H * 0.06f };
    coat[3]  = (Vector2){ x - W * 0.58f, hipY + H * 0.12f };
    coat[4]  = (Vector2){ x,             hipY + H * 0.15f };
    coat[5]  = (Vector2){ x + W * 0.58f, hipY + H * 0.12f };
    coat[6]  = (Vector2){ x + W * 0.66f, hipY + H * 0.06f };
    coat[7]  = (Vector2){ topX + W * 0.60f, shY + H * 0.16f };
    coat[8]  = (Vector2){ topX + W * 0.52f, shY + H * 0.03f };
    coat[9]  = (Vector2){ topX + W * 0.22f, shY - H * 0.02f };
    coat[10] = (Vector2){ topX - W * 0.22f, shY - H * 0.02f };
    coat[11] = coat[0];
    ink_fill(coat, 11, HATCH_NONE, seed + 2, P->coat);
    ink_stroke(coat, 12, 2.4f, 0.8f, seed + 3, ink);

    /* trim: collar and hem, the second colour slot */
    ink_line(topX - W * 0.24f, shY + H * 0.01f, topX + W * 0.24f, shY + H * 0.01f,
             4.5f, 0.6f, seed + 4, P->trim);
    ink_line(x - W * 0.60f, hipY + H * 0.11f, x + W * 0.60f, hipY + H * 0.11f,
             3.4f, 0.9f, seed + 5, P->trim);

    /* arms */
    float armSwing = (pose == POSE_WALK) ? -sinf(t * 7.0f) * W * 0.30f : 0.0f;
    float handY = hipY + H * 0.02f;
    float lhx = topX - W * 0.72f - armSwing;
    float rhx = topX + W * 0.72f + armSwing;
    if (pose == POSE_POINT) { rhx = topX + W * 1.15f; handY = shY + H * 0.02f; }
    ink_line(topX - W * 0.48f, shY + H * 0.05f, lhx, handY, 4.6f, 0.7f, seed + 6, ink);
    ink_line(topX + W * 0.48f, shY + H * 0.05f, rhx, handY, 4.6f, 0.7f, seed + 7, ink);

    /* prop, held in the right hand */
    if (P->prop >= 0 && pose != POSE_SIT)
        doodle(P->prop, rhx + W * 0.16f, handY + H * 0.03f, H * 0.10f,
               sinf(t * 1.1f + id) * 0.08f, P->trim);

    /* neck + head */
    ink_line(topX, shY, topX, headY + headR * 0.7f, 4.0f, 0.4f, seed + 8, ink);
    ink_blob(topX, headY, headR, 1.06f, seed + 9, (Color){ 236, 216, 194, 255 });
    ink_circle(topX, headY, headR, 2.2f, 0.6f, seed + 10, ink);
    face(topX, headY, headR, emote, seed, t);

    /* hair + headwear */
    switch (P->hat) {
    case HAT_CAP:
        ink_blob(topX, headY - headR * 0.72f, headR * 0.98f, 0.55f, seed + 11, P->coat);
        ink_line(topX - headR * 0.2f, headY - headR * 0.72f,
                 topX + headR * 1.5f, headY - headR * 0.60f, 4.5f, 0.5f, seed + 12, P->coat);
        break;
    case HAT_BRIM:
        ink_blob(topX, headY - headR * 0.95f, headR * 0.80f, 0.85f, seed + 11, P->trim);
        ink_blob(topX, headY - headR * 0.55f, headR * 1.75f, 0.22f, seed + 12, P->trim);
        break;
    case HAT_TALL:
        ink_blob(topX, headY - headR * 0.68f, headR * 0.95f, 0.62f, seed + 11, P->coat);
        ink_line(topX - headR * 1.3f, headY - headR * 0.60f,
                 topX + headR * 1.3f, headY - headR * 0.60f, 5.0f, 0.5f, seed + 12, P->coat);
        break;
    case HAT_KERCHIEF: {
        Vector2 k[4] = {
            { topX - headR * 1.05f, headY - headR * 0.30f },
            { topX,                 headY - headR * 1.25f },
            { topX + headR * 1.05f, headY - headR * 0.30f },
            { topX,                 headY - headR * 0.55f }
        };
        ink_fill(k, 4, HATCH_DOT, seed + 11, P->trim);
        break;
    }
    case HAT_BUN:
        ink_blob(topX, headY - headR * 0.62f, headR * 1.02f, 0.72f, seed + 11, P->hair);
        ink_blob(topX - headR * 1.02f, headY - headR * 0.72f, headR * 0.42f, 1.0f,
                 seed + 12, P->hair);
        break;
    case HAT_TOP:
        ink_blob(topX, headY - headR * 1.35f, headR * 0.82f, 1.05f, seed + 11, P->coat);
        ink_line(topX - headR * 1.5f, headY - headR * 0.72f,
                 topX + headR * 1.5f, headY - headR * 0.72f, 5.5f, 0.4f, seed + 12, P->coat);
        break;
    case HAT_VEIL: {
        Color veil = { 240, 244, 248, 130 };
        ink_blob(topX, headY - headR * 0.30f, headR * 1.45f, 1.10f, seed + 11, veil);
        ink_circle(topX, headY - headR * 0.30f, headR * 1.45f, 2.0f, 0.5f, seed + 12, ink);
        break;
    }
    default:
        ink_blob(topX, headY - headR * 0.58f, headR * 1.00f, 0.60f, seed + 11, P->hair);
        break;
    }
}

void npc_bust(int id, Rectangle r, int emote, float t)
{
    if (id < 0 || id >= CAST_N) return;
    /* The same puppet, framed at the shoulders. The frame is derived from
     * THIS character's height, not a fixed one: a nine-year-old and a
     * librarian are different lengths, and a fixed crop put the short ones
     * entirely below the portrait — an empty box where Tansy should be. */
    float h = g_cast[id].height;
    if (h < 0.2f) h = 0.2f;
    float scale = r.height / (45.0f * h);
    float body = 150.0f * scale * h;
    float feet = r.y + r.height + body * 0.68f;

    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    npc_draw(id, r.x + r.width * 0.5f, feet, scale, POSE_STAND, emote, t);
    EndScissorMode();
}
