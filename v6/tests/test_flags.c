/* §10.2 gate: the flag store and the clue journal.
 *
 * The flag store is the only cross-system state in HUEDUNIT, so it is the
 * only thing in the engine that can quietly corrupt a playthrough.
 */
#include "flags/flags.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(bool ok, const char *what)
{
    if (ok) { printf("  ok    %s\n", what); return; }
    printf("  FAIL  %s\n", what);
    g_fail++;
}

int main(void)
{
    printf("test_flags:\n");
    flags_reset();

    expect(flag_get("nothing.at.all") == 0, "unset flags read as zero");

    flag_set("ch1.met_otto", 1);
    expect(flag_get("ch1.met_otto") == 1, "set then get");
    flag_add("ch1.met_otto", 4);
    expect(flag_get("ch1.met_otto") == 5, "add accumulates");
    flag_set("ch1.met_otto", 0);
    expect(flag_get("ch1.met_otto") == 0, "a flag can go back to zero");

    /* namespacing must not collide: these differ only after the dot */
    flag_set("ch1.trust", 3);
    flag_set("ch2.trust", 7);
    expect(flag_get("ch1.trust") == 3 && flag_get("ch2.trust") == 7,
           "namespaced keys do not collide");

    /* clues: idempotent, ordered, countable */
    flags_reset();
    clue_grant("clue.otto_alibi");
    clue_grant("clue.tansy_saw");
    clue_grant("clue.otto_alibi");
    expect(clue_count() == 2, "granting the same clue twice grants it once");
    expect(clue_has("clue.otto_alibi") && clue_has("clue.tansy_saw"),
           "granted clues read back");
    expect(!clue_has("clue.never_granted"), "ungranted clues read false");
    expect(strcmp(clue_at(0), "clue.otto_alibi") == 0 &&
           strcmp(clue_at(1), "clue.tansy_saw") == 0,
           "clues keep discovery order for the journal");
    expect(clue_at(2) == 0 && clue_at(-1) == 0, "clue_at is bounded");

    trust_add("otto", 2);
    trust_add("otto", 1);
    expect(trust_get("otto") == 3, "trust accumulates per character");
    expect(trust_get("nobody") == 0, "trust of a stranger is zero");
    expect(flag_get("trust.otto") == 3, "trust lives in the flag store");

    /* a save is "every non-zero flag by name", so iteration must be complete */
    flags_reset();
    for (int i = 0; i < 200; i++) {
        char k[32];
        snprintf(k, sizeof k, "bulk.key%03d", i);
        flag_set(k, i + 1);
    }
    int seen = 0;
    for (int i = 0; i < flags_count(); i++) {
        const char *k = flags_key_at(i);
        if (k && strncmp(k, "bulk.key", 8) == 0) {
            int idx;
            if (sscanf(k, "bulk.key%d", &idx) == 1 && flags_value_at(i) == idx + 1)
                seen++;
        }
    }
    expect(seen == 200, "every stored flag is reachable by iteration");

    /* the store must refuse to wrap rather than overwrite somebody's chapter */
    flags_reset();
    for (int i = 0; i < FLAG_CAP + 500; i++) {
        char k[32];
        snprintf(k, sizeof k, "flood%05d", i);
        flag_set(k, 1);
    }
    expect(flags_count() <= FLAG_CAP, "the store never exceeds its capacity");
    expect(flag_get("flood00000") == 1, "an early flag survives a flood");

    if (g_fail) { printf("test_flags: %d failure(s)\n", g_fail); return 1; }
    printf("test_flags: all green\n");
    return 0;
}
