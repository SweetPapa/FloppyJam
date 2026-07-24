/* cut — the cutscene interpreter (API.md §9.3).
 *
 * A deliberately small verb set. If a cutscene needs a new verb, that is an
 * artkit or cut-engine job card, not an inline hack.
 */
#ifndef HD_CUT_H
#define HD_CUT_H

#include <stdbool.h>

bool cut_play(const char *id);
/* true when the cutscene is over */
bool cut_update(float dt);
void cut_draw(void);
bool cut_active(void);
void cut_skip(void);

#endif
