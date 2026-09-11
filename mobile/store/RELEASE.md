# MagLava UAT release — September 11, 2026

Bundle/package identifier: **dev.fofo.maglava**. Marketing version: **1.0**.
Production promotion remains manual. This document supersedes the September 7–9
preparation notes: source delivery to the existing repository is now authorized,
the Apple review phone is saved, and Xcode 26 archives have been built and uploaded.

## Executed store delivery

| Destination | Verified state |
| --- | --- |
| [TestFlight](https://appstoreconnect.apple.com/apps/6809634261/testflight/ios) | Build **1010** uploaded with no warnings, processed **VALID**, and assigned to MagLava Playtest and MagLava Closed Beta. Build 1001 is already waiting for external beta review; Apple requires that review to finish before accepting the next build for review. |
| [External beta link](https://testflight.apple.com/join/syeM6ybj) | Enabled with a 1,000-person limit; external installation depends on Apple's beta review. |
| [Google Play closed testing](https://play.google.com/console/u/1/developers/5087831544540862809/app/4974240038269953473/app-dashboard) | Build **1010** accepted on `alpha` with status **completed**. Verified tester group `fofo-testers@googlegroups.com` and configured countries/rest-of-world availability. |
| Store media | Final native iPhone/iPad/Android cards, Android feature graphic and an actual gameplay preview uploaded. The final artwork/video uses stages 1, 6 and 38. |

Matching iOS and Android build **1010** came from UAT merge
`24a66780ae6cfdd9622e848ecaace95b9a01d4b7`. Both were signed and uploaded locally
from the passing hosted artifacts. Google rollout is complete; Apple processing
is valid and external review remains queued behind build 1001.

English descriptions, categories, age declarations, beta and draft store review contacts, and
support/privacy URLs are saved. There are no ads, purchases, chat, accounts or
analytics SDKs in the native game. Console privacy declarations must match those
facts. Mild fantasy/cartoon violence covers lava and hazard deaths.

Support: https://sweetpapa-games.web.app/maglava/support

Privacy: https://sweetpapa-games.web.app/maglava/privacy

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
macOS signing secrets and Windows Azure OIDC credentials are reused. Automatic
approval review rejected exporting local mobile signing/publisher credentials
to GitHub because that specific sensitive transfer needs explicit user approval.
**No new mobile credentials have been transferred.** The user approval question
is pending. Local store uploads keep keys on this Mac.

See [CI operations](../ci/README.md) for exact commands, secret names, local
signing fallback and rerun behavior. The existing Google tester group and country settings were found and preserved.
The previous draft-only blocker no longer applies. TestFlight groups currently
have no individual testers; the public link becomes usable after external review.
Chrome scripting is disabled and the available UI tool lacks Accessibility
permission; direct store API verification was used instead.

## Evidence and media

Ignored `mobile/.build/uat/` contains signed files, per-store receipts, native test
logs, recordings and upload logs. `mobile/.build/uat/local-1010/media/` contains
the original build 1010 media, including the 28-second App Preview and store cards.
The approved Charon rerender is in `mobile/media/out/`, with publication receipts
in `.build/uat/approved-voice-release/`.
Narration now uses the user-approved AssetForge Gemini 3.1 Flash TTS / Charon
take, committed losslessly with request/audio hashes. Original game music is AAC: the complete four-song
playlist is **7,792,904 bytes**, identical in both native apps.

Temporary local capture emulators/simulators are closed. Physical-device frame
time, thermal behavior, haptics and touch comfort remain beta playtest work.
Windows/Linux use the v4 desktop presentation and procedural music; iOS, Android
and Mac Catalyst share the mobile 3D presentation and original playlist.

## Published downloads and website

[Desktop release 1010](https://github.com/SweetPapa/FloppyJam/releases/tag/maglava-uat-1010)
contains the signed macOS and Windows packages, Linux, trailer and App Preview.
App packages and App Preview retain their verified hosted digests. The trailer
was rerendered with approved Charon narration over the same verified 1010 footage;
`SHA256SUMS.txt` now records its revised digest.

[maglava.io](https://maglava.io) now links to those downloads, both beta programs
and the approved Charon gameplay trailer. SweetPapa/magLava PRs **1 and 2** are
merged; the existing Firebase `maglava` site is deployed. PR 2 repairs five denied
artwork URLs using optimized bundled images, the YouTube handle, tester-group
landing page, locale routes, cross-page anchors and tablet navigation. Production
and live browser checks pass across all 14 localized routes and desktop/tablet/phone
layouts, including images, canonical/alternate links, downloads, trailer playback,
Escape, audio shutdown and focus restoration. The live 4.28 MB trailer SHA-256 is
`b2b149d5772026b8fdf7ef21dd50501296c18a728a9932cf154b731ad357cb78`.
Original local website/game edits were preserved by using an isolated checkout.

AssetForge billing was restored to the user-selected Side Projects account.
The user approved the complete 31.44-second Gemini 3.1 Flash TTS / Charon take;
it now replaces the earlier WARLOCK narration in release media sources.
The App Preview contains music and gameplay captions, with no TTS.

Build 1011 was cancelled before publication after visual review found two blank
iPhone launch-screen captures despite passing native tests. PR **19**, merged
into UAT, waits for visible scene content and rejects blank captures. The compiled
validator accepts all 14 visible screenshots from builds 1010/1011 and rejects
both blank ones. Follow-up run **34616899286** tests this capture safeguard;
its artifacts have not replaced the verified 1010 store builds or website media.
