export class Synth {
  constructor() {
    this.context = null;
    this.step = 0;
    this.layers = 0;
    this.weather = 0;
    this.volume = 0.35;
    this.next = 0;
  }
  start() {
    if (this.context) {
      this.context.resume();
      return;
    }
    try {
      this.context = new AudioContext();
      this.master = this.context.createGain();
      this.master.gain.value = this.volume;
      this.master.connect(this.context.destination);
      this.next = this.context.currentTime;
      this.timer = setInterval(() => this.schedule(), 80);
    } catch {
      /* Silent play remains available. */
    }
  }
  note(midi, time, length = 0.4, gain = 0.1, type = "sine") {
    if (!this.context) return;
    const o = this.context.createOscillator(),
      g = this.context.createGain();
    o.type = type;
    o.frequency.value = 440 * 2 ** ((midi - 69) / 12);
    g.gain.setValueAtTime(0, time);
    g.gain.linearRampToValueAtTime(gain, time + 0.025);
    g.gain.exponentialRampToValueAtTime(0.0001, time + length);
    o.connect(g);
    g.connect(this.master);
    o.start(time);
    o.stop(time + length + 0.05);
  }
  noise(time, length = 0.1, gain = 0.05, cutoff = 1000) {
    if (!this.context) return;
    const n = Math.floor(this.context.sampleRate * length),
      buffer = this.context.createBuffer(1, n, this.context.sampleRate),
      a = buffer.getChannelData(0);
    let prev = 0;
    for (let i = 0; i < n; i++) {
      prev = (prev + (Math.random() * 2 - 1) * 0.4) / 1.4;
      a[i] = prev * (1 - i / n);
    }
    const source = this.context.createBufferSource(),
      filter = this.context.createBiquadFilter(),
      g = this.context.createGain();
    source.buffer = buffer;
    filter.type = "lowpass";
    filter.frequency.value = cutoff;
    g.gain.value = gain;
    source.connect(filter);
    filter.connect(g);
    g.connect(this.master);
    source.start(time);
    source.onended = () => {
      source.disconnect();
      filter.disconnect();
      g.disconnect();
    };
  }
  schedule() {
    const c = this.context;
    if (!c || c.state !== "running") return;
    this.master.gain.setTargetAtTime(this.volume, c.currentTime, 0.1);
    if (this.next < c.currentTime - 0.5) this.next = c.currentTime;
    const chords = [
      [48, 55, 59, 64],
      [45, 52, 55, 60],
      [41, 48, 52, 57],
      [43, 50, 53, 59],
    ];
    while (this.next < c.currentTime + 0.2) {
      const t = this.next,
        s = this.step,
        chord = chords[Math.floor(s / 16) % 4];
      if (s % 8 === 0) {
        for (const n of chord.slice(1)) this.note(n, t, 2, 0.035, "sine");
      }
      if (this.layers >= 1 && s % 4 === 0)
        this.note(chord[0] - 12, t, 0.65, 0.13, "triangle");
      if (this.layers >= 2) {
        if (s % 4 === 0) this.note(31, t, 0.18, 0.13);
        if (s % 4 === 2) this.noise(t, 0.16, 0.12, 1800);
        if (s % 2 === 1) this.noise(t, 0.06, 0.035, 5500);
      }
      if (this.layers >= 3 && s % 2 === 0)
        this.note(
          chord[(1 + ((s / 2) % 3)) | 0] + 12,
          t,
          0.32,
          0.035,
          "triangle",
        );
      if (this.layers >= 4 && s % 4 === 0) {
        const melody = [76, 74, 71, 67, 69, 72, 71, 67];
        this.note(melody[Math.floor(s / 4) % 8], t, 0.9, 0.07);
      }
      if (this.layers >= 5 && s % 8 === 0) {
        this.note(chord[2] + 24, t, 1.8, 0.045);
        this.note(chord[1] + 12, t, 2, 0.035, "triangle");
      }
      if (this.weather > 0) this.noise(t, 0.6, 0.025 * this.weather, 700);
      this.step++;
      this.next += 60 / 76 / 4;
    }
  }
  setState(s, settings) {
    this.volume = settings.volume;
    this.layers =
      s.stage === "finale" ? Math.max(1, s.finale.length + 1) : s.lit.length;
    this.weather =
      s.night === 2 ? 1 : s.night === 1 ? 0.4 : s.night === 3 ? 0.2 : 0;
  }
  effect(kind = "tap", character = 0) {
    this.start();
    if (!this.context) return;
    const t = this.context.currentTime;
    if (kind === "talk") {
      this.note(60 + character * 2, t, 0.08, 0.065, "triangle");
      this.note(67 + character, t + 0.09, 0.12, 0.045, "triangle");
      this.noise(t, 0.05, 0.025, 2000);
    } else if (kind === "win") {
      [60, 64, 67, 72].forEach((n, i) => this.note(n, t + i * 0.1, 0.8, 0.1));
    } else if (kind === "chalk") this.noise(t, 0.15, 0.1, 1900);
    else this.note(kind === "tap" ? 74 : 62, t, 0.1, 0.045);
  }
}
