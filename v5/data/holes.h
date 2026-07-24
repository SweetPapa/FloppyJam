/* holes.h — the eighteen (Section 6.5).
 *
 * Front 9 teaches the language, Back 9 speaks it. Every hole has exactly one
 * gimmick, a safe route an average player can par with centre ball, and a
 * hero line that needs a named pool technique and can bite.
 *
 * Convention: +Z runs from tee to green, +X is right, +Y is up. Pads are
 * listed special-surface-first because overlapping pads at equal height
 * resolve to the FIRST match.
 */
#ifndef BP_HOLES_H
#define BP_HOLES_H

/* pads --------------------------------------------------------------- */
#define PAD(x0,z0,x1,z1,r)              { x0,z0,x1,z1, 0,0,0, SURF_FELT, r,0, 0,0 }
#define PADY(x0,z0,x1,z1,y,r)           { x0,z0,x1,z1, y,0,0, SURF_FELT, r,0, 0,0 }
#define PADS(x0,z0,x1,z1,y,sx,sz,s,r)   { x0,z0,x1,z1, y,sx,sz, s, r,0, 0,0 }
#define PADZ(x0,z0,x1,z1,y,s,r)         { x0,z0,x1,z1, y,0,0, s, r,0, 0,0 }
#define PADM(x0,z0,x1,z1,y,m)           { x0,z0,x1,z1, y,0,0, SURF_FELT, 0,m, 0,0 }
#define PADK(x0,z0,x1,z1,ka,ks)         { x0,z0,x1,z1, 0,0,0, SURF_KICK, 0,0, ka,ks }

/* walls -------------------------------------------------------------- */
#define W(x0,z0,x1,z1)                  { x0,z0,x1,z1, 0,0,     BOX_RAIL,0,0 }
#define WY(x0,z0,x1,z1,y)               { x0,z0,x1,z1, y,0,     BOX_RAIL,0,0 }
#define WH(x0,z0,x1,z1,y,h)             { x0,z0,x1,z1, y,h,     BOX_RAIL,0,0 }
#define WBUMP(x0,z0,x1,z1,y)            { x0,z0,x1,z1, y,0,     BOX_BUMPER_WALL,0,0 }
#define WGATE(x0,z0,x1,z1,y,g)          { x0,z0,x1,z1, y,0,     BOX_GATE,g,0 }
#define WMOV(x0,z0,x1,z1,y,m)           { x0,z0,x1,z1, y,0,     BOX_RAIL,0,m }

/* posts -------------------------------------------------------------- */
#define POST(x,z,r)                     { x,0,z, r,0.30f,   POST_RAIL,0,0,0 }
#define BUMP(x,z)                       { x,0,z, 0.20f,0.28f, POST_BUMPER,0,0,0 }
#define BUMPY(x,y,z)                    { x,y,z, 0.20f,0.28f, POST_BUMPER,0,0,0 }
#define TARGET(x,z,g)                   { x,0,z, 0.17f,0.36f, POST_TARGET,g,0,0 }
#define POSTMOV(r,m)                    { 0,0,0, r,0.30f,   POST_RAIL,0,m,0 }

/* pockets ------------------------------------------------------------ */
#define CUP(x,z)                        { x,z,0,   PK_CUP,-1 }
#define CUPY(x,z,y)                     { x,z,y,   PK_CUP,-1 }
#define SCR(x,z)                        { x,z,0,   PK_SCRATCH,-1 }
#define SCRY(x,z,y)                     { x,z,y,   PK_SCRATCH,-1 }
#define BON(x,z)                        { x,z,0,   PK_BONUS,-1 }
#define WRP(x,z,l)                      { x,z,0,   PK_WARP,l }
#define WRPY(x,z,y,l)                   { x,z,y,   PK_WARP,l }

/* movers ------------------------------------------------------------- */
#define MSPIN(rate)                     { MOV_SPIN,  0,0,0,      0, rate, 0,  0,0 }
#define MORBIT(cx,cz,rad,rate,ph)       { MOV_ORBIT, cx,0,cz,  rad, rate, ph, 0,0 }
#define MSHUT(dx,dz,amp,rate,ph)        { MOV_SHUTTLE, 0,0,0,  amp, rate, ph, dx,dz }
#define MTILT(amp,rate,ph,dx,dz)        { MOV_TILT,  0,0,0,    amp, rate, ph, dx,dz }

/* balls -------------------------------------------------------------- */
#define OBJ(x,z,c,n)                    { x,z, BALL_OBJ, c, n }
#define EIGHTBALL(x,z)                  { x,z, BALL_EIGHT, 7, 8 }

#define CNT(a) (unsigned char)(sizeof(a)/sizeof((a)[0]))

/* ==================================================================== */
/* 1 — OPENING BREAK (par 2): straight lane, one dogleg rail            */

static const BpPad H1P[] = {
    PAD(-1.5f, 0.0f, 1.5f, 7.4f, 0),
    PAD( 1.5f, 4.4f, 5.4f, 7.4f, 0),
};
static const BpWallDef H1W[] = {
    W(-1.5f, 0.0f, -1.5f, 7.4f), W(-1.5f, 0.0f, 1.5f, 0.0f),
    W( 1.5f, 0.0f,  1.5f, 4.4f), W( 1.5f, 4.4f, 5.4f, 4.4f),
    W( 5.4f, 4.4f,  5.4f, 7.4f), W(-1.5f, 7.4f, 5.4f, 7.4f),
};
static const BpPocket H1K[] = { CUP(4.3f, 5.7f) };

/* 2 — BANK ALLEY (par 2): cup behind a blocker, one cushion required   */

static const BpPad H2P[] = {
    PAD(-2.5f, 0.0f, 2.5f, 9.2f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpWallDef H2W[] = {
    W(-2.5f, 6.2f, 0.4f, 6.2f),
};
static const BpPocket H2K[] = { CUP(-1.6f, 7.9f) };

/* 3 — LEDGE (par 2): ramp to a plateau with no back rail but a board   */

static const BpPad H3P[] = {
    PAD (-2.0f, 0.0f, 2.0f, 5.0f, PAD_W | PAD_E | PAD_N),
    PADS(-2.0f, 5.0f, 2.0f, 6.2f, 0.0f, 0.0f, 0.25f, SURF_FELT, PAD_W | PAD_E),
    PADY(-2.0f, 6.2f, 2.0f, 8.6f, 0.30f, PAD_W | PAD_E),
};
static const BpWallDef H3W[] = {
    WY(-0.75f, 8.6f, 0.75f, 8.6f, 0.30f),   /* the backboard you draw off */
};
static const BpPocket H3K[] = { CUPY(0.0f, 7.5f, 0.30f) };

/* 4 — S-BEND (par 3): running english holds the line through two jogs  */

static const BpPad H4P[] = {
    PAD(-1.0f,  0.0f, 1.0f,  3.6f, 0),
    PAD(-1.0f,  3.6f, 3.4f,  5.2f, 0),
    PAD( 1.4f,  5.2f, 3.4f,  9.0f, 0),
    PAD(-1.6f,  9.0f, 3.4f, 10.6f, 0),
    PAD(-1.6f, 10.6f, 0.4f, 13.0f, 0),
};
static const BpWallDef H4W[] = {
    W(-1.0f,  0.0f,  1.0f,  0.0f),
    W(-1.0f,  0.0f, -1.0f,  5.2f), W(-1.0f,  5.2f,  1.4f,  5.2f),
    W( 1.4f,  5.2f,  1.4f,  9.0f), W(-1.6f,  9.0f,  1.4f,  9.0f),
    W(-1.6f,  9.0f, -1.6f, 13.0f),
    W( 1.0f,  0.0f,  1.0f,  3.6f), W( 1.0f,  3.6f,  3.4f,  3.6f),
    W( 3.4f,  3.6f,  3.4f, 10.6f), W( 0.4f, 10.6f,  3.4f, 10.6f),
    W( 0.4f, 10.6f,  0.4f, 13.0f), W(-1.6f, 13.0f,  0.4f, 13.0f),
};
static const BpPocket H4K[] = { CUP(-0.6f, 12.2f) };

/* 5 — DEAD STRAIGHT (par 3): the first object ball, a pure combo       */

static const BpPad H5P[] = {
    PAD(-3.0f, 0.0f, 3.0f, 11.0f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpWallDef H5W[] = {
    W(-3.0f, 7.6f, -0.9f, 7.6f), W(0.9f, 7.6f, 3.0f, 7.6f),
};
static const BpPocket H5K[] = { CUP(0.0f, 9.9f) };
static const BpBallDef H5B[] = { OBJ(0.0f, 8.8f, 2, 3) };

/* 6 — AIRMAIL (par 3): ramp, flight, and a rough collar to punish long */

static const BpPad H6P[] = {
    PADZ(-3.2f,  5.9f,  3.2f,  7.0f, 0.0f, SURF_ROUGH, 0),
    PADZ(-3.2f,  7.0f, -1.7f, 11.0f, 0.0f, SURF_ROUGH, PAD_W),
    PADZ( 1.7f,  7.0f,  3.2f, 11.0f, 0.0f, SURF_ROUGH, PAD_E),
    PADZ(-3.2f, 11.0f,  3.2f, 11.8f, 0.0f, SURF_ROUGH, PAD_S),
    PAD (-1.7f,  7.0f,  1.7f, 11.0f, 0),
    PAD (-2.0f,  0.0f,  2.0f,  3.0f, PAD_W | PAD_E | PAD_N),
    PADS(-2.0f,  3.0f,  2.0f,  4.8f, 0.0f, 0.0f, 0.35f, SURF_FELT, PAD_W | PAD_E),
};
static const BpWallDef H6W[] = {
    W(-3.2f, 5.9f, -3.2f, 11.8f), W(3.2f, 5.9f, 3.2f, 11.8f),
};
static const BpPocket H6K[] = { CUP(0.0f, 9.4f) };

/* 7 — WARP GATE (par 3): the warp is safe, the bridge is the hero line */

static const BpPad H7P[] = {
    PAD(-2.5f,  0.0f,  2.5f,  4.5f, PAD_W | PAD_E | PAD_N),
    PAD(-0.45f, 4.5f,  0.45f, 9.5f, 0),
    PAD(-2.5f,  9.5f,  2.5f, 13.5f, PAD_W | PAD_E | PAD_S),
};
static const BpWallDef H7W[] = {
    W(-2.5f, 4.5f, -0.45f, 4.5f), W(0.45f, 4.5f, 2.5f, 4.5f),
    W(-2.5f, 9.5f, -0.45f, 9.5f), W(0.45f, 9.5f, 2.5f, 9.5f),
};
static const BpPocket H7K[] = {
    WRP(-1.6f, 3.4f, 1), WRP(-1.6f, 10.8f, 0), CUP(1.4f, 12.1f),
};

/* 8 — MINEFIELD (par 3): scratch pockets everywhere, gold in the middle */

static const BpPad H8P[] = {
    PADZ(-3.5f, 9.6f, -1.5f, 11.0f, 0.0f, SURF_SAND, 0),
    PAD (-3.5f, 0.0f,  3.5f, 12.0f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpPocket H8K[] = {
    SCR(-2.0f, 4.2f), SCR(1.7f, 5.0f), SCR(-0.9f, 6.9f), SCR(2.7f, 7.8f),
    SCR(-2.8f, 8.8f), SCR(1.1f, 9.6f), BON(0.4f, 7.7f), CUP(0.0f, 11.2f),
};
static const BpBallDef H8B[] = { OBJ(0.4f, 5.6f, 3, 5) };

/* 9 — FRONT NINE EXAM (par 3): bank, sweeper, then draw onto a shelf   */

static const BpPad H9P[] = {
    PAD (-2.6f, 0.0f,  2.6f,  9.2f, PAD_W | PAD_E | PAD_N),
    PADS(-2.6f, 9.2f,  2.6f, 10.0f, 0.0f, 0.0f, 0.3125f, SURF_FELT, PAD_W | PAD_E),
    PADY(-2.6f, 10.0f, 2.6f, 13.0f, 0.25f, PAD_W | PAD_E | PAD_S),
};
static const BpWallDef H9W[] = {
    W(-2.6f, 4.2f, 0.5f, 4.2f),
    WMOV(-1.35f, 7.0f, 1.35f, 7.0f, 0.0f, 1),      /* the sweeper */
};
static const BpMover H9M[] = { MSPIN(1.5f) };
static const BpPocket H9K[] = { CUPY(0.0f, 11.6f, 0.25f) };

/* 10 — RACK HOLE (par 3): pot the eight, then the cup opens            */

static const BpPad H10P[] = {
    PAD(-2.6f, 0.0f, 2.6f, 11.0f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpPost H10O[] = { POST(-0.95f, 5.4f, 0.17f), POST(0.95f, 5.4f, 0.17f) };
/* One gold pocket only. Two pockets flanking the eight would swallow the cue
 * ball on the tangent line every time, which is a trap, not a decision. */
static const BpPocket H10K[] = { BON(2.30f, 8.60f), CUP(0.0f, 10.0f) };
static const BpBallDef H10B[] = { EIGHTBALL(0.0f, 7.0f) };

/* 11 — ICE RINK (par 3): multi-bank on glass; check english is the tool */

static const BpPad H11P[] = {
    PAD (-2.8f, 0.0f, 2.8f,  1.6f, PAD_W | PAD_E | PAD_N),
    PAD (-2.8f, 9.4f, 2.8f, 12.0f, 0),                 /* felt green: a brake */
    PADZ(-2.8f, 1.6f, 2.8f, 12.0f, 0.0f, SURF_ICE, PAD_W | PAD_E | PAD_S),
};
static const BpWallDef H11W[] = {
    W(-2.8f, 5.0f, 0.7f, 5.0f), W(-0.7f, 8.2f, 2.8f, 8.2f),
};
static const BpPocket H11K[] = { CUP(-1.7f, 10.6f) };

/* 12 — THE LONG ONE (par 4): kicker chain, three routes, one channel   */

static const BpPad H12P[] = {
    PADZ(-2.0f,  9.0f,  2.0f, 10.4f, 0.0f, SURF_ROUGH, 0),
    PADZ(-2.0f, 13.0f,  0.5f, 14.6f, 0.0f, SURF_SAND, 0),
    PADK(-0.8f,  6.0f,  0.8f,  6.9f, 0.0f, 6.5f),
    PADK( 2.6f,  6.0f,  4.4f,  6.9f, 0.0f, 8.2f),
    PADS( 2.0f,  9.0f,  3.2f, 12.0f, 0.36f, -0.30f, 0.0f, SURF_FELT, 0),
    PADS( 3.8f,  9.0f,  5.0f, 12.0f, 0.0f,   0.30f, 0.0f, SURF_FELT, 0),
    PAD ( 3.2f,  9.0f,  3.8f, 12.0f, 0),
    PAD (-2.0f,  0.0f,  2.0f, 13.0f, 0),
    PAD ( 2.0f,  3.0f,  5.0f, 13.0f, 0),
    PAD (-2.0f, 13.0f,  5.0f, 17.0f, 0),
};
static const BpWallDef H12W[] = {
    W(-2.0f,  0.0f,  2.0f,  0.0f), W(-2.0f,  0.0f, -2.0f, 17.0f),
    W( 2.0f,  0.0f,  2.0f,  3.0f), W( 2.0f,  3.0f,  5.0f,  3.0f),
    W( 5.0f,  3.0f,  5.0f, 17.0f), W(-2.0f, 17.0f,  5.0f, 17.0f),
    W( 2.0f,  4.5f,  2.0f, 13.0f),
};
static const BpPocket H12K[] = { CUP(3.4f, 15.6f) };

/* 13 — CASINO I (par 3): safe lane, or two ramps between four scratches */

static const BpPad H13P[] = {
    PAD (-3.4f,  0.0f,  3.4f,  3.4f, PAD_N),
    PAD (-3.4f,  3.4f, -1.2f, 13.6f, 0),
    PAD (-0.8f,  3.4f,  0.8f,  5.2f, 0),
    PADS(-0.8f,  5.2f,  0.8f,  6.0f, 0.0f, 0.0f, 0.40f, SURF_FELT, 0),
    PAD (-0.8f,  7.4f,  0.8f,  9.6f, 0),
    PADS(-0.8f,  9.6f,  0.8f, 10.4f, 0.0f, 0.0f, 0.40f, SURF_FELT, 0),
    PAD (-0.8f, 11.8f,  0.8f, 13.6f, 0),
    PADZ(-2.4f,  6.0f, -0.8f,  7.4f, 0.0f, SURF_ROUGH, 0),
    PADZ( 0.8f,  6.0f,  2.4f,  7.4f, 0.0f, SURF_ROUGH, 0),
    PADZ(-2.4f, 10.4f, -0.8f, 11.8f, 0.0f, SURF_ROUGH, 0),
    PADZ( 0.8f, 10.4f,  2.4f, 11.8f, 0.0f, SURF_ROUGH, 0),
    PAD (-3.4f, 13.6f,  3.4f, 16.6f, PAD_S),
};
static const BpWallDef H13W[] = {
    W(-3.4f,  0.0f, -3.4f, 16.6f), W( 3.4f,  0.0f,  3.4f,  3.4f),
    W( 3.4f, 13.6f,  3.4f, 16.6f),
    W( 0.8f,  3.4f,  3.4f,  3.4f), W(-1.2f,  3.4f, -0.8f,  3.4f),
    W(-1.2f,  3.4f, -1.2f, 13.6f),
};
static const BpPocket H13K[] = {
    SCR(-1.7f, 6.7f), SCR(1.7f, 6.7f), SCR(-1.7f, 11.1f), SCR(1.7f, 11.1f),
    CUP(0.0f, 15.2f),
};

/* 14 — BILLIARDS ROOM (par 3): a rack, two gold pockets, two traps     */

static const BpPad H14P[] = {
    PAD(-3.2f, 0.0f, 3.2f, 12.0f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpPocket H14K[] = {
    SCR(-3.0f, 4.4f), SCR(3.0f, 4.4f), BON(-3.0f, 9.6f), BON(3.0f, 9.6f),
    CUP(0.0f, 10.9f),
};
static const BpBallDef H14B[] = {
    OBJ( 0.000f, 7.000f, 1, 1),
    OBJ(-0.030f, 7.052f, 2, 2), OBJ( 0.030f, 7.052f, 3, 3),
    OBJ(-0.060f, 7.104f, 4, 4), OBJ( 0.000f, 7.104f, 5, 5),
    OBJ( 0.060f, 7.104f, 6, 6),
};

/* 15 — CLOCKWORK (par 4): tilt platforms, a gate target, a warp pair    */

static const BpPad H15P[] = {
    PAD (-2.4f,  0.0f, 2.4f,  4.0f, 0),
    PADM(-2.4f,  4.0f, 2.4f,  8.0f, 0.0f, 1),
    PAD (-2.4f,  8.0f, 2.4f, 10.2f, 0),
    PADM(-1.5f, 10.2f, 1.5f, 13.2f, 0.0f, 2),
    PAD (-2.4f, 13.2f, 2.4f, 17.2f, 0),
};
static const BpMover H15M[] = {
    MTILT(0.100f, 0.85f, 0.0f, 1.0f, 0.0f),
    MTILT(0.070f, 0.65f, 1.2f, 1.0f, 0.0f),
};
static const BpWallDef H15W[] = {
    W(-2.4f,  0.0f,  2.4f,  0.0f),
    W(-2.4f,  0.0f, -2.4f, 10.2f), W( 2.4f,  0.0f,  2.4f, 10.2f),
    W(-2.4f, 10.2f, -1.5f, 10.2f), W( 1.5f, 10.2f,  2.4f, 10.2f),
    W(-2.4f, 13.2f, -1.5f, 13.2f), W( 1.5f, 13.2f,  2.4f, 13.2f),
    W(-2.4f, 13.2f, -2.4f, 17.2f), W( 2.4f, 13.2f,  2.4f, 17.2f),
    W(-2.4f, 17.2f,  2.4f, 17.2f),
    WGATE(-1.5f, 10.35f, 1.5f, 10.35f, 0.0f, 1),
};
static const BpPost H15O[] = { TARGET(1.85f, 9.2f, 1) };
static const BpPocket H15K[] = {
    WRP(-1.8f, 9.3f, 1), WRP(-1.8f, 14.2f, 0), CUP(1.3f, 15.8f),
};

/* 16 — RACK HOLE 2 (par 3): the eight behind a guard, behind a sweeper  */

static const BpPad H16P[] = {
    PAD(-3.0f, 0.0f, 3.0f, 12.0f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpMover H16M[] = { MORBIT(0.0f, 7.7f, 0.85f, 1.05f, 0.0f) };
static const BpPost H16O[] = { POSTMOV(0.19f, 1) };
static const BpPocket H16K[] = {
    SCR(-2.4f, 7.0f), SCR(2.4f, 7.0f), SCR(-2.4f, 9.8f), SCR(2.4f, 9.8f),
    CUP(0.0f, 11.0f),
};
static const BpBallDef H16B[] = { OBJ(0.0f, 7.4f, 2, 9), EIGHTBALL(0.0f, 8.4f) };

/* 17 — CASINO II (par 3): everything is a bumper. Going crazy pays.    */

static const BpPad H17P[] = {
    PAD(-3.6f, 0.0f, 3.6f, 13.0f, PAD_W | PAD_E | PAD_N | PAD_S),
};
static const BpPost H17O[] = {
    BUMP(-2.4f,  3.4f), BUMP( 0.0f,  3.9f), BUMP( 2.4f,  3.4f),
    BUMP(-1.2f,  5.2f), BUMP( 1.2f,  5.2f),
    BUMP(-3.0f,  6.6f), BUMP( 0.0f,  6.6f), BUMP( 3.0f,  6.6f),
    BUMP(-1.8f,  8.0f), BUMP( 1.8f,  8.0f),
    BUMP(-2.7f,  9.4f), BUMP( 0.0f,  9.4f), BUMP( 2.7f,  9.4f),
    BUMP(-1.0f, 10.8f), BUMP( 1.0f, 10.8f),
};
static const BpPocket H17K[] = { BON(-3.3f, 11.9f), BON(3.3f, 11.9f), CUP(0.0f, 12.3f) };
static const BpBallDef H17B[] = { OBJ(-2.9f, 7.3f, 3, 7), OBJ(2.9f, 7.3f, 4, 4) };

/* 18 — THE DROP (par 4): three tiers, a warp finish, a tilting island  */

static const BpPad H18P[] = {
    PADZ(-3.2f,  8.6f, -1.0f,  9.8f, 0.40f, SURF_ROUGH, 0),
    PADY(-2.4f,  0.0f,  2.4f,  4.0f, 1.20f, PAD_W | PAD_E | PAD_N),
    PADS(-2.4f,  4.0f,  2.4f,  5.8f, 1.20f, 0.0f, -0.4444f, SURF_FELT, PAD_W | PAD_E),
    PADY(-3.2f,  5.8f,  3.2f,  9.8f, 0.40f, PAD_W | PAD_E),
    PADS(-3.2f,  9.8f,  3.2f, 11.0f, 0.40f, 0.0f, -0.3333f, SURF_FELT, PAD_W | PAD_E),
    PAD (-3.6f, 11.0f,  3.6f, 13.0f, 0),
    PAD (-3.6f, 13.0f, -1.7f, 17.4f, 0),
    PAD ( 1.7f, 13.0f,  3.6f, 17.4f, 0),
    PAD (-1.7f, 16.8f,  1.7f, 17.4f, 0),
    PAD (-0.45f, 13.0f, 0.45f, 13.7f, 0),
    PADM(-1.35f, 13.7f, 1.35f, 16.8f, 0.0f, 1),
};
static const BpMover H18M[] = { MTILT(0.055f, 0.50f, 0.0f, 1.0f, 0.35f) };
static const BpWallDef H18W[] = {
    W(-3.6f, 11.0f, -3.6f, 17.4f), W( 3.6f, 11.0f,  3.6f, 17.4f),
    W(-3.6f, 17.4f,  3.6f, 17.4f), W(-3.6f, 11.0f, -3.2f, 11.0f),
    W( 3.2f, 11.0f,  3.6f, 11.0f),
};
static const BpPost H18O[] = { BUMPY(0.0f, 0.40f, 7.4f), BUMPY(-2.0f, 0.40f, 6.6f) };
static const BpPocket H18K[] = {
    WRPY( 2.6f,  8.6f, 0.40f, 1), WRPY(-2.7f, 12.0f, 0.0f, 0), CUP(0.0f, 15.2f),
};

/* ==================================================================== */

const BpHole BP_HOLES[BP_NHOLES] = {
{ "OPENING BREAK", "One dogleg rail. Aim, power, go.",
  "Hold SHIFT while aiming for eight-times finer control.",
  2, 0,  0.0f, 0.7f, -6.0f,
  H1P, CNT(H1P), H1W, CNT(H1W), 0,0, H1K, CNT(H1K), 0,0, 0,0 },

{ "BANK ALLEY", "The cup hides behind a wall. Find a cushion.",
  "The dotted guide shows one bounce. Read the angle, then trust it.",
  2, 0,  0.0f, 0.8f, -6.0f,
  H2P, CNT(H2P), H2W, CNT(H2W), 0,0, H2K, CNT(H2K), 0,0, 0,0 },

{ "LEDGE", "Up the ramp to a shelf with no back rail.",
  "Draw spin (tip DOWN on the ball face) pulls the ball back after it lands.",
  2, 0,  0.0f, 0.7f, -3.0f,
  H3P, CNT(H3P), H3W, CNT(H3W), 0,0, H3K, CNT(H3K), 0,0, 0,0 },

{ "S-BEND", "Two blind jogs. Side spin keeps your speed.",
  "Side english changes how the ball comes off a rail — running spin widens it.",
  3, 0,  0.0f, 0.6f, -6.0f,
  H4P, CNT(H4P), H4W, CNT(H4W), 0,0, H4K, CNT(H4K), 0,0, 0,0 },

{ "DEAD STRAIGHT", "An object ball parked on the cup. Play the combo.",
  "Hit the object ball full in the face and it goes where you were pointing.",
  3, 0,  0.0f, 0.8f, -6.0f,
  H5P, CNT(H5P), H5W, CNT(H5W), 0,0, H5K, CNT(H5K), 0,0, H5B, CNT(H5B) },

{ "AIRMAIL", "Launch off the lip. The rough eats anything long.",
  "Backspin survives the flight — a drawn ball checks up when it lands.",
  3, 0,  0.0f, 0.7f, -3.0f,
  H6P, CNT(H6P), H6W, CNT(H6W), 0,0, H6K, CNT(H6K), 0,0, 0,0 },

{ "WARP GATE", "Two linked pockets, or one very thin bridge.",
  "A warp pocket keeps your speed. Aim where you want to LEAVE, not arrive.",
  3, 0,  0.0f, 0.7f, -3.0f,
  H7P, CNT(H7P), H7W, CNT(H7W), 0,0, H7K, CNT(H7K), 0,0, 0,0 },

{ "MINEFIELD", "Six ways to scratch. One gold pocket worth a stroke.",
  "Gold pockets pay a stroke for an OBJECT ball. Your own ball in one is a scratch.",
  3, 0,  0.0f, 0.8f, -6.0f,
  H8P, CNT(H8P), 0,0, 0,0, H8K, CNT(H8K), 0,0, H8B, CNT(H8B) },

{ "FRONT NINE EXAM", "Bank past the wall, time the sweeper, hold the shelf.",
  "Movers reset every shot — what you see while aiming is exactly what you get.",
  3, 0,  0.0f, 0.8f, -6.0f,
  H9P, CNT(H9P), H9W, CNT(H9W), 0,0, H9K, CNT(H9K), H9M, CNT(H9M), 0,0 },

{ "THE RACK", "The cup is capped. Pot the eight to open it.",
  "Any pocket counts for the eight — even a scratch pocket.",
  3, 1,  0.0f, 0.8f, -6.0f,
  H10P, CNT(H10P), 0,0, H10O, CNT(H10O), H10K, CNT(H10K), 0,0, H10B, CNT(H10B) },

{ "ICE RINK", "Nothing slows down. Bank it and pray.",
  "Check english scrubs speed off a cushion. On ice that is the only brake.",
  3, 0,  0.0f, 0.8f, -6.0f,
  H11P, CNT(H11P), H11W, CNT(H11W), 0,0, H11K, CNT(H11K), 0,0, 0,0 },

{ "THE LONG ONE", "Kicker pads and a channel. Pick your risk.",
  "Kicker pads set your speed exactly — power stops mattering once you are on one.",
  4, 0,  0.0f, 0.8f, -6.0f,
  H12P, CNT(H12P), H12W, CNT(H12W), 0,0, H12K, CNT(H12K), 0,0, 0,0 },

{ "CASINO", "Long way round, or two ramps over four scratch pockets.",
  "Missing a jump usually leaves you playable. Usually.",
  3, 0,  0.0f, 0.8f, -3.0f,
  H13P, CNT(H13P), H13W, CNT(H13W), 0,0, H13K, CNT(H13K), 0,0, 0,0 },

{ "BILLIARDS ROOM", "A full rack. Blast it or pick it apart.",
  "A soft cut sends one ball to a gold pocket. A smash sends six somewhere.",
  3, 0,  0.0f, 0.8f, -6.0f,
  H14P, CNT(H14P), 0,0, 0,0, H14K, CNT(H14K), 0,0, H14B, CNT(H14B) },

{ "CLOCKWORK", "Tilting floors, a locked bridge, and a way around it.",
  "Strike the lit post to drop the gate. Or skip the whole thing with the warp.",
  4, 0,  0.0f, 0.7f, -3.0f,
  H15P, CNT(H15P), H15W, CNT(H15W), H15O, CNT(H15O), H15K, CNT(H15K),
  H15M, CNT(H15M), 0,0 },

{ "THE RACK II", "The eight, a guard ball, and a bar that will not stop.",
  "Combo through the guard: hit it thin and the eight barely moves, thick and it flies.",
  3, 1,  0.0f, 0.8f, -6.0f,
  H16P, CNT(H16P), 0,0, H16O, CNT(H16O), H16K, CNT(H16K), H16M, CNT(H16M),
  H16B, CNT(H16B) },

{ "BUMPER CITY", "Fifteen bumpers. Genius or chaos, both work.",
  "Bumpers add speed. Sometimes the smart shot really is the stupid one.",
  3, 0,  0.0f, 0.8f, -6.0f,
  H17P, CNT(H17P), 0,0, H17O, CNT(H17O), H17K, CNT(H17K), 0,0, H17B, CNT(H17B) },

{ "THE DROP", "Three tiers, a warp, and a cup on a tilting island.",
  "Everything you have been taught, in order. Take your time on the last one.",
  4, 0,  0.0f, 0.8f, -3.0f,
  H18P, CNT(H18P), H18W, CNT(H18W), H18O, CNT(H18O), H18K, CNT(H18K),
  H18M, CNT(H18M), 0,0 },
};

#endif /* BP_HOLES_H */
