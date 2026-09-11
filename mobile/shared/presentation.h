#ifndef ML_PRESENTATION_H
#define ML_PRESENTATION_H
#include "mobile_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Shared, platform-independent 3D scene. Clip coordinates retain perspective W;
 * native Metal/GLES rasterize the same triangles with a depth buffer. */
typedef struct { float x,y,z,w,r,g,b,a; } MLVertex;
typedef struct MLScene MLScene;
MLScene *ml_scene_create(void);
void ml_scene_destroy(MLScene *scene);
void ml_scene_build(MLScene *scene, const float *snapshot, float aspect, int reduced);
const MLVertex *ml_scene_vertices(const MLScene *scene);
int ml_scene_opaque_count(const MLScene *scene);
int ml_scene_vertex_count(const MLScene *scene);
int ml_scene_overflowed(const MLScene *scene);
/* Rectangles in R/B/Y/G order, in logical points/dp, with a shared 68pt cap. */
void ml_control_layout(float width, float height, float *rects);
unsigned ml_magnet_color(int color);
unsigned ml_accent_color(int level);
const char *ml_chapter_name(int level);
int ml_music_count(void);
const char *ml_music_track(int index); /* Full playlist, wraps in either direction. */
void ml_menu_snapshot(float seconds, float aspect, int reduced, float *out);
#ifdef __cplusplus
}
#endif
#endif
