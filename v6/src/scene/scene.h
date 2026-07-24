/* scene — point-and-click screens (API.md §9.4).
 *
 * A scene is a background composition, a walk plane, a cast of standing
 * puppets and a list of hotspots. The Detective is a paper-puppet who
 * strolls; nothing here has a reflex demand in it (§1.2.1).
 */
#ifndef HD_SCENE_H
#define HD_SCENE_H

#include <stdbool.h>

typedef enum {
    SC_NONE = 0,
    SC_TALK,          /* dialogue node   */
    SC_EXIT,          /* another scene   */
    SC_BOARD,         /* a case board    */
    SC_PUZZLE,        /* a mini-game     */
    SC_CUT            /* a cutscene      */
} scene_request;

bool scene_load(const char *id);
/* Returns SC_NONE while the player is pottering about. When it returns
 * anything else, scene_request_id() says what they walked into. */
scene_request scene_update(float dt);
void          scene_draw(void);
const char   *scene_request_id(void);
const char   *scene_current(void);
const char   *scene_title(void);
int           scene_district_mood(void);

/* backgrounds are shared with cutscenes (BG verb, §9.3) */
void scene_draw_backdrop(const char *bg_kind, float t);
/* the bg kind a scene id composes with, so `BG <scene>` in a .cut resolves
 * to the same picture the player walks around in */
const char *scene_bg_of(const char *scene_id);

#endif
