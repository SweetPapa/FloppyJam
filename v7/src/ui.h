/* ui.h — §10. Menus, HUD, stat card, and the Tilt negotiation screen.
 *
 * The HUD's only real job is the 90-second test (§16.2): a first-time player
 * must be able to name what the glowing meter means without being told. So it
 * is labelled, it is the colour of the ball, and it says what it does to the
 * ball's speed in plain numbers.
 */
#ifndef VB_UI_H
#define VB_UI_H

typedef struct VbApp VbApp;

/* input and state transitions for every screen that is not the table */
void vb_ui_frame(VbApp *a, float dt);
/* everything on screen, table included (it calls into fx.c) */
void vb_ui_draw(VbApp *a);

#endif /* VB_UI_H */
