/* save.c — persistent progress (stars per level). Pure stdio, no deps. */
#include "maglava.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SAVE_MAGIC 0x4D4C4756u /* 'MLGV' */
#define SAVE_VER   1

static void save_path(char *buf, size_t n) {
    const char *dir = getenv("HOME");
#ifdef _WIN32
    if (!dir) dir = getenv("APPDATA");
    if (!dir) dir = getenv("USERPROFILE");
#endif
    if (dir && *dir)
        snprintf(buf, n, "%s/.maglava_save.dat", dir);
    else
        snprintf(buf, n, "maglava_save.dat");
}

void save_load(SaveData *s) {
    memset(s, 0, sizeof(*s));
    s->unlocked = 1; /* level 1 always available */
    char path[1024];
    save_path(path, sizeof path);
    FILE *fp = fopen(path, "rb");
    if (!fp) return;
    unsigned int magic = 0, ver = 0;
    if (fread(&magic, sizeof magic, 1, fp) == 1 && magic == SAVE_MAGIC &&
        fread(&ver, sizeof ver, 1, fp) == 1 && ver == SAVE_VER) {
        if (fread(s->stars, 1, LEVEL_COUNT, fp) != (size_t)LEVEL_COUNT) { /* ignore */ }
        if (fread(&s->unlocked, sizeof s->unlocked, 1, fp) != 1) s->unlocked = 1;
    }
    fclose(fp);
    if (s->unlocked < 1) s->unlocked = 1;
    if (s->unlocked > LEVEL_COUNT) s->unlocked = LEVEL_COUNT;
}

void save_store(const SaveData *s) {
    char path[1024];
    save_path(path, sizeof path);
    FILE *fp = fopen(path, "wb");
    if (!fp) return;
    unsigned int magic = SAVE_MAGIC, ver = SAVE_VER;
    fwrite(&magic, sizeof magic, 1, fp);
    fwrite(&ver, sizeof ver, 1, fp);
    fwrite(s->stars, 1, LEVEL_COUNT, fp);
    fwrite(&s->unlocked, sizeof s->unlocked, 1, fp);
    fclose(fp);
}

void save_record(SaveData *s, int level_id, int stars) {
    if (level_id < 1 || level_id > LEVEL_COUNT) return;
    int i = level_id - 1;
    if (stars > s->stars[i]) s->stars[i] = (unsigned char)stars;
    if (level_id + 1 > s->unlocked && level_id < LEVEL_COUNT)
        s->unlocked = level_id + 1;
    save_store(s);
}
