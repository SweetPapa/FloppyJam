# MagLava release media

All footage is recorded from the actual native game. Debug tour mode selects real
magnet inputs; it does not move the ship or bypass hazards. `mobile/ci/capture-*.py`
regenerates screenshots and recordings from the release source after native tests.

Narration uses WARLOCK's Orpheus 3B/Tara service. The authored request is
`narration.json`; production speech runs locally at generation time, not in CI.
No voice cloning or third-party narration assets are used.

```sh
python3 mobile/media/prepare.py
cd mobile/media
npm ci
npm run check
npm run render
npx remotion render src/index.tsx AppPreview out/MagLava-AppPreview.mp4 --codec h264 --crf 18
python3 render-stores.py
```

The landscape trailer is 36 seconds, with voiceover, original music, animated
title cards and current gameplay. The portrait store preview is 28 seconds,
30 FPS, with gameplay captions and music. Store cards are rendered at iPhone,
iPad and Android dimensions. FFmpeg normalizes narration and encodes web copies.
Generated large assets live in ignored `out/` and are attached to release artifacts.
