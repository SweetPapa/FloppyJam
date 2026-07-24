#include "save.h"
#include "flags/flags.h"
#include "art/artkit.h"

#include <stdio.h>
#include <string.h>

static Settings g_set;
static char     g_last_scene[64] = "p_gate";

Settings *settings(void) { return &g_set; }

float text_scale(void)
{
    switch (g_set.text_size) {
    case 1:  return 1.25f;
    case 2:  return 1.50f;
    default: return 1.00f;
    }
}

void settings_defaults(void)
{
    memset(&g_set, 0, sizeof g_set);
    g_set.text_size = 0;
    g_set.highlight = 1;          /* §1.2.1: interactable-highlight default ON */
    g_set.music_vol = 6;
    g_set.sfx_vol = 8;
    g_set.fullscreen = 0;
    g_set.reduce_motion = 0;
    snprintf(g_set.detective, sizeof g_set.detective, "%s", "Quill");
}

void settings_load(void)
{
    settings_defaults();
    FILE *f = fopen("huedunit.cfg", "rb");
    if (!f) return;
    char key[64];
    int v;
    while (fscanf(f, "%63s %d", key, &v) == 2) {
        if (!strcmp(key, "text"))       g_set.text_size = v;
        else if (!strcmp(key, "hilite")) g_set.highlight = v;
        else if (!strcmp(key, "music"))  g_set.music_vol = v;
        else if (!strcmp(key, "sfx"))    g_set.sfx_vol = v;
        else if (!strcmp(key, "full"))   g_set.fullscreen = v;
        else if (!strcmp(key, "motion")) g_set.reduce_motion = v;
        else if (!strcmp(key, "name")) {
            /* the value was a placeholder; the name follows on its own line */
            if (fscanf(f, "%23s", g_set.detective) != 1)
                snprintf(g_set.detective, sizeof g_set.detective, "%s", "Quill");
        }
    }
    fclose(f);
}

void settings_store(void)
{
    FILE *f = fopen("huedunit.cfg", "wb");
    if (!f) return;
    fprintf(f, "text %d\nhilite %d\nmusic %d\nsfx %d\nfull %d\nmotion %d\nname 0\n%s\n",
            g_set.text_size, g_set.highlight, g_set.music_vol, g_set.sfx_vol,
            g_set.fullscreen, g_set.reduce_motion, g_set.detective);
    fclose(f);
}

static void slot_path(int slot, char *buf, int cap)
{
    snprintf(buf, (size_t)cap, "huedunit%d.sav", slot);
}

/* The save is "every non-zero flag, by name". No layout, no offsets, so a
 * save written before a chapter existed still loads after it ships. */
bool save_write(int slot, const char *scene_id, const char *recap)
{
    char path[64];
    slot_path(slot, path, sizeof path);
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    fprintf(f, "HUEDUNIT %d\n", SAVE_VERSION);
    fprintf(f, "scene %s\n", scene_id ? scene_id : g_last_scene);
    fprintf(f, "stage %d\n", palette_stage());
    fprintf(f, "recap %s\n", recap ? recap : "");
    int n = flags_count();
    for (int i = 0; i < n; i++) {
        int v = flags_value_at(i);
        if (v == 0) continue;
        fprintf(f, "f %s %d\n", flags_key_at(i), v);
    }
    fclose(f);
    return true;
}

bool save_read(int slot)
{
    char path[64];
    slot_path(slot, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char line[256];
    int ver = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "HUEDUNIT %d", &ver) != 1 ||
        ver > SAVE_VERSION) {
        fclose(f);
        return false;
    }
    flags_reset();
    int stage = 0;
    while (fgets(line, sizeof line, f)) {
        char key[FLAG_MAX_KEY];
        int v;
        if (sscanf(line, "f %47s %d", key, &v) == 2) {
            if (strncmp(key, "clue.", 5) == 0) clue_grant(key);
            else flag_set(key, v);
        } else if (sscanf(line, "scene %63s", g_last_scene) == 1) {
            continue;
        } else if (sscanf(line, "stage %d", &stage) == 1) {
            palette_set_stage(stage);
        }
    }
    fclose(f);
    return true;
}

bool save_exists(int slot)
{
    char path[64];
    slot_path(slot, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool save_peek(int slot, char *chapter_out, int cap, char *recap_out, int rcap)
{
    char path[64];
    slot_path(slot, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[256];
    if (chapter_out) snprintf(chapter_out, (size_t)cap, "%s", "Prismbrook");
    if (recap_out) recap_out[0] = 0;
    int stage = 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "stage %d", &stage) == 1) {
            static const char *names[7] = { "Prologue", "Chapter One", "Chapter Two",
                                            "Chapter Three", "Chapter Four",
                                            "Chapter Five", "Finale" };
            if (chapter_out) snprintf(chapter_out, (size_t)cap, "%s", names[stage % 7]);
        } else if (strncmp(line, "recap ", 6) == 0 && recap_out) {
            snprintf(recap_out, (size_t)rcap, "%s", line + 6);
            int n = (int)strlen(recap_out);
            while (n && (recap_out[n - 1] == '\n' || recap_out[n - 1] == '\r'))
                recap_out[--n] = 0;
        }
    }
    fclose(f);
    return true;
}

const char *save_last_scene(void) { return g_last_scene; }

void save_autosave(const char *scene_id)
{
    if (scene_id) snprintf(g_last_scene, sizeof g_last_scene, "%s", scene_id);
    char recap[512];
    save_build_recap(recap, sizeof recap);
    save_write(0, g_last_scene, recap);
}

/* "Previously, in Prismbrook..." — read off the flag store, so it is true
 * for any save any build ever wrote (§1.2.1). */
void save_build_recap(char *buf, int cap)
{
    int stage = palette_stage();
    int clues = clue_count();
    int feathers = flag_get("feathers");

    static const char *beat[7] = {
        "The town woke gray. The Prism is gone, the Tinter is gone, and the "
        "only witness is a magpie who will not come down.",
        "The harbor has its blue back. Otto Brine was at the tower at midnight, "
        "and he was not stealing anything.",
        "Market Row is yellow again. The Prismworks was not forced: it was "
        "opened, carefully, by someone with Petra's picks.",
        "The Old Quarter is red again. Iris Marlow ordered repair braces for a "
        "crystal the whole town believed was fine.",
        "The Gardens are green again. The Festival of Lanterns stopped three "
        "years ago, and everyone had a reason they were ashamed of.",
        "Tower Green is violet again. Nona's key is found, Pip trusts you, and "
        "the stair to the lantern room is finally open.",
        "The Prism is mended and Prismbrook is in full colour."
    };

    snprintf(buf, (size_t)cap,
             "%s  You are carrying %d clue%s and %d feather%s.",
             beat[stage < 0 ? 0 : (stage > 6 ? 6 : stage)],
             clues, clues == 1 ? "" : "s",
             feathers, feathers == 1 ? "" : "s");
}
