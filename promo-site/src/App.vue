<script setup>
import { RouterLink, RouterView } from 'vue-router'
import { REPO } from './data/games.js'
</script>

<template>
  <header class="site-head">
    <div class="inner">
      <RouterLink to="/" class="brand">
        <span class="dot" />
        <span>SweetPapa&nbsp;Games</span>
      </RouterLink>
      <nav>
        <RouterLink to="/">Games</RouterLink>
        <a :href="REPO" target="_blank" rel="noopener">Source</a>
      </nav>
    </div>
  </header>

  <main>
    <RouterView />
  </main>

  <footer class="site-foot">
    <div class="wrap inner">
      <span>© {{ new Date().getFullYear() }} Forrester Terry · SweetPapa Technologies</span>
      <span class="mono">built small on purpose</span>
    </div>
  </footer>
</template>
