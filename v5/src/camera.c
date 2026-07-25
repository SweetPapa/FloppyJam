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
    /* 26 deg, not the old 40. At 40 the top of a 58-degree frame sits a
     * degree BELOW the horizon, so the player never once saw the sky the
     * whole course is standing in. 26 puts the skyline in the top of the
     * frame and still reads as a three-quarter view for aiming. */
    c->pitch = 26.0f * BP_DEG;
    c->zoom = 1;
    c->dist_mul = 1.0f;
    c->target = target;
    c->eye_dist = ZOOM_DIST[1];
    c->pos = v3add(target, v3(0.0f, 1.0f, -2.0f));
    c->mode = CAM_SURVEY;
    c->snap = 1;
}

void bp_cam_orbit(BpCam *c, float dyaw, float dpitch)
{
    c->yaw += dyaw;
    /* the floor drops to 6 deg so a player can duck the eye right down and
     * put the neon skyline behind the shot */
    c->pitch = bp_clampf(c->pitch + dpitch, 6.0f * BP_DEG, 82.0f * BP_DEG);
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

/* The near plane is 0.01 and the eye is a point, so "clear" has to mean clear
 * by a margin or geometry still slices into the frame. */
#define CAM_SKIN 0.16f

/* Would the camera sphere be inside any solid geometry here?
 *
 * Posts count. They did not use to, which is why the eye could be driven
 * straight through a mushroom bumper on hole 17 — fifteen of them, each one a
 * place the camera could get stuck inside with no way to orbit out. */
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
        if (fabsf(q.y) > b->hy + CAM_SKIN) continue;
        lx =  q.x * cs + q.z * sn;
        lz = -q.x * sn + q.z * cs;
        if (fabsf(lx) < b->hx + CAM_SKIN && fabsf(lz) < b->hz + CAM_SKIN) return 1;
    }
    for (i = 0; i < w->nposts; ++i) {
        const BpPost *po = &w->posts[i];
        float r = po->r + CAM_SKIN, dx, dz;
        V3 c;
        bp_post_pose(w, i, &c);
        if (p.y < c.y - CAM_SKIN || p.y > c.y + po->h + CAM_SKIN) continue;
        dx = p.x - c.x; dz = p.z - c.z;
        if (dx * dx + dz * dz < r * r) return 1;
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
        pitch = 14.0f * BP_DEG + bp_clampf(sp * 0.035f, 0.0f, 0.22f);
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
    case CAM_FIRST: {
        /* Down the cue. The look-at point is put a metre and a bit AHEAD of
         * the ball rather than on it, so the ball sits low in the frame and
         * the line to the cup is what you are actually looking at. The eye
         * then lands about a quarter of a metre behind the ball. Callers keep
         * c->yaw locked to the aim, so turning the cue turns the view. */
        V3 fwd = v3(-sinf(c->yaw), 0.0f, -cosf(c->yaw));
        want_t = v3add(v3add(ball, v3mul(fwd, 1.15f)), v3(0.0f, 0.11f, 0.0f));
        pitch = 5.0f * BP_DEG;
        want_dist = 1.42f;
        t_rate = 18.0f;
        /* the eye is deliberately parked inches off the felt behind the ball;
         * letting the wall-avoid lift it defeats the entire point of the view */
        avoid_walls = 0;
        break;
    }
    case CAM_SURVEY:
    default:
        want_t = v3add(ball, v3(0.0f, 0.06f, 0.0f));
        want_dist *= (c->dist_mul > 0.01f) ? c->dist_mul : 1.0f;
        t_rate = 14.0f;
        break;
    }

    (void)cupdist;

    /* Auto-lift. The eye now sits low enough by default (26 deg, so the city
     * is in frame) that orbiting can walk it straight into a rail. The old
     * answer was pullback_dist alone: yank the eye toward the ball until it
     * is clear. That moves the eye radially, by a lot, in one frame, and it
     * reads as a glitch — the survey-orbit judder check catches it.
     *
     * Raising the eye instead moves it tangentially and keeps the framing
     * distance, which reads as a camera decision rather than a pop. Search
     * upward in coarse steps for a pitch where the eye is out of the
     * geometry, then ease onto it; pullback stays as the last resort for
     * anything blocking the middle of the ray. */
    if (avoid_walls) {
        float want_lift = 0.0f;
        int k;
        for (k = 0; k < 16; ++k) {
            float p = pitch + want_lift;
            V3 d = v3(sinf(yaw) * cosf(p), sinf(p), cosf(yaw) * cosf(p));
            /* the test is the same one pullback uses, so a lift that clears
             * it means pullback will not fire at all this frame */
            if (pullback_dist(w, c->target, d, want_dist) >= want_dist - 1e-4f) break;
            want_lift += 2.0f * BP_DEG;
        }
        /* Asymmetric, and that asymmetry is the whole point. Rising is a
         * collision response and happens on the frame it is needed; sinking
         * back is a preference and eases. Easing the rise as well is what put
         * the eye inside a rail for a third of a second at a time. */
        if (c->snap || want_lift > c->lift) c->lift = want_lift;
        else c->lift += (want_lift - c->lift) * smooth_k(4.0f, sdt);
        pitch = bp_clampf(pitch + c->lift, 0.0f, 84.0f * BP_DEG);
    } else {
        c->lift = 0.0f;
    }

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
        /* Pulling in is not negotiable and is not smoothed: an eye that eases
         * toward the safe distance is an eye INSIDE the rail until it gets
         * there. Easing back out stays gentle. */
        if (desired < c->eye_dist) c->eye_dist = desired;
        else c->eye_dist += (desired - c->eye_dist) * smooth_k(7.0f, sdt);
    }

    c->pos = v3add(c->target, v3mul(dir, c->eye_dist));

    /* never let the eye sink through the floor */
    {
        float g = bp_ground_h(w, c->pos.x, c->pos.z, 90.0f, NULL);
        if (g > -1e8f && c->pos.y < g + 0.12f) c->pos.y = g + 0.12f;
    }

    /* Last line of defence. Everything above is a heuristic that can be
     * beaten — the target is still easing, the floor clamp just moved the eye
     * sideways of the ray, a mover swung a bar through where the eye was
     * standing. So test the position we actually arrived at, and if it is
     * inside something, walk it in along the ray until it is not. This is the
     * check that makes "the camera is never inside a wall" true rather than
     * usually true. */
    if (avoid_walls && occluded(w, c->pos)) {
        V3 ray = v3sub(c->pos, c->target);
        float len = v3len(ray);
        if (len > 1e-4f) {
            float safe = pullback_dist(w, c->target, v3mul(ray, 1.0f / len), len);
            c->eye_dist = safe;
            c->pos = v3add(c->target, v3mul(ray, safe / len));
        } else {
            c->pos = c->target;
        }
    }
}

int bp_cam_eye_blocked(const BpCam *c, const BpWorld *w)
{
    return occluded(w, c->pos);
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
