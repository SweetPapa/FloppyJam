/* app — the state machine (API.md §7.3).
 *
 * BOOT / TITLE / TOWN / PUZZLE / BOARD / CUTSCENE / JOURNAL, plus the two
 * screens the mom test cares about: name entry and the recap on load.
 */
#ifndef HD_APP_H
#define HD_APP_H

#include <stdbool.h>

void app_init(void);
void app_frame(float dt);
void app_shutdown(void);
bool app_wants_quit(void);

/* The capture harness every integration card asks for (CANON 13, 15): jump
 * straight to one screen so it can be photographed. Spec is one of
 *   title | scene:<id> | board:<id> | puzzle:<id> | cut:<id>
 * Returns false if the spec names something that is not there. */
bool app_debug_goto(const char *spec);

#endif
