/* Scores + config, flat files beside the binary (§10, §4.1). */
#include "game.h"

#define SCORES_FILE "gauntlet.scores"
#define CONFIG_FILE "gauntlet.cfg"
#define SCORES_MAGIC 0x474E5431u   /* "GNT1" */

/* fixed 16-byte records */
typedef struct {
    char     initials[3];
    uint8_t  tier;
    int32_t  score;
    uint32_t seed;
    uint32_t pad;
} DiskRow;

typedef struct {
    uint32_t magic;
    uint32_t party_seed;
    uint32_t count;
    uint32_t pad;
} DiskHeader;

void persist_load_scores(ScoreRow *rows, int *count, uint32_t *party_seed)
{
    *count = 0;
    FILE *f = fopen(SCORES_FILE, "rb");
    if (!f) return;
    DiskHeader h;
    if (fread(&h, sizeof h, 1, f) != 1 || h.magic != SCORES_MAGIC) { fclose(f); return; }
    if (party_seed) *party_seed = h.party_seed;
    int n = (int)h.count;
    if (n > LEADERBOARD_N) n = LEADERBOARD_N;
    for (int i = 0; i < n; i++) {
        DiskRow r;
        if (fread(&r, sizeof r, 1, f) != 1) break;
        memcpy(rows[i].initials, r.initials, 3);
        rows[i].initials[3] = 0;
        rows[i].score = r.score;
        rows[i].tier = r.tier;
        rows[i].seed = r.seed;
        (*count)++;
    }
    fclose(f);
}

void persist_save_scores(const ScoreRow *rows, int count, uint32_t party_seed)
{
    FILE *f = fopen(SCORES_FILE, "wb");
    if (!f) return;
    DiskHeader h = { SCORES_MAGIC, party_seed, (uint32_t)g_clampi(count, 0, LEADERBOARD_N), 0 };
    fwrite(&h, sizeof h, 1, f);
    for (int i = 0; i < (int)h.count; i++) {
        DiskRow r;
        memset(&r, 0, sizeof r);
        memcpy(r.initials, rows[i].initials, 3);
        r.tier = rows[i].tier;
        r.score = rows[i].score;
        r.seed = rows[i].seed;
        fwrite(&r, sizeof r, 1, f);
    }
    fclose(f);
}

void persist_load_config(float *calib_ms)
{
    FILE *f = fopen(CONFIG_FILE, "rb");
    if (!f) return;
    float v = 0.0f;
    if (fread(&v, sizeof v, 1, f) == 1 && v > -200.0f && v < 200.0f) *calib_ms = v;
    fclose(f);
}

void persist_save_config(float calib_ms)
{
    FILE *f = fopen(CONFIG_FILE, "wb");
    if (!f) return;
    fwrite(&calib_ms, sizeof calib_ms, 1, f);
    fclose(f);
}
