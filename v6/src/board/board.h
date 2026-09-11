/* board — the Case Board interpreter (API.md §9.2, CANON §3.3).
 *
 * The deduction spine. The board never explains the mystery; it holds the
 * blanks and tells the player how many they have right. The player does the
 * detecting (§1.2.4).
 */
#ifndef HD_BOARD_H
#define HD_BOARD_H

#include <stdbool.h>

typedef enum { BOARD_RUNNING = 0, BOARD_SOLVED, BOARD_EXITED } board_status;

bool         board_start(const char *id);
board_status board_update(float dt);
void         board_draw(void);

/* what the board asked for on solve: "cut" or "goto" plus an id */
const char *board_solved_kind(void);
const char *board_solved_id(void);
const char *board_id(void);

/* the journal shows past boards read-only */
bool board_is_solved(const char *id);
/* Shared by mouse input and campaign regression tests. No drawing required. */
bool board_place(const char *blank, const char *token);
int board_deduce(void); /* -1 incomplete/not enough evidence; otherwise correct count */
int board_blank_count(void);
int board_pool_count(void);
int board_line_count(void);
bool board_layout_fits(void); /* valid after drawing; capture regression check */

#endif
