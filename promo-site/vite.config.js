import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  // Relative base so the built site works from a subpath — GitHub Pages serves
  // project sites from /<repo>/, and an absolute base would 404 every asset.
  base: './',
  build: { outDir: 'dist', assetsDir: 'assets' },
})
