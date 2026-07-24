/* camera.h — Section 9. */
#ifndef BP_CAMERA_H
#define BP_CAMERA_H

#include "physics.h"
#include "raylib.h"

typedef enum {
    CAM_SURVEY = 0,   /* free orbit around the resting ball */
    CAM_RIDE,         /* smoothed chase, widens with speed  */
    CAM_CUP,          /* the tension zoom                   */
    CAM_FLYOVER,      /* hole intro, cup back to tee        */
    CAM_CINEMA        /* ace replay, fixed side angle       */
} BpCamMode;

typedef struct {
    float yaw, pitch;
    int   zoom;             /* 0..2 */
    V3    target;           /* smoothed look-at            */
    V3    pos;              /* smoothed eye                */
    int   mode;
    float t;                /* seconds in the current mode */
    float shake;
    float warp_blur;        /* 0..1, drives the tunnel wipe */
    int   snap;             /* jump instead of easing on the next update  */
    V3    fly_a, fly_b;
} BpCam;

void   bp_cam_init(BpCam *c, V3 target);
void   bp_cam_set_mode(BpCam *c, int mode, const BpWorld *w, V3 tee, V3 cup);
void   bp_cam_orbit(BpCam *c, float dyaw, float dpitch);
void   bp_cam_zoom(BpCam *c, int delta);
void   bp_cam_update(BpCam *c, const BpWorld *w, V3 ball, V3 vel, float cupdist, float dt);
Camera3D bp_cam_raylib(const BpCam *c);
/* aim direction that "away from camera" means, for keyboard-only players */
float  bp_cam_forward_yaw(const BpCam *c);

#endif
