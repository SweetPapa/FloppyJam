# MagLava launch media

All footage comes from the actual native game. The debug tour selects real color
inputs and obeys the same physics and hazards. Native integration tests precede
recording. Capture scripts close the simulators they start.

Current media uses **music and gameplay only, with no narration**:

- 36-second 1920×1080 landscape gameplay video and optimized 1280×720 web copy.
- 36-second 1080×1920 portrait gameplay video for sharing.
- 28-second 886×1920 App Store preview, without added marketing captions.
- Six screenshot cards per platform (iPhone, iPad, Android), in seven languages:
  126 cards total, plus seven localized 1024×500 feature graphics.
- A downloadable press kit containing these assets, localized store/social copy,
  credits and SHA-256 checksums.

Screenshots and clips cover stages 1, 6, 12, 20, 30 and 38. Debug capture tours
replay short stages through the normal restart path so recorder startup cannot
leave footage on the completion menu. Apple screenshot validation rejects
completion menus while allowing intentionally dark neon stages. Screenshot
headlines are localized; the source game UI is English.

```sh
python3 mobile/media/prepare.py
npm ci --prefix mobile/media
npm run check --prefix mobile/media
python3 mobile/media/render-stores.py
npm run render --prefix mobile/media
cd mobile/media
npx remotion render src/index.tsx GameplayPortrait out/MagLava-Gameplay-Portrait.mp4 --codec h264 --crf 20
npx remotion render src/index.tsx AppPreview out/MagLava-AppPreview.mp4 --codec h264 --crf 18
python3 finish.py
python3 press-kit.py --build BUILD_NUMBER
```

The screenshot renderer bundles once and renders two cards at a time. Licensed
Barlow and pinned official Noto CJK fonts cover the marketing copy. Full CJK fonts
are downloaded only for media rendering; native game font assets are unchanged.
Original narration source files remain archived but are not used by these renders.
Generated media lives in ignored `out/` and is published as release assets.
