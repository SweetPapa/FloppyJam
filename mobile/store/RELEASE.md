# MagLava beta release — September 13, 2026

Bundle/package identifier: **dev.fofo.maglava**. Marketing version: **1.0**.
Production promotion remains manual. This document supersedes the September 7–9
preparation notes: source delivery to the existing repository is now authorized,
the Apple review phone is saved, and Xcode 26 archives have been built and uploaded.

## Public beta and launch refresh — September 13, 2026

Build **1016** from UAT commit `7d7ba40a6032e48079cc0cb581489b6721cbde7b`
completed the entire hosted pipeline, including unattended signing and mobile
store delivery: [run 34773171518](https://github.com/SweetPapa/FloppyJam/actions/runs/34773171518).

- Apple build `3284c70c-8172-4212-9c08-ab7bcf2a7ad3` is **VALID** and
  **BETA_APPROVED**. The external group is **MagLava Public Beta** with its public
  [TestFlight link](https://testflight.apple.com/join/syeM6ybj) enabled and no custom
  tester cap. Apple still limits external testing to 10,000 testers per app.
- Google Play build **1016** is configured on open testing (`beta`) as
  **MagLava 1.0 (1016) Public Beta** with API release status `completed`.
  Track persistence and 177 countries/rest-of-world were verified, but the owner
  reports that Play Console and public opt-in still show closed-test access.
  **Public availability is not confirmed.** Beta has no Google Group restriction;
  only alpha has `fofo-testers`. Check the `dev.fofo.maglava` app's Publishing
  overview for review/publishing state before describing public testing as live.
  The `open-testing-verified.json` receipt proves API persistence, not tester access.
  Production was not changed.
- Store information and beta/release notes are saved in all seven game languages.
  Apple uses `es-MX` for Latin American Spanish and `zh-Hans` for Simplified Chinese;
  Google uses `es-419` and `zh-CN`. Other locale mappings are in
  `mobile/store/localizations.json`.
- Launch media includes six-stage screenshot cards for iPhone, iPad and Android
  in seven languages (126 cards), seven feature graphics, a 36-second landscape
  gameplay video, a 36-second portrait video and a 28-second App Store preview.
  Videos use the original soundtrack with no narration. Captured game UI is
  English; screenshot captions and store copy are localized.

The [launch website](https://maglava.io) is deployed from website PRs 3 and 4.
It removes itch.io links, obsolete concept artwork and the old roadmap, and adds
six current gameplay screenshots, a music-only trailer, localized controls and a
press-kit download. Browser checks pass across all seven languages and three
viewport sizes, including legacy redirects, downloads and video playback.
The deployed gallery, hero, poster and trailer hashes match the reviewed files;
external desktop, community, feedback and support links return successfully.

Capture correction PR 27 replays short levels through the normal restart path
in debug media tours only. Release gameplay is unchanged. Native Android and
both Apple device integration checks passed; 30 sampled video frames contain
active gameplay rather than completion menus. The Apple screenshot guard now
rejects completion menus and accepts intentionally dark neon scenes.

The public [desktop release 1016](https://github.com/SweetPapa/FloppyJam/releases/tag/maglava-uat-1016)
contains signed macOS/Windows downloads and Linux. Its launch media refresh keeps
those binaries unchanged and adds a multilingual press kit with revised media
checksums. Production store release remains a separate manual decision.

## UAT automation

The new pipeline is merged into the existing public `SweetPapa/FloppyJam`
repository's `uat` branch. PRs require **Shared game and release checks**.
[Workflow](../../.github/workflows/maglava-uat.yml) builds iOS, Android, universal
Mac Catalyst, Windows x64 and Linux x64. It generates native captures, Remotion
media, beta uploads and versioned desktop prereleases; it never promotes PROD.

- Build **1010**, [run 34588961624](https://github.com/SweetPapa/FloppyJam/actions/runs/34588961624),
  uses UAT commit `24a66780ae6cfdd9622e848ecaace95b9a01d4b7`. All five platform builds
  and native Android/iPhone/iPad checks and captures passed. Hosted media rendering
  and automatic desktop publication passed. The workflow ends with the explicit
  missing-mobile-secrets error; local delivery completed both mobile uploads.
- Native capture waits for focused gameplay and successful rendered frames before
  recording. Android test helpers observe real lifecycle callbacks and report
  stale activity failures safely. One complete retry is permitted only for a
  diagnosed emulator resource recreation, launcher ANR or input-injection error;
  unrelated failures still fail the job. A controlled theme recreation verified
  that the resource-retry condition does not accept other configuration changes.
- Windows uses a static C runtime, verified from the actual executable imports.
  macOS packages are universal, signed, notarized and stapled. Hosted rendering
  uses genuine native iPhone/iPad/Android footage.
- PRs **5–10, 12, 13, 15 and 16** are merged into `uat`; **11 and 14** install
  the workflow definitions on the default branch. Desktop publication was proven
  in builds 1006 and 1010. Original diagnostics and earlier artifacts remain in ignored
  `.build/uat/` directories.

The `maglava-uat` environment permits only the `uat` deployment branch. Existing
macOS signing secrets and Windows Azure OIDC credentials are reused. On September
13, the user explicitly approved uploading the mobile signing and publisher
credentials. All ten `MAGLAVA_*` secrets listed by `configure-secrets.py` are now
configured in that encrypted GitHub environment. Private values were not printed
or committed, and temporary export files were removed. The iOS distribution
identity is valid and the MagLava App Store profile expires June 7, 2027.
Local signing remains available as a fallback.

See [CI operations](../ci/README.md) for exact commands, secret names, local
signing fallback and rerun behavior. The existing Google tester group and country settings were found and preserved.
The previous draft-only blocker no longer applies. TestFlight external review
has approved build 1016; Google beta is configured but public availability remains unverified.
Chrome scripting is disabled and the available UI tool lacks Accessibility
permission; direct store API verification was used instead.

## Evidence and media

Ignored `mobile/.build/uat/launch/` contains before-state store snapshots, upload
logs and release checksum records. `launch-1016/` contains the original successful
hosted receipts and captures; `captures/` contains the additional six-stage native
recordings. `mobile/media/out/` contains the current launch media and press kit.
The former narrated media and build 1010 remain historical artifacts, superseded
by the music-only launch refresh for website and current marketing use.

Temporary capture emulators and simulators are closed. The four original AAC
music files total **7,792,904 bytes**, identical in both native apps. Windows and
Linux retain procedural music; iOS, Android and macOS use the original playlist.
