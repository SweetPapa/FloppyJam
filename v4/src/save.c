/* Versioned desktop progress. Stable level keys survive campaign reordering. */
#include "maglava.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#define SAVE_MAGIC 0x4D4C4756u
#define SAVE_VER 2u
#define KEY_SIZE 48

static void save_path(char *buf, size_t n, int legacy) {
    const char *override = getenv("MAGLAVA_SAVE_PATH");
    if (override && *override) { snprintf(buf, n, "%s", override); return; }
    const char *dir = getenv("HOME");
#ifdef _WIN32
    dir = getenv("APPDATA");
    if (!dir) dir = getenv("USERPROFILE");
#endif
    snprintf(buf, n, "%s/%s", dir && *dir ? dir : ".",
             legacy ? ".maglava_save.dat" : ".maglava_desktop.dat");
}

void save_load(SaveData *s) {
    memset(s, 0, sizeof *s);
    s->unlocked = 1;
    char path[1024]; save_path(path, sizeof path, 0);
    FILE *fp = fopen(path, "rb");
    if (!fp && !getenv("MAGLAVA_SAVE_PATH")) {
        save_path(path, sizeof path, 1); fp = fopen(path, "rb");
    }
    if (!fp) return;
    SaveData tmp = {0}; tmp.unlocked = 1;
    uint32_t magic, ver;
    if (fread(&magic, 4, 1, fp) != 1 || magic != SAVE_MAGIC ||
        fread(&ver, 4, 1, fp) != 1) goto done;
    if (ver == 1) {
        unsigned char stars[25]; int32_t unlocked;
        if (fread(stars, 1, 25, fp) != 25 || fread(&unlocked, 4, 1, fp) != 1) goto done;
        if (unlocked < 1 || unlocked > 25) goto done;
        for (int i = 0; i < LEVEL_COUNT; i++) {
            int old = LEVELS[i].legacy_id;
            if (old > 0 && old <= 25) {
                if (stars[old - 1] > 3) goto done;
                tmp.stars[i] = stars[old - 1];
                if (old <= unlocked) tmp.unlocked = i + 1;
            }
        }
    } else if (ver == SAVE_VER) {
        uint32_t count, flags;
        if (fread(&count, 4, 1, fp) != 1 || count > 256 ||
            fread(&flags, 4, 1, fp) != 1) goto done;
        tmp.reduced_motion = !!(flags & 1); tmp.muted = !!(flags & 2);
        for (uint32_t j = 0; j < count; j++) {
            char key[KEY_SIZE]; unsigned char star, unlocked; float time; int32_t score;
            if (fread(key, 1, KEY_SIZE, fp) != KEY_SIZE ||
                fread(&star, 1, 1, fp) != 1 || fread(&unlocked, 1, 1, fp) != 1 ||
                fread(&time, 4, 1, fp) != 1 || fread(&score, 4, 1, fp) != 1) goto done;
            if (!memchr(key, 0, KEY_SIZE) || star > 3 || unlocked > 1 ||
                !isfinite(time) || time < 0 || score < 0) goto done;
            for (int i = 0; i < LEVEL_COUNT; i++) if (!strcmp(key, LEVELS[i].key)) {
                tmp.stars[i] = star; tmp.best_time[i] = time; tmp.best_score[i] = score;
                if (unlocked && i + 1 > tmp.unlocked) tmp.unlocked = i + 1;
            }
        }
    } else goto done;
    *s = tmp; /* truncated or invalid files never partially overwrite defaults */
done:
    fclose(fp);
}

void save_store(const SaveData *s) {
    char path[1024], temp[1040]; save_path(path, sizeof path, 0);
    snprintf(temp, sizeof temp, "%s.tmp", path);
    FILE *fp = fopen(temp, "wb");
    if (!fp) { fprintf(stderr, "Could not save progress to %s\n", path); return; }
    uint32_t magic = SAVE_MAGIC, ver = SAVE_VER, count = LEVEL_COUNT;
    uint32_t flags = (s->reduced_motion ? 1u : 0u) | (s->muted ? 2u : 0u);
    int ok = fwrite(&magic,4,1,fp)==1 && fwrite(&ver,4,1,fp)==1 &&
             fwrite(&count,4,1,fp)==1 && fwrite(&flags,4,1,fp)==1;
    for (int i = 0; i < LEVEL_COUNT && ok; i++) {
        char key[KEY_SIZE] = {0}; snprintf(key, sizeof key, "%s", LEVELS[i].key);
        unsigned char unlocked = i < s->unlocked;
        int32_t score = s->best_score[i];
        ok = fwrite(key,1,KEY_SIZE,fp)==KEY_SIZE && fwrite(&s->stars[i],1,1,fp)==1 &&
             fwrite(&unlocked,1,1,fp)==1 && fwrite(&s->best_time[i],4,1,fp)==1 &&
             fwrite(&score,4,1,fp)==1;
    }
    if (fclose(fp) != 0) ok = 0;
#ifdef _WIN32
    /* The Windows CRT cannot replace an existing path with rename. */
    if (ok) {
        extern int save_replace_windows(const char *, const char *);
        ok = save_replace_windows(temp, path);
    }
#else
    if (ok) ok = rename(temp, path) == 0;
#endif
    if (!ok) { remove(temp); fprintf(stderr, "Could not commit progress to %s\n", path); }
}
#ifdef _WIN32
#include <windows.h>
int save_replace_windows(const char *temp, const char *path) {
    return MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
#endif

void save_record(SaveData *s, int level_id, int stars) {
    if (level_id < 1 || level_id > LEVEL_COUNT || stars < 1 || stars > 3) return;
    int i = level_id - 1;
    if (stars > s->stars[i]) s->stars[i] = (unsigned char)stars;
    if (level_id + 1 > s->unlocked && level_id < LEVEL_COUNT) s->unlocked = level_id + 1;
    save_store(s);
}

void save_result(SaveData *s, const GameSim *g) {
    if (!g->won || g->level_id < 1 || g->level_id > LEVEL_COUNT) return;
    int i = g->level_id - 1;
    if (s->best_time[i] == 0 || g->elapsed < s->best_time[i]) s->best_time[i] = g->elapsed;
    if (g->score > s->best_score[i]) s->best_score[i] = g->score;
    save_record(s, g->level_id, sim_stars(g));
}
