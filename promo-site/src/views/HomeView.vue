<script setup>
import GameCard from '../components/GameCard.vue'
import { games } from '../data/games.js'
</script>

<template>
  <section class="hero">
    <div class="wrap">
      <p class="eyebrow">SweetPapa Technologies</p>
      <h1>
        Small, strange,<br />
        <span class="grad">hand-built games.</span>
      </h1>
      <p class="lede">
        No engines, no asset stores, no launchers. Every mesh, palette and sound is
        generated while the game runs, which is why they still fit on a floppy disk
        instead of a hard drive.
      </p>
    </div>
  </section>

  <section class="wrap list">
    <div class="head">
      <h2>The games</h2>
      <span class="eyebrow">{{ games.length }} released</span>
    </div>
    <div class="grid">
      <GameCard v-for="g in games" :key="g.slug" :game="g" />
    </div>
    <p class="soon mono">More on the way.</p>
  </section>
</template>

<style scoped>
.hero { padding: 92px 0 64px; }
h1 { font-size: clamp(38px, 6.4vw, 68px); font-weight: 700; margin: 10px 0 0; }
.grad {
  background: linear-gradient(96deg, var(--accent), var(--accent-2) 78%);
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
}
.lede { max-width: 60ch; margin: 22px 0 0; color: var(--ink-dim); font-size: 19px; }

.list { padding-bottom: 20px; }
.head { display: flex; align-items: baseline; gap: 16px; margin-bottom: 22px; }
h2 { font-size: 24px; }
.head .eyebrow { margin-left: auto; }
.grid { display: grid; gap: 22px; }
.soon { margin-top: 26px; color: var(--ink-faint); font-size: 13px; }
</style>
