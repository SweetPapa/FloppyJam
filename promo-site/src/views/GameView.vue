<script setup>
import { computed, ref, watch } from 'vue'
import { useRoute, RouterLink } from 'vue-router'
import { gameBySlug } from '../data/games.js'
import { useLatestRelease } from '../data/useLatestRelease.js'

const route = useRoute()
const game = computed(() => gameBySlug(route.params.slug))
const active = ref(0)
watch(() => route.params.slug, () => (active.value = 0))

/* Resolved from the GitHub API on mount, with the build-time data as the
 * fallback, so the download links follow new releases without a redeploy. */
const release = useLatestRelease()
const downloads = computed(() => {
  const g = game.value
  if (!g) return []
  const r = release.value
  return [
    { platform: 'Windows', note: `signed · ${r.windows.size}`, href: r.windows.url, kind: 'bin' },
    { platform: 'macOS', note: `notarized · ${r.macos.size}`, href: r.macos.url, kind: 'bin' },
    ...g.downloads.filter((d) => d.icon === 'code').map((d) => ({ ...d, kind: 'code' })),
  ]
})
</script>

<template>
  <div v-if="!game" class="wrap missing">
    <h1>Not found</h1>
    <RouterLink to="/" class="btn">← Back to the games</RouterLink>
  </div>

  <article
    v-else
    :style="{ '--accent': game.accent, '--accent-2': game.accent2 }"
  >
    <!-- ---- hero ---- -->
    <section class="hero">
      <img class="hero-art" :src="game.hero" :alt="`${game.title} title screen`" />
      <div class="hero-fade" />
      <div class="wrap hero-copy">
        <RouterLink to="/" class="back mono">← all games</RouterLink>
        <img v-if="game.icon" class="icon" :src="game.icon" :alt="`${game.title} icon`" />
        <h1>{{ game.title }}</h1>
        <p class="tag">{{ game.tagline }}</p>
        <div class="dl">
          <a
            v-for="d in downloads"
            :key="d.platform"
            class="btn"
            :class="{ 'btn-primary': d.kind !== 'code' }"
            :href="d.href"
            target="_blank"
            rel="noopener"
          >
            <span>{{ d.platform }}</span>
            <span class="sub">{{ d.note }}</span>
          </a>
        </div>
        <p class="relinfo mono">
          <a :href="release.notes" target="_blank" rel="noopener">{{ release.tag }}</a>
          · signed &amp; notarized
          <template v-if="release.checksums">
            ·
            <a :href="release.checksums" target="_blank" rel="noopener">SHA256SUMS</a>
          </template>
        </p>
      </div>
    </section>

    <!-- ---- facts strip ---- -->
    <section class="wrap">
      <ul class="facts panel">
        <li v-for="f in game.facts" :key="f.label">
          <strong>{{ f.value }}</strong>
          <span class="eyebrow">{{ f.label }}</span>
        </li>
      </ul>
    </section>

    <!-- ---- about ---- -->
    <section class="wrap about">
      <h2>About</h2>
      <p v-for="(p, i) in game.long" :key="i">{{ p }}</p>
    </section>

    <!-- ---- gallery ---- -->
    <section class="wrap gallery">
      <h2>Screenshots</h2>
      <figure class="stage">
        <img class="shot" :src="game.shots[active].src" :alt="game.shots[active].caption" />
        <figcaption>{{ game.shots[active].caption }}</figcaption>
      </figure>
      <div class="thumbs">
        <button
          v-for="(s, i) in game.shots"
          :key="s.src"
          :class="['thumb', { on: i === active }]"
          type="button"
          :aria-label="s.caption"
          :aria-current="i === active"
          @click="active = i"
        >
          <img :src="s.src" :alt="s.caption" loading="lazy" />
        </button>
      </div>
    </section>

    <!-- ---- features ---- -->
    <section class="wrap features">
      <h2>What is in it</h2>
      <div class="feat-grid">
        <div v-for="f in game.features" :key="f.title" class="panel feat">
          <h3>{{ f.title }}</h3>
          <p>{{ f.body }}</p>
        </div>
      </div>
    </section>

    <!-- ---- controls ---- -->
    <section class="wrap controls">
      <h2>Controls</h2>
      <dl class="panel">
        <div v-for="[k, v] in game.controls" :key="k">
          <dt>{{ k }}</dt>
          <dd class="mono">{{ v }}</dd>
        </div>
      </dl>
    </section>
  </article>
</template>

<style scoped>
.missing { padding: 120px 0; display: grid; gap: 20px; justify-items: start; }

/* ---- hero ---- */
.hero { position: relative; padding: 0 0 44px; }
.hero-art {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
  opacity: 0.32;
  filter: saturate(1.15);
}
.hero-fade {
  position: absolute;
  inset: 0;
  background: linear-gradient(
    180deg,
    rgba(7, 7, 15, 0.55) 0%,
    rgba(7, 7, 15, 0.8) 55%,
    var(--bg) 100%
  );
}
.hero-copy { position: relative; padding-top: 74px; }
.back { color: var(--ink-faint); font-size: 13px; }
.icon {
  display: block;
  width: 92px; height: 92px;
  margin: 20px 0 -4px;
  border-radius: 20px;
  border: 1px solid var(--line);
  box-shadow: 0 10px 40px color-mix(in srgb, var(--accent) 28%, transparent);
}
h1 {
  margin: 18px 0 0;
  font-size: clamp(40px, 8vw, 86px);
  font-weight: 700;
  letter-spacing: -0.03em;
  text-shadow: 0 0 42px color-mix(in srgb, var(--accent) 35%, transparent);
}
.tag { margin: 8px 0 30px; font-size: 21px; color: var(--accent); font-weight: 600; }
.dl { display: flex; flex-wrap: wrap; gap: 12px; }
.relinfo { margin: 18px 0 0; font-size: 12.5px; color: var(--ink-faint); }
.relinfo a { color: var(--ink-dim); }

/* ---- facts ---- */
.facts {
  list-style: none;
  margin: 0;
  padding: 20px 8px;
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
}
.facts li { text-align: center; display: grid; gap: 2px; padding: 6px 12px; }
.facts strong { font-size: 26px; letter-spacing: -0.02em; }

/* ---- sections ---- */
section { margin-top: 66px; }
h2 { font-size: 24px; margin-bottom: 18px; }
.about p { max-width: 72ch; color: var(--ink-dim); margin: 0 0 16px; }

/* ---- gallery ---- */
.stage { margin: 0; }
.stage figcaption { margin-top: 12px; color: var(--ink-faint); font-size: 14px; }
.thumbs {
  margin-top: 16px;
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 10px;
}
.thumb {
  padding: 0;
  border: 1px solid var(--line);
  border-radius: 8px;
  overflow: hidden;
  background: #05050b;
  cursor: pointer;
  transition: border-color 0.18s, opacity 0.18s;
  opacity: 0.55;
}
.thumb img { display: block; width: 100%; aspect-ratio: 16 / 9; object-fit: cover; }
.thumb:hover { opacity: 0.85; }
.thumb.on { opacity: 1; border-color: var(--accent); }

/* ---- features ---- */
.feat-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 16px; }
.feat { padding: 22px 24px; }
.feat h3 { font-size: 18px; margin-bottom: 8px; color: var(--accent); }
.feat p { margin: 0; color: var(--ink-dim); font-size: 15px; }

/* ---- controls ---- */
dl { margin: 0; padding: 8px 4px; }
dl > div {
  display: grid;
  grid-template-columns: 190px 1fr;
  gap: 16px;
  padding: 11px 22px;
  border-bottom: 1px solid var(--line);
}
dl > div:last-child { border-bottom: 0; }
dt { font-weight: 600; }
dd { margin: 0; color: var(--ink-dim); font-size: 14.5px; }

@media (max-width: 640px) {
  dl > div { grid-template-columns: 1fr; gap: 2px; }
}
</style>
