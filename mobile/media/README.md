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

## Narration refresh

The current trailer uses the WARLOCK voice above. A requested replacement is
pending: AssetForge's `sweet-papa-technologies` project returns HTTP 403
`BILLING_DISABLED`, including on free availability probes. The next candidate is
[`gemini-3.1-flash-tts-preview`](https://cloud.google.com/blog/products/ai-machine-learning/gemini-3-1-flash-tts-on-google-cloud/),
which adds expressive delivery and pacing controls. AssetForge's July model list
is stale, and its verifier incorrectly labels every HTTP failure as a 404.
Use the actual service error when diagnosing access. Do not regenerate or claim
the replacement complete until billing is restored and a real audio generation
has been reviewed. App Preview has no narration and is independent of this change.
