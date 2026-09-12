#ifndef MOBILE_CORE_H
#define MOBILE_CORE_H
#ifdef __cplusplus
extern "C" {
#endif
/* Single-owner API: invoke only from the platform's UI/frame thread.
 * Snapshot: 40 header floats, 64 magnets × 6, 64 hazards × 10, 64 checkpoints × 3.
 * Header indices are documented in README.md and consumed by both native renderers. */
#define ML_SNAPSHOT_SIZE 1256
#define ML_MAG_OFFSET 40
#define ML_OB_OFFSET 424
#define ML_CP_OFFSET 1064
/* Events are ORed across all simulation ticks of one rendered frame. */
enum { ML_ATTACH=1, ML_CHECKPOINT=2, ML_SWING=4, ML_DEATH=8, ML_COMPLETE=16, ML_RESPAWN=32 };
typedef struct MLGame MLGame;
MLGame *ml_create(void);
void ml_destroy(MLGame *game);
void ml_start(MLGame *game, int level);
void ml_set_lava_rate(MLGame *game, float rate);
float ml_lava_rate(const MLGame *game);
void ml_pause(MLGame *game, int paused);
void ml_input(MLGame *game, int color);
void ml_advance(MLGame *game, double seconds);
void ml_snapshot(const MLGame *game, float *out);
int ml_level_count(void);
const char *ml_level_key(int level);
const char *ml_level_name(int level);
const char *ml_level_hint(int level);
#ifdef __cplusplus
}
#endif
#endif
