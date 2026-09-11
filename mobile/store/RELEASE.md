# Build 2 preparation — September 9, 2026

Both native apps are version **1.0 (2)**. Build 1 has never been released, so the
marketing version remains 1.0. Build 2 adds the continuous four-song playlist,
stronger shared impact effects, molten surface currents and animated lava home
screen with a 68% dark overlay for menu contrast. English release notes are updated.

- Signed Android bundle: `mobile/.build/store/MagLava-1.0-2.aab` (8.1 MiB).
- Signed Android APK: `mobile/.build/store/MagLava-1.0-2.apk` (8.5 MiB).
- SHA-256 receipt: `mobile/.build/store/android-signing-2.json`.
- iOS device Release and simulator Debug builds succeeded. The installed SDK is
  still iOS 18.5 / Xcode 16.4, so the local device build is **not upload eligible**.
  Apple requires iOS 26 SDK or newer for uploads.
- Refreshed minimal cloud-build sources: `mobile/.build/store/cloud-build-2`.
  These remain local. The earlier approval requirement for creating a GitHub
  repository and transferring source still applies; no cloud upload occurred.
- Live store status checked again: Google alpha still contains build 1 **draft**;
  Apple iOS/macOS versions remain **PREPARE_FOR_SUBMISSION**, with no builds.
  **Build 2 has not been uploaded to either store.** Console setup and the Apple
  SDK/review-contact blockers described below remain outstanding.
- Current native home screenshots and test evidence: `mobile/.build/playlist-review/`.
  Existing store screenshots have not been replaced and should be refreshed before submission.

`sign-android.py` derives artifact names from the matching iOS/Android versions,
checks parity and keeps per-build signing receipts. `sign-ios.py` derives its
output name from the future archive. To prepare the Play closed-testing draft
when proceeding with submission:

```sh
node mobile/tools/store/upload-play.mjs --build 2
```

The uploader verifies the signed bundle hash, rejects duplicate/newer version
codes and refuses to replace an active closed-testing release. It only saves a
draft; final Console review submission is separate. No upload command was run in
this refinement pass.

---

# MagLava beta release — September 7, 2026

Identifier on every platform: **dev.fofo.maglava**. Version **1.0 (1)**.
These are beta submissions; no production release has been submitted.

| Store | Saved | Outstanding |
| --- | --- | --- |
| [Google Play](https://play.google.com/console/u/1/developers/5087831544540862809/app/4974240038269953473/app-dashboard) | Signed AAB version code 1 committed to alpha, English listing, icon, feature graphic, three phone screenshots, contact email/website | Release remains **draft**. API validation returns “Only releases with status draft may be created on draft app.” Complete Console declarations, countries and tester configuration, then submit the existing release. |
| [App Store Connect](https://appstoreconnect.apple.com/apps/6809634261) | iOS/macOS descriptions, subtitle, Games/Action/Casual categories, age declaration, privacy/support links, iPhone/iPad screenshots, internal/external TestFlight groups, beta description, distribution profiles | No uploaded binary. Xcode 26 archives, local signing, upload/processing, review contact phone and beta-review submission remain pending. Mac listing screenshots and store privacy questionnaire remain pending. |

Apple content declaration records mild cartoon/fantasy violence for the orb's
lava and mechanical-hazard deaths; other content categories are absent. There
are no ads, purchases, chat, analytics SDKs or accounts. The store privacy
questionnaires must match those implementation facts.

Support: https://sweetpapa-games.web.app/maglava/support

Privacy: https://sweetpapa-games.web.app/maglava/privacy

The two pages were added to the existing Firebase Hosting release while
preserving every other live file and its hosting configuration.

## Required input/access

1. Automatic approval review rejected creation of the private GitHub repository
   `SweetPapa/maglava-release-builds`, source/assets upload and workflow dispatch
   because store authorization did not explicitly authorize source-code egress
   or repository creation. User approval was requested and remains pending.
   No repository was created and no source was transferred.
2. Apple's beta-review contact phone is missing. Name/email prepared:
   Forrester Terry / fterry@sweetpapatechnologies.com. No phone number was guessed.
3. Chrome's “Allow JavaScript from Apple Events” is disabled; native AppleScript
   accessibility is also unavailable. User was asked to enable the Chrome
   setting or finish Google Play's Console-only setup. No security setting was
   silently changed.

This Mac has Xcode 16.4 / iOS 18.5 SDK. Apple currently requires Xcode 26 / iOS 26
SDK for uploads: [Apple requirements](https://developer.apple.com/news/upcoming-requirements/).
`apple-build.yml` is a prepared GitHub Actions workflow for unsigned iOS and Mac
Catalyst archives and iOS 26 integration tests. The minimal source snapshot is
in `mobile/.build/store/cloud`; it contains no store credentials or signing keys.
It must be refreshed from the current source before any approved upload.

## Local artifacts and receipts

Everything below lives in ignored `mobile/.build/store/`:

- `MagLava-1.0-1.aab`, SHA-256
  `2f0319ae7daae7d90624f74ca3b002b7bb5ec64ce81815ead951f611eb9acb5a`
- `MagLava-1.0-1.apk`, SHA-256
  `04f46f8d8f578e67ac0fb6f5d5c6b24960614e5844837b6bd5e90e7c4b347133`
- `android-signing.json`, `play-upload.json`, `play-artwork.json`
- `apple-setup.json`, `apple-screenshots.json`, `support-release.json`
- `verified-status.json`: all four Apple screenshots processed **COMPLETE**;
  no Apple builds; Google alpha version 1 **draft**, artwork present, testers empty.
- `MagLava-iOS.mobileprovision`, `MagLava-macOS.mobileprovision`
- `screenshots/` contains captured gameplay and generated store artwork.

Do not re-upload Android version code 1. Continue with its committed alpha draft.
`play-upload.json` describes the initial commit; `play-artwork.json` records the
subsequent artwork correction and Android-specific release notes.

## Release tooling

Run scripts from the repository root. Authentication modules read local keys;
they never log tokens or secret key contents.

- `tools/store/apple-api.mjs`: existing personal ASC key in `~/Downloads`,
  overridable by `ASC_KEY_ID`, `ASC_ISSUER_ID`, `ASC_KEY_PATH`.
- `tools/store/play-api.mjs`: `~/secrets/fofo-play-publisher.json`, overridable by
  `GOOGLE_APPLICATION_CREDENTIALS`.
- `tools/store/sign-android.py`: `~/code/SPT`, alias `spt`; passwords are read
  from existing macOS Keychain entries into process environment variables.
- `tools/store/apple-profile.mjs`: existing Apple Distribution certificate;
  IOS_APP_STORE and MAC_CATALYST_APP_STORE profiles for this bundle.
- `tools/store/setup-apple.mjs`: rerunnable metadata setup. Set
  `MAGLAVA_REVIEW_PHONE` to the supplied contact number to complete beta contact.
- `tools/store/sign-ios.py`: prepared local signer for a future unsigned iOS 26
  archive. Validates bundle/team/SDK/profile before signing; not yet exercised
  against a cloud archive. Mac installer signing/packaging remains to be done.
- `tools/store/upload-play.mjs`: initial bundle upload only; refuses duplicate
  version 1. `upload-play-artwork.mjs` refuses replacing existing listing images.
- `tools/store/upload-apple-screenshots.mjs`: native iPhone/iPad screenshots.
- `tools/store/generate-artwork.swift`: icon/feature graphic from the native icon.
- `tools/store/publish-support.mjs`: inspect by default; `--publish` adds the two
  prepared support pages while preserving the live site's other content.

Gameplay integration passed on Android, iPhone, iPad and Mac Catalyst. Physical
device performance, thermal behavior and haptic feel still need beta playtesting.
