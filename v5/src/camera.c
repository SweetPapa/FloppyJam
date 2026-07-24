#include "camera.h"
#include <string.h>

static const float ZOOM_DIST[4] = { 1.00f, 1.70f, 3.00f, 6.00f };

/* Frame-rate independent exponential smoothing. dt*rate clamped to 1 is not:
 * a single long frame used to jump the camera, which is what read as stutter. */
static float smooth_k(float rate, float dt)
{
    return 1.0f - expf(-rate * dt);
}

void bp_cam_init(BpCam *c, V3 target)
{
    memset(c, 0, sizeof(*c));
    c->yaw = BP_PI;              /* looking down +Z from behind the tee */
    c->pitch = 40.0f * BP_DEG;
    c->zoom = 1;
    c->target = target;
    c->eye_dist = ZOOM_DIST[1];
    c->pos = v3add(target, v3(0.0f, 1.0f, -2.0f));
    c->mode = CAM_SURVEY;
    c->snap = 1;
}

void bp_cam_orbit(BpCam *c, float dyaw, float dpitch)
{
    c->yaw += dyaw;
    c->pitch = bp_clampf(c->pitch + dpitch, 12.0f * BP_DEG, 82.0f * BP_DEG);
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

/* Would the camera sphere be inside any solid box here? */
static int occluded(const BpWorld *w, V3 p)
{
    int i;
    for (i = 0; i < w->nboxes; ++i) {
        const BpBox *b = &w->boxes[i];
        float cs, sn, lx, lz;
        V3 q;
        if (!bp_box_visible(w, i)) continue;
        cs = cosf(b->yaw); sn = sinf(b->yaw);
        q = v3sub(p, v3(b->cx, b->cy, b->cz));
        if (fabsf(q.y) > b->hy + 0.16f) continue;
        lx =  q.x * cs + q.z * sn;
        lz = -q.x * sn + q.z * cs;
        if (fabsf(lx) < b->hx + 0.16f && fabsf(lz) < b->hz + 0.16f) return 1;
    }
    return 0;
}

/* How far back along dir the eye may sit. Coarse scan then bisect, so the
 * answer is continuous in the geometry instead of snapping between samples
 * (9.4). The quantised version was the source of the opening-flyover jitter. */
static float pullback_dist(const BpWorld *w, V3 target, V3 dir, float want)
{
    const int N = 12;
    float lo = 0.0f, hi = want;
    int i, blocked = 0;
    for (i = 1; i <= N; ++i) {
        float t = want * (float)i / (float)N;
        if (occluded(w, v3add(target, v3mul(dir, t)))) {
            lo = want * (float)(i - 1) / (float)N;
            hi = t;
            blocked = 1;
            break;
        }
    }
    if (!blocked) return want;
    for (i = 0; i < 10; ++i) {
        float mid = 0.5f * (lo + hi);
        if (occluded(w, v3add(target, v3mul(dir, mid)))) hi = mid; else lo = mid;
    }
    return lo;
}

void bp_cam_update(BpCam *c, const BpWorld *w, V3 ball, V3 vel, float cupdist, float dt)
{
    V3 want_t = ball, dir;
    float yaw = c->yaw, pitch = c->pitch;
    float want_dist = ZOOM_DIST[c->zoom];
    float t_rate = 14.0f, allowed, desired;
    /* The opening flyover is an authored sweep along a path chosen to clear
     * the hole. Letting the wall-avoid duck fire during it makes the eye
     * lurch every time it passes a rail, which is exactly the judder players
     * saw at the start of a round. Cinematics do not dodge scenery. */
    int avoid_walls = 1;
    /* One long frame must not teleport the camera. */
    float sdt = dt > (1.0f / 30.0f) ? (1.0f / 30.0f) : dt;

    c->t += dt;
    if (c->shake > 0.0f) c->shake = bp_clampf(c->shake - dt * 6.0f, 0.0f, 1.0f);
    if (c->warp_blur > 0.0f) c->warp_blur = bp_clampf(c->warp_blur - dt * 5.0f, 0.0f, 1.0f);

    switch (c->mode) {
    case CAM_RIDE: {
        float sp = sqrtf(vel.x * vel.x + vel.z * vel.z);
        float head = (sp > 0.35f) ? atan2f(vel.x, vel.z) : (c->yaw + BP_PI);
        c->yaw += bp_angdiff(head + BP_PI, c->yaw) * smooth_k(2.4f, sdt);
        yaw = c->yaw;
        pitch = 17.0f * BP_DEG + bp_clampf(sp * 0.035f, 0.0f, 0.22f);
        want_dist = want_dist + 0.45f + bp_clampf(sp * 0.26f, 0.0f, 2.2f);
        want_t = v3add(ball, v3(0.0f, 0.14f, 0.0f));
        t_rate = 9.0f;
        break;
    }
    case CAM_CUP:
        /* the tension zoom: low, close, side-on */
        want_t = v3add(ball, v3(0.0f, 0.02f, 0.0f));
        pitch = 20.0f * BP_DEG;
        want_dist = 0.48f;
        t_rate = 12.0f;
        break;
    case CAM_FLYOVER: {
        float u = bp_clampf(c->t / 2.5f, 0.0f, 1.0f);
        float e = u * u * (3.0f - 2.0f * u);
        want_t = v3lerp(c->fly_a, c->fly_b, e);
        pitch = bp_lerpf(37.0f, 24.0f, e) * BP_DEG;
        want_dist = bp_lerpf(2.9f, 1.9f, e);
        t_rate = 26.0f;              /* the path is already smooth */
        avoid_walls = 0;
        break;
    }
    case CAM_CINEMA:
        yaw = c->yaw + 1.35f;
        pitch = 22.0f * BP_DEG;
        want_dist = 1.55f;
        t_rate = 6.0f;
        break;
    case CAM_SURVEY:
    default:
        want_t = v3add(ball, v3(0.0f, 0.06f, 0.0f));
        t_rate = 14.0f;
        break;
    }

    (void)cupdist;
    dir = v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));

    if (c->snap) {
        c->target = want_t;
        c->eye_dist = avoid_walls ? pullback_dist(w, c->target, dir, want_dist)
                                  : want_dist;
        c->snap = 0;
    } else {
        c->target = v3lerp(c->target, want_t, smooth_k(t_rate, sdt));
        allowed = avoid_walls ? pullback_dist(w, c->target, dir, want_dist)
                              : want_dist;
        desired = allowed < want_dist ? allowed : want_dist;
        /* duck in quickly so we never clip a rail, ease back out gently */
        c->eye_dist += (desired - c->eye_dist) *
                       smooth_k(desired < c->eye_dist ? 24.0f : 7.0f, sdt);
    }

    c->pos = v3add(c->target, v3mul(dir, c->eye_dist));
    /* never let the eye sink through the floor */
    {
        float g = bp_ground_h(w, c->pos.x, c->pos.z, 90.0f, NULL);
        if (g > -1e8f && c->pos.y < g + 0.12f) c->pos.y = g + 0.12f;
    }
}

Camera3D bp_cam_raylib(const BpCam *c)
{
    Camera3D cam;
    float s = c->shake * c->shake * 0.045f;
    cam.position = (Vector3){ c->pos.x + s * sinf(c->t * 61.0f),
                              c->pos.y + s * sinf(c->t * 47.0f),
                              c->pos.z + s * sinf(c->t * 53.0f) };
    cam.target   = (Vector3){ c->target.x, c->target.y, c->target.z };
    cam.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy     = 58.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}
