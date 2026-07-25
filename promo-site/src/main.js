import { createApp } from 'vue'
import { createRouter, createWebHashHistory } from 'vue-router'
import App from './App.vue'
import HomeView from './views/HomeView.vue'
import GameView from './views/GameView.vue'
import './styles.css'

// Hash history on purpose: the built site is static and may be served from a
// GitHub Pages subpath, where deep links under history mode 404 without server
// rewrites nobody wants to configure.
const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    { path: '/', name: 'home', component: HomeView },
    { path: '/game/:slug', name: 'game', component: GameView },
    { path: '/:pathMatch(.*)*', redirect: '/' },
  ],
  scrollBehavior: () => ({ top: 0 }),
})

createApp(App).use(router).mount('#app')
