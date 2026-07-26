/* shaders.h — §8, compiled in as strings exactly as §12 asks.
 *
 * Four shaders: the table glass, the ball halo, the crowd bokeh field, and the
 * goal ripple. Each is written twice, once for GLSL 100 (the web build and
 * GLES) and once for 330 (desktop), because a first-class web build is not
 * something you discover at the end (§0).
 *
 * Every one of them is OPTIONAL. If a driver refuses to compile any of these,
 * fx.c drops that pass and draws the primitive version instead — the game is
 * proven fun in gray-box before a single shader is loaded (§14.1, DoD 6), and
 * nothing down here is allowed to be the reason it will not run.
 *
 * All four obey Pillar 4: they render BENEATH the gameplay layer and their
 * brightness is handed to them already capped by palette.c.
 */
#ifndef VB_SHADERS_H
#define VB_SHADERS_H

/* ---------------------------------------------------------------- table --
 * A slab of smoked glass over a void: a faint fresnel edge, engraved lane
 * markings, and a screen-space smear of the ball's own glow standing in for a
 * reflection. Cheap, and it sells the material completely. */
#define VB_GLSL_TABLE_BODY \
"uniform vec2  uRes;\n" \
"uniform vec2  uBall;\n"      /* ball position, screen space              */ \
"uniform vec3  uBallCol;\n" \
"uniform float uHeat;\n"      /* 0..1                                     */ \
"uniform vec4  uRect;\n"      /* table rect: x, y, w, h                   */ \
"uniform float uCrack;\n"     /* a scorcher cracked the glass last rally  */ \
"uniform vec2  uCrackAt;\n"   /* ...and the mouth it went in through      */ \
"vec3 tableShade(vec2 fc, vec3 base, vec3 line) {\n" \
"  vec2 uv = (fc - uRect.xy) / uRect.zw;\n" \
"  vec3 col = base;\n" \
"  float edge = min(min(uv.x, 1.0-uv.x), min(uv.y, 1.0-uv.y));\n" \
"  col += line * 0.55 * pow(1.0 - clamp(edge*14.0, 0.0, 1.0), 3.0);\n" \
"  float cl = 1.0 - smoothstep(0.0006, 0.0028, abs(uv.x-0.5));\n" \
"  col += line * cl * 0.5;\n" \
"  float lanes = 1.0 - smoothstep(0.0, 0.0035, abs(fract(uv.y*6.0)-0.5)/6.0);\n" \
"  col += line * lanes * 0.10;\n" \
"  vec2 d = (fc - uBall) / uRes.y;\n" \
"  float refl = exp(-dot(d,d) * 34.0);\n" \
"  col += uBallCol * refl * (0.16 + 0.26*uHeat);\n" \
"  if (uCrack > 0.001) {\n" \
"    vec2 q = (fc - uCrackAt) / uRes.y;\n" \
"    float rad = length(q);\n" \
"    float ang = atan(q.y, q.x);\n" \
"    float vein = abs(sin(ang*3.0 + sin(rad*6.0)*1.4 + 1.3));\n" \
"    float cr = smoothstep(0.992, 1.0, 1.0 - vein) * exp(-rad*3.6);\n" \
"    col += line * uCrack * cr * 1.6;\n" \
"  }\n" \
"  return col;\n" \
"}\n"

#define VB_TABLE_FS_330 \
"#version 330\n" \
"in vec2 fragTexCoord; in vec4 fragColor;\n" \
"out vec4 finalColor;\n" \
"uniform vec3 uBase; uniform vec3 uLine;\n" \
VB_GLSL_TABLE_BODY \
"void main(){ finalColor = vec4(tableShade(gl_FragCoord.xy, uBase, uLine), 1.0); }\n"

#define VB_TABLE_FS_100 \
"#version 100\n" \
"precision mediump float;\n" \
"varying vec2 fragTexCoord; varying vec4 fragColor;\n" \
"uniform vec3 uBase; uniform vec3 uLine;\n" \
VB_GLSL_TABLE_BODY \
"void main(){ gl_FragColor = vec4(tableShade(gl_FragCoord.xy, uBase, uLine), 1.0); }\n"

/* ----------------------------------------------------------------- ball --
 * A molten core inside a refractive halo. At step 12 it drags the arena's
 * light toward it — the radial pull, which reduce-motion switches off while
 * keeping every bit of the colour. */
#define VB_GLSL_BALL_BODY \
"uniform vec2  uRes;\n" \
"uniform vec2  uBall;\n" \
"uniform float uRadius;\n" \
"uniform vec3  uCol;\n" \
"uniform float uHeat;\n" \
"uniform float uPull;\n" \
"vec4 ballShade(vec2 fc) {\n" \
"  vec2 d = fc - uBall;\n" \
"  float r = length(d) / max(uRadius, 1.0);\n" \
"  float core = 1.0 - smoothstep(0.55, 1.0, r);\n" \
"  float halo = exp(-r*r*0.55) * (0.30 + 0.55*uHeat);\n" \
"  float ring = exp(-pow(r-1.15, 2.0)*26.0) * 0.35;\n" \
"  float pull = uPull * exp(-r*r*0.06) * 0.10 * uHeat;\n" \
"  vec3 col = uCol * (core*1.35 + halo + ring) + vec3(1.0)*core*0.45*uHeat;\n" \
"  return vec4(col + uCol*pull, clamp(core + halo + ring + pull, 0.0, 1.0));\n" \
"}\n"

#define VB_BALL_FS_330 \
"#version 330\n" \
"in vec2 fragTexCoord; in vec4 fragColor;\n" \
"out vec4 finalColor;\n" \
VB_GLSL_BALL_BODY \
"void main(){ finalColor = ballShade(gl_FragCoord.xy); }\n"

#define VB_BALL_FS_100 \
"#version 100\n" \
"precision mediump float;\n" \
"varying vec2 fragTexCoord; varying vec4 fragColor;\n" \
VB_GLSL_BALL_BODY \
"void main(){ gl_FragColor = ballShade(gl_FragCoord.xy); }\n"

/* ---------------------------------------------------------------- crowd --
 * The void is not empty. Fields of bokeh bank and swell with the heat and
 * gasp — a brightness dip — on a save. Pure shader, no sprites; it should read
 * as ten thousand phones in a dark stadium. */
#define VB_GLSL_CROWD_BODY \
"uniform vec2  uRes;\n" \
"uniform float uTime;\n" \
"uniform float uHeat;\n" \
"uniform float uGasp;\n" \
"uniform vec3  uCol;\n" \
"uniform vec4  uRect;\n" \
"float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1)))*43758.5453); }\n" \
"vec4 crowdShade(vec2 fc) {\n" \
"  vec2 uv = fc / uRes;\n" \
"  float inside = step(uRect.x, fc.x) * step(fc.x, uRect.x+uRect.z)\n" \
"               * step(uRect.y, fc.y) * step(fc.y, uRect.y+uRect.w);\n" \
"  float acc = 0.0;\n" \
"  for (int i = 0; i < 3; i++) {\n" \
"    float fi = float(i);\n" \
"    float sc = 9.0 + fi*7.0;\n" \
"    vec2 g = uv*sc + vec2(fi*13.7, uTime*(0.012 + fi*0.008));\n" \
"    vec2 cell = floor(g);\n" \
"    vec2 f = fract(g) - 0.5;\n" \
"    float h = hash(cell + fi*37.0);\n" \
"    float tw = 0.55 + 0.45*sin(uTime*(0.7 + h*2.3) + h*31.0);\n" \
"    float d = length(f - (vec2(hash(cell+1.0), hash(cell+2.0)) - 0.5)*0.6);\n" \
"    acc += smoothstep(0.30, 0.0, d) * step(0.55, h) * tw / (1.0 + fi);\n" \
"  }\n" \
"  float band = 1.0 - smoothstep(0.0, 0.75, abs(uv.y - 0.5)*2.0);\n" \
"  float amt = acc * (0.35 + 0.85*uHeat) * (1.0 - uGasp*0.75) * (0.35 + 0.65*band);\n" \
"  amt *= (1.0 - inside);\n" \
"  return vec4(uCol * amt, clamp(amt, 0.0, 1.0));\n" \
"}\n"

#define VB_CROWD_FS_330 \
"#version 330\n" \
"in vec2 fragTexCoord; in vec4 fragColor;\n" \
"out vec4 finalColor;\n" \
VB_GLSL_CROWD_BODY \
"void main(){ finalColor = crowdShade(gl_FragCoord.xy); }\n"

#define VB_CROWD_FS_100 \
"#version 100\n" \
"precision mediump float;\n" \
"varying vec2 fragTexCoord; varying vec4 fragColor;\n" \
VB_GLSL_CROWD_BODY \
"void main(){ gl_FragColor = crowdShade(gl_FragCoord.xy); }\n"

/* ----------------------------------------------------------------- goal --
 * A goal is a detonation: a shockwave that displaces the glass on its way
 * out, and the scorer's colour flooding the void behind it. */
#define VB_GLSL_RIPPLE_BODY \
"uniform sampler2D texture0;\n" \
"uniform vec2  uRes;\n" \
"uniform vec2  uAt;\n" \
"uniform float uT;\n"     /* 0..1 through the detonation                 */ \
"uniform float uAmp;\n" \
"uniform vec3  uFlood;\n" \
"vec4 rippleShade(vec2 fc, vec2 uv) {\n" \
"  vec2 d = (fc - uAt) / uRes.y;\n" \
"  float r = length(d);\n" \
"  float front = uT * 1.15;\n" \
"  float w = exp(-pow((r - front)*7.0, 2.0));\n" \
"  vec2 off = (r > 0.0001 ? d/r : vec2(0.0)) * w * uAmp * (1.0 - uT);\n" \
"  vec4 c = texture2D(texture0, uv + off*0.06);\n" \
"  c.rgb += uFlood * w * (1.0 - uT) * 0.55;\n" \
"  c.rgb += uFlood * (1.0 - uT) * (1.0 - uT) * 0.10;\n" \
"  return c;\n" \
"}\n"

#define VB_RIPPLE_FS_330 \
"#version 330\n" \
"in vec2 fragTexCoord; in vec4 fragColor;\n" \
"out vec4 finalColor;\n" \
"#define texture2D texture\n" \
VB_GLSL_RIPPLE_BODY \
"void main(){ finalColor = rippleShade(gl_FragCoord.xy, fragTexCoord); }\n"

#define VB_RIPPLE_FS_100 \
"#version 100\n" \
"precision mediump float;\n" \
"varying vec2 fragTexCoord; varying vec4 fragColor;\n" \
VB_GLSL_RIPPLE_BODY \
"void main(){ gl_FragColor = rippleShade(gl_FragCoord.xy, fragTexCoord); }\n"

#endif /* VB_SHADERS_H */
