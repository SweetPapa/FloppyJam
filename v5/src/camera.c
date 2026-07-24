#include "camera.h"
#include <string.h>

static const float ZOOM_DIST[4] = { 1.00f, 1.70f, 3.00f, 6.00f };

void bp_cam_init(BpCam *c, V3 target)
{
    memset(c, 0, sizeof(*c));
    c->yaw = BP_PI;              /* looking down +Z from behind the tee */
    c->pitch = 40.0f * BP_DEG;
    c->zoom = 1;
    c->target = target;
    c->pos = v3add(target, v3(0.0f, 1.0f, -2.0f));
    c->mode = CAM_SURVEY;
}

void bp_cam_orbit(BpCam *c, float dyaw, float dpitch)
{
    c->yaw += dyaw;
    c->pitch = bp_clampf(c->pitch + dpitch, 15.0f * BP_DEG, 80.0f * BP_DEG);
}

void bp_cam_zoom(BpCam *c, int delta)
{
    c->zoom = bp_clampi(c->zoom + delta, 0, 3);
}

float bp_cam_forward_yaw(const BpCam *c)
{
    /* the heading the camera is looking along, in aim-space (0 = +Z) */
    return c->yaw + BP_PI;
}

void bp_cam_set_mode(BpCam *c, int mode, const BpWorld *w, V3 tee, V3 cup)
{
    (void)w;
    if (c->mode == mode) return;
    /* Leaving the flyover means crossing the whole hole. Easing from there
     * makes the player watch the camera commute; jump instead. */
    if (c->mode == CAM_FLYOVER && mode == CAM_SURVEY) c->snap = 1;
    c->mode = mode;
    c->t = 0.0f;
    if (mode == CAM_FLYOVER) { c->fly_a = cup; c->fly_b = tee; }
}

/* Keep the eye out of the walls: walk back along the eye ray and stop at the
 * first box the camera sphere would poke into (9.4). */
static V3 pullback(const BpWorld *w, V3 target, V3 eye)
{
    V3 d = v3sub(eye, target);
    float len = v3len(d), t;
    int i, s;
    if (len < 1e-4f) return eye;
    d = v3mul(d, 1.0f / len);
    for (s = 4; s <= 24; ++s) {
        t = len * (float)s / 24.0f;
        {
            V3 p = v3add(target, v3mul(d, t));
            for (i = 0; i < w->nboxes; ++i) {
                const BpBox *b = &w->boxes[i];
                float cs = cosf(b->yaw), sn = sinf(b->yaw);
                V3 q = v3sub(p, v3(b->cx, b->cy, b->cz));
                float lx =  q.x * cs + q.z * sn;
                float lz = -q.x * sn + q.z * cs;
                if (fabsf(lx) < b->hx + 0.16f && fabsf(q.y) < b->hy + 0.16f &&
                    fabsf(lz) < b->hz + 0.16f) {
                    return v3add(target, v3mul(d, len * (float)(s - 1) / 24.0f));
                }
            }
        }
    }
    return eye;
}

void bp_cam_update(BpCam *c, const BpWorld *w, V3 ball, V3 vel, float cupdist, float dt)
{
    V3 want_t = ball, want_p;
    float dist = ZOOM_DIST[c->zoom];
    float k;

    c->t += dt;
    if (c->shake > 0.0f) c->shake = bp_clampf(c->shake - dt * 6.0f, 0.0f, 1.0f);
    if (c->warp_blur > 0.0f) c->warp_blur = bp_clampf(c->warp_blur - dt * 5.0f, 0.0f, 1.0f);

    switch (c->mode) {
    case CAM_RIDE: {
        float sp = sqrtf(vel.x * vel.x + vel.z * vel.z);
        float back = dist + 0.45f + bp_clampf(sp * 0.26f, 0.0f, 2.2f);
        float high = 0.42f + bp_clampf(sp * 0.10f, 0.0f, 0.9f);
        float head = (sp > 0.35f) ? atan2f(vel.x, vel.z) : (c->yaw + BP_PI);
        c->yaw = c->yaw + bp_angdiff(head + BP_PI, c->yaw) * bp_clampf(dt * 2.2f, 0.0f, 1.0f);
        want_t = v3add(ball, v3(0.0f, 0.14f, 0.0f));
        want_p = v3add(want_t, v3(sinf(c->yaw) * back, high, cosf(c->yaw) * back));
        k = bp_clampf(dt * 5.0f, 0.0f, 1.0f);
        break;
    }
    case CAM_CUP:
        /* the tension zoom: low, close, side-on */
        want_t = v3add(ball, v3(0.0f, 0.02f, 0.0f));
        want_p = v3add(want_t, v3(sinf(c->yaw) * 0.46f, 0.16f, cosf(c->yaw) * 0.46f));
        k = bp_clampf(dt * 6.0f, 0.0f, 1.0f);
        break;
    case CAM_FLYOVER: {
        float u = bp_clampf(c->t / 2.5f, 0.0f, 1.0f);
        float e = u * u * (3.0f - 2.0f * u);
        want_t = v3lerp(c->fly_a, c->fly_b, e);
        want_p = v3add(want_t, v3(sinf(c->yaw) * (2.6f - 1.1f * e),
                                  2.0f - 0.9f * e,
                                  cosf(c->yaw) * (2.6f - 1.1f * e)));
        k = 1.0f;
        break;
    }
    case CAM_CINEMA: {
        float ang = c->yaw + 1.35f;
        want_t = ball;
        want_p = v3add(ball, v3(sinf(ang) * 1.5f, 0.60f, cosf(ang) * 1.5f));
        k = bp_clampf(dt * 3.0f, 0.0f, 1.0f);
        break;
    }
    case CAM_SURVEY:
    default:
        want_t = v3add(ball, v3(0.0f, 0.06f, 0.0f));
        want_p = v3add(want_t, v3(sinf(c->yaw) * cosf(c->pitch) * dist,
                                  sinf(c->pitch) * dist,
                                  cosf(c->yaw) * cosf(c->pitch) * dist));
        k = bp_clampf(dt * 9.0f, 0.0f, 1.0f);
        break;
    }

    (void)cupdist;
    if (c->snap) { k = 1.0f; c->snap = 0; }
    c->target = v3lerp(c->target, want_t, k);
    c->pos = v3lerp(c->pos, pullback(w, c->target, want_p), k);
    /* never let the eye sink through the floor */
    {
        float g = bp_ground_h(w, c->pos.x, c->pos.z, 90.0f, NULL);
        if (g > -1e8f && c->pos.y < g + 0.12f) c->pos.y = g + 0.12f;
    }
}

Camera3D bp_cam_raylib(const BpCam *c)
{
    Camera3D cam;
    float s = c->shake * 0.035f;
    cam.position = (Vector3){ c->pos.x + s * sinf(c->t * 61.0f),
                              c->pos.y + s * sinf(c->t * 47.0f),
                              c->pos.z + s * sinf(c->t * 53.0f) };
    cam.target   = (Vector3){ c->target.x, c->target.y, c->target.z };
    cam.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy     = 58.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}
