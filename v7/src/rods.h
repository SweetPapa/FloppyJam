/* rods.h — Section 3. Rod state, the two control schemes, and the strike
 * vocabulary. Everything a player can do to a ball is decided here; sim.c
 * only decides where the ball then goes.
 *
 * Raylib-free.
 */
#ifndef VB_RODS_H
#define VB_RODS_H

#include "sim.h"

/* Lay out the six rods, interleaved foosball-style, and apply Table Tilt. */
void vb_rods_init(VbSim *s);

/* One tick of input: rod motion, scheme logic, state machines, pin clocks.
 * Called by vb_sim_step before the ball moves. */
void vb_rods_update(VbSim *s, const VbInput in[VB_NPLAYERS]);

/* The moment of contact. Called by sim.c with the surface normal pointing out
 * of the paddle. Returns 1 if the ball is now pinned (sim.c then leaves that
 * ball alone until it is released). */
int  vb_rod_contact(VbSim *s, int rod, int pad, int ball, V2 n);

/* Drop every pin — a goal, a dead ball, a mode change. */
void vb_rods_release_all(VbSim *s);

/* GLIDE angle assist and the no-own-goal rule (§3.1, §5.5). Exposed for the
 * tests and for the aim wedge the UI draws while pinned. */
V2   vb_assist_dir(const VbSim *s, int rod, int ball, V2 dir, float strength);

/* Which player slot is steering this rod right now (GRIP active-rod display,
 * and the AI's honesty audit). */
int  vb_rod_is_active(const VbSim *s, int rod);

#endif /* VB_RODS_H */
