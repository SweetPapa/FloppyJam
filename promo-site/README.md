# SweetPapa Games — promo site

The landing site for the games in this repo. Vue 3 + Vite, no backend, builds to
static files you can drop on GitHub Pages, Netlify, S3 or anything else.

```sh
npm install
npm run dev       # http://localhost:5173
npm run build     # -> dist/
npm run preview   # serve dist/ locally
```

## Adding a game

Everything the site renders comes out of `src/data/games.js`. Adding the next
game is one object in that array plus a folder of screenshots — no component
changes:

1. Drop screenshots in `public/games/<slug>/`. 1280×720 PNGs straight out of the
   game are fine; BREAK PAR's came from `./breakpar --tour`, which walks all
   eighteen holes and screenshots each one.
2. Add an entry to the `games` array. The fields that matter:

   | field | what it does |
   | --- | --- |
   | `slug` | the URL — `#/game/<slug>` |
   | `title`, `tagline`, `blurb` | the card and the hero |
   | `long` | array of paragraphs for the About section |
   | `hero` | the big background image |
   | `accent`, `accent2` | two hex colours; they re-theme the whole detail page |
   | `facts` | the stat strip — label/value pairs |
   | `features` | the "what is in it" grid |
   | `shots` | gallery images with captions |
   | `controls` | array of `[key, description]` pairs |
   | `downloads` | buttons; `note` is the small grey text |

## Notes

- **Hash routing** on purpose. The build is static and may be served from a
  subpath (GitHub Pages project sites are `/<repo>/`), where history-mode deep
  links 404 without server rewrites.
- **`base: './'`** in `vite.config.js` for the same reason — an absolute base
  would 404 every asset off a subpath.
- Download links are placeholders pointing at the repo's releases page. The
  `prod` release workflow attaches the signed builds there, so they start
  working on their own once the first tagged release lands.
