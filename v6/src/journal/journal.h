/* journal — the Case Journal book UI (CANON §5.3).
 *
 * Four tabs, auto-written, zero management burden. It READS the flag store
 * and writes nothing back, which is why a player can open it at any moment
 * without changing the state of the mystery.
 */
#ifndef HD_JOURNAL_H
#define HD_JOURNAL_H

#include <stdbool.h>

void journal_open(void);
void journal_close(void);
bool journal_is_open(void);
/* returns true when the player closed the book */
bool journal_update(float dt);
void journal_draw(void);

/* the Town tab offers puzzle replays; non-empty means "run this for fun" */
const char *journal_replay_request(void);
void        journal_clear_replay(void);

#endif
