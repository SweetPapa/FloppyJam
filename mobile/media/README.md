# MagLava release media

All footage is recorded from the actual native game. Debug tour mode selects real
magnet inputs; it does not move the ship or bypass hazards. `mobile/ci/capture-*.py`
regenerates screenshots and recordings from the release source after native tests.

Narration uses the user-approved Gemini 3.1 Flash TTS / Charon take generated
through AssetForge. The authored request is `narration.json`; speech is generated
outside CI and committed losslessly with request/audio hashes. The complete
31.44-second take was transcribed and approved before use. No voice cloning or
third-party narration assets are used.

```sh
python3 mobile/media/prepare.py
cd mobile/media
npm ci
npm run check
npm run render
npx remotion render src/index.tsx AppPreview out/MagLava-AppPreview.mp4 --codec h264 --crf 18
python3 render-stores.py
python3 finish.py
```

The landscape trailer is 36 seconds, with voiceover, original music, animated
title cards and current gameplay. The portrait store preview is 28 seconds,
30 FPS, with gameplay captions and music. Store cards use real stages 1, 6 and 38 at iPhone,
iPad and Android dimensions, plus a 1024×500 Google Play feature graphic. FFmpeg
normalizes narration and creates a 720p streaming copy with two-pass audio
mastering and a poster. The final local web copy is 4.5 MB and measures −16.04
LUFS; hosted output sizes vary with freshly captured footage.
Generated large assets live in ignored `out/` and are attached to release artifacts.

AssetForge billing was restored to the user-selected Side Projects account.
The approved take replaces the earlier WARLOCK narration.
