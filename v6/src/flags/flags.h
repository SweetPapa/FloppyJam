/* flags — the only cross-system state (API.md §7.3).
 *
 * String-keyed ints, namespaced: "ch1.otto.trust", "clue.midnight_lantern".
 * Dialogue, boards, cutscenes and scenes read/write the world exclusively
 * through here, which is what makes saves forward-compatible with content:
 * a save is the flag store and nothing else.
 */
#ifndef HD_FLAGS_H
#define HD_FLAGS_H

#include <stdbool.h>

#define FLAG_MAX_KEY 48
#define FLAG_CAP     2048

void flags_reset(void);

int  flag_get(const char *name);
void flag_set(const char *name, int value);
void flag_add(const char *name, int delta);

/* clue tokens are flags under "clue."; grant is idempotent and stamps the
 * order of discovery so the journal can say where it came from. */
void clue_grant(const char *id);
bool clue_has(const char *id);
int  clue_count(void);
/* iterate granted clues in discovery order; returns id or NULL past the end */
const char *clue_at(int index);

void trust_add(const char *npc, int n);
int  trust_get(const char *npc);

/* raw iteration, used by save/ and the tests */
int         flags_count(void);
const char *flags_key_at(int i);
int         flags_value_at(int i);

#endif
