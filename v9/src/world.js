import * as T from "three";
import { LineSegments2 } from "three/addons/lines/LineSegments2.js";
import { LineSegmentsGeometry } from "three/addons/lines/LineSegmentsGeometry.js";
import { LineMaterial } from "three/addons/lines/LineMaterial.js";
import { mergeGeometries } from "three/addons/utils/BufferGeometryUtils.js";
import { EffectComposer } from "three/addons/postprocessing/EffectComposer.js";
import { RenderPass } from "three/addons/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/addons/postprocessing/UnrealBloomPass.js";
import { OutputPass } from "three/addons/postprocessing/OutputPass.js";
import { DISTRICTS, COLORS, CAST, decorations, rng } from "./story.js";
export function terrain(x, z) {
  let y = 0;
  for (const [i, d] of DISTRICTS.entries()) {
    if (i === 2) y += Math.max(0, 1 - Math.hypot(x - d.x, z - d.z) / 32) * -3;
    if (i === 3) y += Math.max(0, 1 - Math.hypot(x - d.x, z - d.z) / 45) * 7;
  }
  return y;
}
export class World {
  constructor(canvas, settings) {
    this.settings = settings;
    this.scene = new T.Scene();
    this.scene.background = new T.Color("#03070c");
    this.scene.fog = new T.FogExp2("#050c14", 0.007);
    this.camera = new T.PerspectiveCamera(
      48,
      innerWidth / innerHeight,
      0.1,
      450,
    );
    this.renderer = new T.WebGLRenderer({
      canvas,
      antialias: true,
      powerPreference: "high-performance",
    });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
    this.renderer.setSize(innerWidth, innerHeight);
    this.renderer.toneMapping = T.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.1;
    this.composer = new EffectComposer(this.renderer);
    this.composer.addPass(new RenderPass(this.scene, this.camera));
    this.bloom = new UnrealBloomPass(
      new T.Vector2(innerWidth / 2, innerHeight / 2),
      0.55,
      0.5,
      0.4,
    );
    this.composer.addPass(this.bloom);
    this.composer.addPass(new OutputPass());
    this.root = new T.Group();
    this.scene.add(this.root);
    this.orbit = 0;
    this.velocity = new T.Vector2();
    this.travel = null;
    this.jump = 0;
    this.vy = 0;
    this.idle = 0;
    this.time = 0;
    this.weather = 0;
    this.snow = 0;
    this.particles = [];
    this.reflections = [];
    this.muralMeshes = [];
    this.landmarks = [];
    this.actors = [];
    this.chalk = [];
    this.lastSeed = -1;
    this.build(settings.seed);
    this.player = this.character("#f7f1dc", "Kit");
    this.scene.add(this.player);
    this.player.position.set(0, 0, 10);
    this.marker = this.lantern("#f7dda0");
    this.scene.add(this.marker);
    this.arrow = new T.ArrowHelper(
      new T.Vector3(0, 0, -1),
      new T.Vector3(),
      4,
      0xf5dda0,
      1.2,
      0.8,
    );
    this.scene.add(this.arrow);
    this.createSky();
    this.camera.position.set(48, 58, 94);
    this.camera.lookAt(-8, 0, -20);
    window.addEventListener("resize", () => this.resize());
  }
  resize() {
    this.camera.aspect = innerWidth / innerHeight;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(innerWidth, innerHeight);
    this.composer.setSize(innerWidth, innerHeight);
  }
  glowTexture() {
    const c = document.createElement("canvas");
    c.width = c.height = 64;
    const ctx = c.getContext("2d"),
      g = ctx.createRadialGradient(32, 32, 0, 32, 32, 32);
    g.addColorStop(0, "rgba(255,255,255,1)");
    g.addColorStop(0.15, "rgba(255,255,255,.6)");
    g.addColorStop(0.5, "rgba(255,255,255,.12)");
    g.addColorStop(1, "rgba(255,255,255,0)");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, 64, 64);
    return new T.CanvasTexture(c);
  }
  glow(color, size = 3) {
    if (!this.glowMap) this.glowMap = this.glowTexture();
    const m = new T.Sprite(
      new T.SpriteMaterial({
        map: this.glowMap,
        color,
        transparent: true,
        depthWrite: false,
        blending: T.AdditiveBlending,
      }),
    );
    m.scale.set(size, size, 1);
    return m;
  }
  lantern(color) {
    const g = new T.Group();
    const mesh = new T.Mesh(
      new T.BoxGeometry(0.5, 0.75, 0.5),
      new T.MeshBasicMaterial({ color }),
    );
    g.add(mesh);
    const halo = this.glow(color, 4);
    g.add(halo);
    return g;
  }
  character(color, name) {
    const g = new T.Group();
    const material = new T.MeshBasicMaterial({
      color: new T.Color(color).multiplyScalar(0.3),
    });
    const body = new T.Mesh(new T.CapsuleGeometry(0.36, 0.6, 3, 6), material);
    body.position.y = 0.9;
    g.add(body);
    const edge = new T.LineSegments(
      new T.EdgesGeometry(body.geometry),
      new T.LineBasicMaterial({ color }),
    );
    edge.position.copy(body.position);
    g.add(edge);
    const head = new T.Mesh(
      new T.SphereGeometry(0.31, 10, 8),
      new T.MeshBasicMaterial({ color: "#aa9683" }),
    );
    head.position.y = 1.65;
    g.add(head);
    for (const x of [-0.1, 0.1]) {
      const eye = new T.Mesh(
        new T.SphereGeometry(0.035, 4, 4),
        new T.MeshBasicMaterial({ color: "#18252c" }),
      );
      eye.position.set(x, 1.69, 0.285);
      g.add(eye);
    }
    const smile = new T.Line(
      new T.BufferGeometry().setFromPoints([
        new T.Vector3(-0.09, 1.56, 0.28),
        new T.Vector3(0, 1.52, 0.3),
        new T.Vector3(0.09, 1.56, 0.28),
      ]),
      new T.LineBasicMaterial({ color: "#3f4040" }),
    );
    g.add(smile);
    const accessory = new T.Mesh(
      name === "Bo"
        ? new T.CylinderGeometry(0.38, 0.38, 0.13, 8)
        : name === "Idris"
          ? new T.BoxGeometry(0.75, 0.14, 0.65)
          : name === "Odette"
            ? new T.BoxGeometry(0.43, 0.65, 0.06)
            : new T.BoxGeometry(0.8, 0.15, 0.5),
      new T.MeshBasicMaterial({ color }),
    );
    accessory.position.set(
      0,
      name === "Bo" ? 1.95 : name === "Idris" ? 1.35 : 0.75,
      name === "Odette" ? 0.35 : 0,
    );
    g.add(accessory);
    if (name === "Pearl") {
      const lamp = this.glow("#f5dda0", 0.8);
      lamp.position.set(0, 1.85, 0.3);
      g.add(lamp);
    }
    const lamp = this.lantern(color);
    lamp.scale.setScalar(0.45);
    lamp.position.set(0.58, 0.8, 0.2);
    g.add(lamp);
    g.userData = { name, color, lamp };
    return g;
  }
  build(seed) {
    this.lastSeed = seed;
    const old = this.root;
    this.root = new T.Group();
    this.scene.remove(old);
    old.traverse((o) => {
      o.geometry?.dispose();
      if (o.material) {
        if (Array.isArray(o.material)) o.material.forEach((m) => m.dispose());
        else o.material.dispose();
      }
    });
    this.scene.add(this.root);
    this.landmarks = [];
    this.actors = [];
    this.chalk = [];
    this.reflections = [];
    this.muralMeshes = [];
    this.rails = [];
    this.colliders = [];
    this.staticLines = [];
    this.staticColors = [];
    this.fills = [];
    const random = rng(seed);
    const stroke = (a, b, color) => {
      const c = new T.Color(color);
      this.staticLines.push(...a, ...b);
      this.staticColors.push(c.r, c.g, c.b, c.r, c.g, c.b);
    };
    this.stroke = stroke;
    const solid = (geo, x, y, z, color, rx = 0, ry = 0, fill = 0.055) => {
      const matrix = new T.Matrix4().compose(
        new T.Vector3(x, y, z),
        new T.Quaternion().setFromEuler(new T.Euler(rx, ry, 0)),
        new T.Vector3(1, 1, 1),
      );
      const edge = new T.EdgesGeometry(geo, 20);
      const pos = edge.attributes.position;
      for (let i = 0; i < pos.count; i += 2) {
        const a = new T.Vector3()
            .fromBufferAttribute(pos, i)
            .applyMatrix4(matrix),
          b = new T.Vector3()
            .fromBufferAttribute(pos, i + 1)
            .applyMatrix4(matrix);
        stroke(a.toArray(), b.toArray(), color);
      }
      edge.dispose();
      let g = geo.toNonIndexed();
      geo.dispose();
      g.applyMatrix4(matrix);
      const c = new T.Color(color).multiplyScalar(fill),
        cols = new Float32Array(g.attributes.position.count * 3);
      for (let i = 0; i < cols.length; i += 3) {
        cols[i] = c.r;
        cols[i + 1] = c.g;
        cols[i + 2] = c.b;
      }
      g.setAttribute("color", new T.BufferAttribute(cols, 3));
      g.deleteAttribute("normal");
      g.deleteAttribute("uv");
      this.fills.push(g);
    };
    this.solid = solid;
    // A dark harbor surface with hand-drawn tide lines.
    solid(new T.BoxGeometry(270, 0.3, 240), 0, -5, 0, "#142e3b", 0, 0, 0.28);
    for (let i = 0; i < 180; i++) {
      const x = random() * 260 - 130,
        z = random() * 215 - 95;
      stroke([x, -4.79, z], [x + 2 + random() * 5, -4.79, z + 0.2], "#163c49");
    }
    for (const [i, d] of DISTRICTS.entries()) {
      const y = terrain(d.x, d.z);
      const island = new T.CylinderGeometry(35, 36, 2, 24, 4);
      const ip = island.attributes.position;
      for (let v = 0; v < ip.count; v++)
        ip.setY(
          v,
          ip.getY(v) + terrain(d.x + ip.getX(v), d.z + ip.getZ(v)) - y,
        );
      solid(island, d.x, y - 1.15, d.z, d.color, 0, 0, 0.055);
      for (let j = -5; j <= 5; j++)
        stroke(
          [d.x - 12, y + 0.01, d.z + j * 2],
          [d.x + 12, y + 0.01, d.z + j * 2],
          "#26494a",
        );
      const ax = d.x,
        az = d.z - 9;
      const h = i === 4 ? 20 : i === 3 ? 9 : 6,
        w = i === 4 ? 5 : 10;
      solid(
        i === 4
          ? new T.CylinderGeometry(2.4, 3.3, h, 10)
          : new T.BoxGeometry(w, h, 7),
        ax,
        y + h / 2,
        az,
        d.color,
      );
      if (i === 3)
        solid(
          new T.SphereGeometry(5.3, 12, 6, 0, Math.PI * 2, 0, Math.PI / 2),
          ax,
          y + h,
          az,
          d.color,
        );
      else if (i === 4) {
        solid(
          new T.CylinderGeometry(3.4, 3.4, 3, 10),
          ax,
          y + h + 1.5,
          az,
          d.color,
        );
        solid(new T.ConeGeometry(4, 3, 10), ax, y + h + 4.5, az, d.color);
      } else
        solid(
          new T.ConeGeometry(8, 4, 4),
          ax,
          y + h + 2,
          az,
          d.color,
          0,
          Math.PI / 4,
        );
      if (i !== 4)
        for (const x of [-3, 3])
          solid(
            new T.BoxGeometry(1.6, 2, 0.06),
            ax + x,
            y + 3,
            az + 3.54,
            "#f5dda0",
            0,
            0,
            0.65,
          );
      const sign = this.label(d.anchor, d.color, 8);
      sign.position.set(ax, y + (i === 4 ? 27 : i === 3 ? 15 : 12), az);
      this.root.add(sign);
      const actor = this.character(d.color, d.npc);
      actor.position.set(d.x, y, d.z + 1);
      this.root.add(actor);
      this.actors.push(actor);
      const name = this.label(d.npc, d.color, 2.8);
      name.position.set(d.x, y + 3, d.z + 1);
      this.root.add(name);
      const festival = this.lantern(d.color);
      festival.position.set(d.x + 8, y + 3, d.z + 5);
      this.root.add(festival);
      this.landmarks.push(festival);
      const reflection = new T.Mesh(
        new T.PlaneGeometry(5, 9),
        new T.MeshBasicMaterial({
          map: this.glowMap,
          color: d.color,
          transparent: true,
          opacity: 0,
          depthWrite: false,
          blending: T.AdditiveBlending,
        }),
      );
      reflection.rotation.x = -Math.PI / 2;
      reflection.position.set(d.x + 8, y + 0.03, d.z + 5);
      this.root.add(reflection);
      this.reflections.push(reflection);
      const mural = this.label("⌂  ≋  ✧  ♡  ☼", d.color, 8);
      mural.position.set(d.x + 12, y + 4, d.z - 4);
      this.root.add(mural);
      this.muralMeshes.push(mural);
      // Each district has a spring, chalk sticks, benches, a mural, and string lights.
      solid(
        new T.CylinderGeometry(1.2, 1.2, 0.35, 12),
        d.x - 9,
        y + 0.2,
        d.z + 6,
        "#efaec8",
      );
      for (let n = 0; n < 3; n++) {
        const cx = d.x - 6 + n * 5,
          cz = d.z + 9;
        const chalk = new T.Mesh(
          new T.BoxGeometry(0.2, 0.2, 0.8),
          new T.MeshBasicMaterial({ color: COLORS[(i + n) % 6] }),
        );
        chalk.position.set(cx, terrain(cx, cz) + 0.5, cz);
        this.root.add(chalk);
        const glow = this.glow(COLORS[(i + n) % 6], 1.4);
        chalk.add(glow);
        this.chalk.push(chalk);
      }
      for (let n = 0; n < 9; n++) {
        const x = d.x - 12 + n * 3,
          z = d.z + 5,
          yy = y + 6 - Math.sin((n / 8) * Math.PI) * 1.6;
        const l = this.glow(n % 2 ? d.color : "#f5dda0", 1.3);
        l.position.set(x, yy, z);
        this.root.add(l);
        if (n < 8)
          stroke(
            [x, yy, z],
            [x + 3, y + 6 - Math.sin(((n + 1) / 8) * Math.PI) * 1.6, z],
            d.color,
          );
      }
      for (const x of [-12, 12]) {
        solid(new T.BoxGeometry(0.2, 6, 0.2), d.x + x, y + 3, d.z + 5, d.color);
        solid(new T.BoxGeometry(3, 0.3, 1), d.x + x, y + 0.8, d.z + 9, d.color);
      }
      if (i > 0) {
        const start = new T.Vector3(
            d.x * 0.13,
            terrain(d.x * 0.13, d.z * 0.13) + 1,
            d.z * 0.13 + 6,
          ),
          end = new T.Vector3(d.x, y + 1, d.z + 7),
          mid = start.clone().lerp(end, 0.5);
        mid.y += i === 2 ? 1 : 8;
        const curve = new T.QuadraticBezierCurve3(start, mid, end);
        this.rails.push({ curve, district: i, start, end });
        const pts = curve.getPoints(40);
        for (let j = 0; j < 40; j++)
          stroke(pts[j].toArray(), pts[j + 1].toArray(), d.color);
        const length = Math.hypot(d.x, d.z),
          angle = Math.atan2(d.x, d.z);
        const road = new T.BoxGeometry(6, 0.6, length, 1, 1, 20),
          rp = road.attributes.position,
          roadY = terrain(d.x / 2, d.z / 2);
        for (let v = 0; v < rp.count; v++) {
          const xx =
              rp.getX(v) * Math.cos(angle) +
              rp.getZ(v) * Math.sin(angle) +
              d.x / 2,
            zz =
              -rp.getX(v) * Math.sin(angle) +
              rp.getZ(v) * Math.cos(angle) +
              d.z / 2;
          rp.setY(v, rp.getY(v) + terrain(xx, zz) - roadY);
        }
        solid(road, d.x / 2, roadY - 0.5, d.z / 2, d.color, 0, angle, 0.06);
        for (let j = 1; j < 12; j++) {
          const f = j / 12,
            x = d.x * f,
            z = d.z * f;
          for (const side of [-1, 1]) {
            const xx = x + Math.cos(angle) * 3.4 * side,
              zz = z - Math.sin(angle) * 3.4 * side,
              yy = terrain(xx, zz);
            stroke([xx, yy, zz], [xx, yy + 1.1, zz], d.color);
          }
        }
      }
    }
    for (const house of decorations(seed)) {
      const d = DISTRICTS[house.district],
        y = terrain(house.x, house.z);
      solid(
        new T.BoxGeometry(house.w, house.h, 4),
        house.x,
        y + house.h / 2,
        house.z,
        d.color,
      );
      solid(
        new T.ConeGeometry(house.w * 0.84, 3, 4),
        house.x,
        y + house.h + 1.5,
        house.z,
        d.color,
        0,
        Math.PI / 4,
      );
      this.colliders.push({
        x: house.x,
        z: house.z,
        w: house.w / 2 + 0.5,
        h: 2.5,
        base: y,
        height: house.h + 3,
      });
      for (let j = 0; j < Math.floor(house.h / 2.5); j++)
        solid(
          new T.BoxGeometry(0.8, 1.1, 0.02),
          house.x,
          y + 1.6 + j * 2.5,
          house.z + 2.02,
          j % 2 ? "#edd6a3" : d.color,
          0,
          0,
          0.3,
        );
    }
    // Boats, crane, tunnel arches and greenhouse make the districts legible.
    for (let i = 0; i < 7; i++) {
      const x = 37 + i * 7,
        z = 34 + Math.sin(i) * 4;
      solid(
        new T.CylinderGeometry(1.8, 1.1, 5, 4),
        x,
        -1.4,
        z,
        COLORS[1],
        Math.PI / 2,
        Math.PI / 4,
      );
      stroke([x, -1, z], [x, 6, z], COLORS[1]);
      stroke([x, 5.5, z], [x + 2, 1, z], COLORS[1]);
    }
    solid(new T.BoxGeometry(0.8, 17, 0.8), 78, 8, 2, COLORS[1]);
    solid(new T.BoxGeometry(17, 0.6, 0.6), 71, 16, 2, COLORS[1]);
    stroke([63, 16, 2], [63, 5, 2], COLORS[1]);
    for (let j = 0; j < 5; j++) {
      solid(
        new T.TorusGeometry(5, 0.12, 4, 12, Math.PI),
        23 + j * 3,
        -1,
        -50 - j * 3,
        COLORS[2],
        0,
        -Math.PI / 4,
      );
    }
    for (let j = 0; j < 8; j++) {
      const x = -57 + j * 3,
        z = -46,
        y = terrain(x, z);
      solid(new T.ConeGeometry(1.6, 4, 7), x, y + 2, z, COLORS[0]);
    }
    solid(
      new T.BoxGeometry(8, 5, 6),
      -57,
      terrain(-57, -68) + 2.5,
      -68,
      COLORS[3],
    );
    for (const [name, x, z] of [
      ["Winnie", -5, 7],
      ["Ferris", 5, 10],
      ["Rosie", -10, 4],
      ["Gus", -73, 19],
    ]) {
      const actor = this.character(CAST[name].color, name);
      actor.position.set(x, terrain(x, z), z);
      this.root.add(actor);
      this.actors.push(actor);
      const label = this.label(name, CAST[name].color, 2.7);
      label.position.set(x, terrain(x, z) + 3, z);
      this.root.add(label);
    }
    const geom = new LineSegmentsGeometry();
    geom.setPositions(this.staticLines);
    geom.setColors(this.staticColors);
    this.lines = new LineSegments2(
      geom,
      new LineMaterial({
        vertexColors: true,
        transparent: true,
        opacity: 0.8,
        linewidth: 1.3,
        resolution: new T.Vector2(innerWidth, innerHeight),
      }),
    );
    this.root.add(this.lines);
    const merged = mergeGeometries(this.fills);
    this.fills.forEach((g) => g.dispose());
    this.root.add(
      new T.Mesh(
        merged,
        new T.MeshBasicMaterial({ vertexColors: true, side: T.DoubleSide }),
      ),
    );
  }
  label(text, color, width) {
    const c = document.createElement("canvas");
    c.width = 512;
    c.height = 96;
    const ctx = c.getContext("2d");
    ctx.font = "32px Georgia";
    ctx.textAlign = "center";
    ctx.fillStyle = color;
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.fillText(text, 256, 55);
    const s = new T.Sprite(
      new T.SpriteMaterial({
        map: new T.CanvasTexture(c),
        transparent: true,
        depthWrite: false,
      }),
    );
    s.scale.set(width, (width * 96) / 512, 1);
    return s;
  }
  createSky() {
    const random = rng(117);
    const pos = [],
      colors = [];
    for (let i = 0; i < 1200; i++) {
      pos.push(random() * 400 - 200, 40 + random() * 130, random() * 360 - 180);
      const c = new T.Color(COLORS[i % 6]);
      colors.push(c.r, c.g, c.b);
    }
    const geo = new T.BufferGeometry();
    geo.setAttribute("position", new T.Float32BufferAttribute(pos, 3));
    geo.setAttribute("color", new T.Float32BufferAttribute(colors, 3));
    this.stars = new T.Points(
      geo,
      new T.PointsMaterial({
        size: 0.22,
        vertexColors: true,
        transparent: true,
        opacity: 0.8,
      }),
    );
    this.scene.add(this.stars);
    const moon = this.glow("#c5dbe7", 25);
    moon.position.set(-65, 80, -120);
    this.scene.add(moon);
    const disk = new T.Mesh(
      new T.CircleGeometry(3, 32),
      new T.MeshBasicMaterial({ color: "#d2ddd8" }),
    );
    disk.position.copy(moon.position);
    this.scene.add(disk);
    const weatherPos = new Float32Array(900 * 3);
    for (let i = 0; i < 900; i++) {
      weatherPos[i * 3] = random() * 100 - 50;
      weatherPos[i * 3 + 1] = random() * 45;
      weatherPos[i * 3 + 2] = random() * 100 - 50;
    }
    const wgeo = new T.BufferGeometry();
    wgeo.setAttribute("position", new T.BufferAttribute(weatherPos, 3));
    this.precipitation = new T.Points(
      wgeo,
      new T.ShaderMaterial({
        uniforms: { uSnow: { value: 0 }, uOpacity: { value: 0 } },
        transparent: true,
        depthWrite: false,
        vertexShader:
          "uniform float uSnow; void main(){vec4 p=modelViewMatrix*vec4(position,1.0);gl_Position=projectionMatrix*p;gl_PointSize=clamp(mix(420.0,130.0,uSnow)/max(1.0,-p.z),2.0,18.0);}",
        fragmentShader:
          "uniform float uSnow;uniform float uOpacity;void main(){vec2 q=gl_PointCoord-0.5;float rain=1.0-smoothstep(0.035,0.085,abs(q.x+q.y*0.18));float snow=1.0-smoothstep(0.12,0.48,length(q));float a=mix(rain,snow,uSnow)*uOpacity;gl_FragColor=vec4(0.7,0.84,0.9,a);}",
      }),
    );
    this.scene.add(this.precipitation);
    this.beam = new T.Mesh(
      new T.ConeGeometry(12, 100, 32, 1, true),
      new T.MeshBasicMaterial({
        color: "#f5dda0",
        transparent: true,
        opacity: 0.035,
        side: T.DoubleSide,
        depthWrite: false,
        blending: T.AdditiveBlending,
      }),
    );
    this.beam.geometry.translate(0, -50, 0);
    this.beam.rotation.z = Math.PI / 2;
    this.beam.position.set(-68, 23, 8);
    this.scene.add(this.beam);
  }
  rideTo(index, position) {
    const d = DISTRICTS[index];
    const from = new T.Vector3(
        position.x,
        terrain(position.x, position.z) + 0.8,
        position.z,
      ),
      to = new T.Vector3(d.x, terrain(d.x, d.z) + 0.8, d.z + 4),
      mid = from.clone().lerp(to, 0.5);
    mid.y += index === 2 ? 2 : 10;
    this.travel = {
      curve: new T.QuadraticBezierCurve3(from, mid, to),
      t: 0,
      duration: Math.max(1.5, from.distanceTo(to) / 19),
      index,
    };
  }
  nearest(position) {
    let found = null,
      best = 4;
    for (const actor of this.actors) {
      const d = Math.hypot(
        position.x - actor.position.x,
        position.z - actor.position.z,
      );
      if (d < best) {
        best = d;
        found = actor.userData.name;
      }
    }
    return found;
  }
  update(dt, state, keys, active, title = false) {
    this.time += dt;
    if (state.seed !== this.lastSeed) this.build(state.seed);
    const p = state.position;
    let moving = false;
    this.idle += dt;
    if (active) {
      if (this.travel) {
        const ride = this.travel;
        ride.t = Math.min(1, ride.t + dt / ride.duration);
        const at = ride.curve.getPoint(ride.t);
        p.x = at.x;
        p.z = at.z;
        p.y = at.y;
        moving = true;
        if (ride.t === 1) {
          this.travel = null;
          p.y = terrain(p.x, p.z);
          this.railCooldown = 2;
        }
      } else {
        let x = (keys.has("right") ? 1 : 0) - (keys.has("left") ? 1 : 0),
          z = (keys.has("down") ? 1 : 0) - (keys.has("up") ? 1 : 0);
        const input = new T.Vector2(x, z);
        if (input.length() > 0) {
          input.normalize();
          moving = true;
          this.idle = 0;
        }
        const angle = this.orbit + 0.25;
        const vx = input.x * Math.cos(angle) + input.y * Math.sin(angle),
          vz = -input.x * Math.sin(angle) + input.y * Math.cos(angle);
        const speed = this.settings.lively ? 12 : 9;
        this.velocity.lerp(
          new T.Vector2(vx * speed, vz * speed),
          1 - Math.exp(-dt * (this.weather > 0.2 ? 5 : 11)),
        );
        const nx = p.x + this.velocity.x * dt,
          nz = p.z + this.velocity.y * dt;
        const blocked = this.colliders.some(
          (c) => Math.abs(nx - c.x) < c.w && Math.abs(nz - c.z) < c.h,
        );
        if (!blocked) {
          p.x = nx;
          p.z = nz;
        }
        p.y = terrain(p.x, p.z);
        if (moving)
          this.player.rotation.y = Math.atan2(this.velocity.x, this.velocity.y);
        const onLand =
          DISTRICTS.some((d) => Math.hypot(p.x - d.x, p.z - d.z) < 34) ||
          DISTRICTS.slice(1).some((d) => {
            const f = Math.max(
              0,
              Math.min(1, (p.x * d.x + p.z * d.z) / (d.x * d.x + d.z * d.z)),
            );
            return Math.hypot(p.x - d.x * f, p.z - d.z * f) < 3.5;
          });
        this.waterTime = onLand ? 0 : (this.waterTime || 0) + dt;
        if (!onLand) p.y -= this.waterTime * 4;
        if (this.waterTime > 0.7 || Math.hypot(p.x, p.z) > 118) {
          const d = [...DISTRICTS].sort(
            (a, b) =>
              Math.hypot(p.x - a.x, p.z - a.z) -
              Math.hypot(p.x - b.x, p.z - b.z),
          )[0];
          p.x = d.x;
          p.z = d.z + 10;
          p.y = terrain(p.x, p.z);
          this.velocity.set(0, 0);
          this.waterTime = 0;
          this.onRescue?.();
        }
        this.railCooldown = Math.max(0, (this.railCooldown || 0) - dt);
        if (moving && !this.railCooldown)
          for (const rail of this.rails) {
            if (Math.hypot(p.x - rail.start.x, p.z - rail.start.z) < 1.3) {
              this.rideTo(rail.district, p);
              break;
            }
            if (Math.hypot(p.x - rail.end.x, p.z - rail.end.z) < 1.3) {
              this.rideTo(0, p);
              break;
            }
          }
        for (const d of DISTRICTS)
          if (
            Math.hypot(p.x - (d.x - 9), p.z - (d.z + 6)) < 1.3 &&
            this.jump === 0
          )
            this.vy = 9;
      }
    }
    this.vy -= 18 * dt;
    this.jump = Math.max(0, this.jump + this.vy * dt);
    if (!this.jump) this.vy = 0;
    this.player.position.set(
      p.x,
      p.y +
        this.jump +
        (moving
          ? Math.sin(this.time * 14) * 0.045
          : Math.sin(this.time * 2) * 0.025),
      p.z,
    );
    this.player.userData.lamp.children[0].material.color.set(
      COLORS[state.trail % 6],
    );
    if (moving && Math.sin(this.time * 60) > 0.1)
      this.dust(p.x, p.y + 0.2, p.z, COLORS[state.trail % 6]);
    for (const [i, c] of this.chalk.entries()) {
      c.visible = !state.chalk.includes(i);
      c.rotation.y = this.time * 0.6;
      if (
        c.visible &&
        active &&
        Math.hypot(p.x - c.position.x, p.z - c.position.z) < 1.5
      ) {
        state.chalk.push(i);
        c.visible = false;
        this.onChalk?.();
      }
    }
    for (const actor of this.actors) {
      actor.rotation.y = Math.atan2(
        p.x - actor.position.x,
        p.z - actor.position.z,
      );
      actor.children[0].rotation.z =
        Math.sin(this.time * 1.5 + actor.position.x) * 0.025;
    }
    for (const [i, l] of this.landmarks.entries()) {
      const lit = state.lit.includes(i);
      l.children[0].material.color.set(lit ? DISTRICTS[i].color : "#526069");
      l.children[1].material.opacity = lit ? 1 : 0.25;
      l.position.y =
        terrain(l.position.x, l.position.z) +
        3 +
        Math.sin(this.time * 1.5 + i) * 0.12;
    }
    const targetIndex =
      state.stage === "finale"
        ? DISTRICTS.findIndex((d, i) => !state.finale.includes(i))
        : state.night;
    const d = DISTRICTS[Math.max(0, targetIndex)];
    const target = state.stage === "sleep" ? { x: 0, z: 10 } : d;
    this.marker.position.set(
      target.x,
      terrain(target.x, target.z) + 12 + Math.sin(this.time) * 0.5,
      target.z,
    );
    this.marker.visible = !state.completed;
    const dir = new T.Vector3(target.x - p.x, 0, target.z - p.z);
    if (dir.length() > 0) dir.normalize();
    this.arrow.position.set(p.x, p.y + 0.2, p.z);
    this.arrow.setDirection(dir);
    this.arrow.visible = active && this.idle > 3 && !state.completed;
    this.weather = state.weather?.rain ?? this.weather;
    this.snow = state.weather?.snow ?? this.snow;
    this.weather +=
      (Number(state.night === 1 ? 0.4 : state.night === 2 ? 1 : 0) -
        this.weather) *
      Math.min(1, dt * 0.3);
    this.snow +=
      ((state.night === 3 ? 1 : 0) - this.snow) * Math.min(1, dt * 0.3);
    state.weather = { rain: this.weather, snow: this.snow };
    const w = this.precipitation;
    w.material.uniforms.uOpacity.value = this.weather * 0.45 + this.snow * 0.8;
    w.material.uniforms.uSnow.value = this.snow;
    for (const r of this.reflections) r.material.opacity = this.weather * 0.35;
    for (const [i, m] of this.muralMeshes.entries()) {
      m.visible = state.murals > i * 3;
      m.material.opacity = Math.min(1, (state.murals - i * 3) / 3);
    }
    w.position.set(p.x, 0, p.z);
    const a = w.geometry.attributes.position;
    for (let i = 0; i < a.count; i++) {
      a.array[i * 3 + 1] -= dt * (this.snow > 0.4 ? 2.2 : 22);
      if (a.array[i * 3 + 1] < 0) a.array[i * 3 + 1] = 45;
      a.array[i * 3] +=
        Math.sin(this.time + i) * dt * (this.snow > 0.4 ? 0.6 : 0.05);
    }
    a.needsUpdate = true;
    this.beam.visible = state.stage === "finale" || state.completed;
    this.beam.rotation.set(0, this.time * 0.12, Math.PI / 2);
    for (let i = this.particles.length - 1; i >= 0; i--) {
      const q = this.particles[i];
      q.life -= dt;
      q.mesh.position.y += dt * 0.5;
      q.mesh.material.opacity = q.life;
      if (q.life <= 0) {
        this.scene.remove(q.mesh);
        q.mesh.material.dispose();
        this.particles.splice(i, 1);
      }
    }
    if (title) {
      const t = this.settings.reduced ? 0 : this.time * 0.025;
      this.camera.position.set(48 + Math.sin(t) * 12, 55, 87);
      this.camera.lookAt(-8, 1, -20);
    } else {
      const dist = this.settings.reduced ? 23 : 20;
      const desired = new T.Vector3(
        p.x + Math.sin(this.orbit + 0.25) * dist,
        p.y + 14,
        p.z + Math.cos(this.orbit + 0.25) * dist,
      );
      const focus = new T.Vector3(p.x, p.y + 1.8, p.z),
        direction = desired.clone().sub(focus).normalize(),
        ray = new T.Ray(focus, direction),
        intersection = new T.Vector3();
      let nearest = desired.distanceTo(focus);
      for (const c of this.colliders) {
        const box = new T.Box3(
          new T.Vector3(c.x - c.w, c.base, c.z - c.h),
          new T.Vector3(c.x + c.w, c.base + c.height, c.z + c.h),
        );
        if (ray.intersectBox(box, intersection))
          nearest = Math.min(
            nearest,
            Math.max(4, intersection.distanceTo(focus) - 1.2),
          );
      }
      desired.copy(focus).addScaledVector(direction, nearest);
      this.camera.position.lerp(desired, 1 - Math.exp(-dt * 5));
      this.camera.lookAt(p.x, p.y + 1.5, p.z - 2);
    }
    this.lines.material.linewidth = this.settings.contrast ? 2.4 : 1.3;
    this.lines.material.opacity = this.settings.contrast
      ? 1
      : 0.78 + (this.settings.reduced ? 0 : Math.sin(this.time * 3) * 0.025);
    this.bloom.strength = this.settings.contrast ? 0.55 : 0.35;
    this.composer.render();
  }
  dust(x, y, z, color) {
    if (this.particles.length > 50) return;
    const mesh = this.glow(color, 0.25);
    mesh.position.set(x + Math.sin(this.time * 17) * 0.4, y, z);
    this.scene.add(mesh);
    this.particles.push({ mesh, life: 0.65 });
  }
  jumpNow() {
    if (this.jump === 0) this.vy = 6;
  }
}
