<script setup>
import { RouterLink } from 'vue-router'
defineProps({ game: { type: Object, required: true } })
</script>

<template>
  <RouterLink :to="`/game/${game.slug}`" class="card panel">
    <div class="art">
      <img :src="game.hero" :alt="`${game.title} key art`" loading="lazy" />
      <img v-if="game.icon" class="badge-icon" :src="game.icon" :alt="`${game.title} icon`" loading="lazy" />
      <span class="badge mono">{{ game.status }}</span>
    </div>
    <div class="body">
      <h3>{{ game.title }}</h3>
      <p class="tag">{{ game.tagline }}</p>
      <p class="blurb">{{ game.blurb }}</p>
      <ul class="facts mono">
        <li v-for="f in game.facts" :key="f.label">
          <span>{{ f.value }}</span> {{ f.label }}
        </li>
      </ul>
      <span class="more">View game →</span>
    </div>
  </RouterLink>
</template>

<style scoped>
.card {
  display: grid;
  grid-template-columns: 1.15fr 1fr;
  overflow: hidden;
  color: inherit;
  transition: border-color 0.2s, transform 0.2s;
}
.card:hover { text-decoration: none; transform: translateY(-2px); border-color: rgba(255,255,255,0.2); }

.art { position: relative; background: #05050b; }
.art img { display: block; width: 100%; height: 100%; object-fit: cover; }
/* .art img sets width/height 100%, and it out-specifies a bare class —
 * so the icon rule has to name the element too or it renders full-bleed */
.art img.badge-icon {
  position: absolute;
  bottom: 12px; left: 12px;
  width: 54px; height: 54px;
  border-radius: 12px;
  border: 1px solid rgba(255,255,255,0.16);
  object-fit: cover;
  box-shadow: 0 6px 22px rgba(0,0,0,0.55);
}
.badge {
  position: absolute;
  top: 12px; left: 12px;
  padding: 4px 10px;
  border-radius: 999px;
  font-size: 11px;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  background: rgba(7, 7, 15, 0.8);
  border: 1px solid color-mix(in srgb, var(--accent) 50%, transparent);
  color: var(--accent);
}

.body { padding: 26px 28px 24px; }
h3 { font-size: 28px; }
.tag { margin: 6px 0 14px; color: var(--accent); font-weight: 600; }
.blurb { margin: 0 0 18px; color: var(--ink-dim); font-size: 15.5px; }

.facts { list-style: none; margin: 0 0 20px; padding: 0; display: flex; flex-wrap: wrap; gap: 6px 18px; font-size: 12.5px; color: var(--ink-faint); }
.facts span { color: var(--ink); font-weight: 700; }

.more { font-weight: 600; font-size: 15px; color: var(--accent); }

@media (max-width: 820px) {
  .card { grid-template-columns: 1fr; }
  .art img { max-height: 240px; }
}
</style>
