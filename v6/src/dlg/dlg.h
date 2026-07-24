/* dlg — the dialogue interpreter (API.md §9.1).
 *
 * Knows HOW to run a conversation. Never knows WHAT any conversation says.
 */
#ifndef HD_DLG_H
#define HD_DLG_H

#include <stdbool.h>

typedef enum {
    DLG_RUNNING = 0,
    DLG_DONE,
    DLG_WANT_PUZZLE,     /* start_puzzle — app runs it, then dlg_resume() */
    DLG_WANT_CUT,        /* play_cut                                      */
    DLG_WANT_BOARD       /* open_board                                    */
} dlg_status;

bool        dlg_start(const char *node_id);
dlg_status  dlg_update(float dt);
void        dlg_draw(void);
void        dlg_resume(void);
const char *dlg_request(void);      /* id that came with WANT_* */
bool        dlg_active(void);
/* the last speaker, so the town can react (portraits, ducking, blips) */
int         dlg_speaker(void);

#endif
