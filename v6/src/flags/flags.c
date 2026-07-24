#include "flags.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    char key[FLAG_MAX_KEY];
    int  value;
    int  stamp;          /* discovery order, 0 = never granted */
} FlagEntry;

static FlagEntry g_flags[FLAG_CAP];
static int       g_count;
static int       g_stamp;

/* clue order index into g_flags */
static int g_clue_order[FLAG_CAP];
static int g_clue_n;

void flags_reset(void)
{
    memset(g_flags, 0, sizeof g_flags);
    g_count = 0;
    g_stamp = 0;
    g_clue_n = 0;
}

/* Linear-probed open addressing. Keys are short and the table is tiny;
 * this is measurably faster than anything cleverer at this size. */
static unsigned key_hash(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static int slot_find(const char *name, bool create)
{
    unsigned h = key_hash(name) & (FLAG_CAP - 1);
    for (int probe = 0; probe < FLAG_CAP; probe++) {
        int i = (int)((h + (unsigned)probe) & (FLAG_CAP - 1));
        if (g_flags[i].key[0] == 0) {
            if (!create) return -1;
            if (g_count >= FLAG_CAP - 8) return -1;   /* full: refuse, never wrap */
            snprintf(g_flags[i].key, FLAG_MAX_KEY, "%s", name);
            g_flags[i].value = 0;
            g_flags[i].stamp = 0;
            g_count++;
            return i;
        }
        if (strcmp(g_flags[i].key, name) == 0) return i;
    }
    return -1;
}

int flag_get(const char *name)
{
    int i = slot_find(name, false);
    return i < 0 ? 0 : g_flags[i].value;
}

void flag_set(const char *name, int value)
{
    int i = slot_find(name, true);
    if (i >= 0) g_flags[i].value = value;
}

void flag_add(const char *name, int delta)
{
    int i = slot_find(name, true);
    if (i >= 0) g_flags[i].value += delta;
}

void clue_grant(const char *id)
{
    int i = slot_find(id, true);
    if (i < 0 || g_flags[i].value != 0) return;     /* idempotent */
    g_flags[i].value = 1;
    g_flags[i].stamp = ++g_stamp;
    if (g_clue_n < FLAG_CAP) g_clue_order[g_clue_n++] = i;
}

bool clue_has(const char *id) { return flag_get(id) != 0; }

int clue_count(void) { return g_clue_n; }

const char *clue_at(int index)
{
    if (index < 0 || index >= g_clue_n) return 0;
    return g_flags[g_clue_order[index]].key;
}

void trust_add(const char *npc, int n)
{
    char key[FLAG_MAX_KEY];
    snprintf(key, sizeof key, "trust.%s", npc);
    flag_add(key, n);
}

int trust_get(const char *npc)
{
    char key[FLAG_MAX_KEY];
    snprintf(key, sizeof key, "trust.%s", npc);
    return flag_get(key);
}

int flags_count(void) { return g_count; }

const char *flags_key_at(int i)
{
    int seen = 0;
    for (int s = 0; s < FLAG_CAP; s++) {
        if (g_flags[s].key[0] == 0) continue;
        if (seen++ == i) return g_flags[s].key;
    }
    return 0;
}

int flags_value_at(int i)
{
    int seen = 0;
    for (int s = 0; s < FLAG_CAP; s++) {
        if (g_flags[s].key[0] == 0) continue;
        if (seen++ == i) return g_flags[s].value;
    }
    return 0;
}
