# MagLava UAT delivery

Work on a feature branch, validate it, then merge a pull request into `uat`.
Every relevant merge builds iOS, Android, macOS, Windows and Linux from that
commit. No job submits or promotes a production release. The `uat` branch
requires the **Shared game and release checks** status before merging.

## Release flow

1. Verify the shared C game and release inputs. Assign matching Android
   `versionCode` and Apple `CFBundleVersion`: `1000 + GITHUB_RUN_NUMBER`.
   Marketing version remains 1.0 until explicitly changed on both platforms.
2. Build Android APK/AAB and unsigned iOS archives with current store SDKs.
   Run native integration on Android, iPhone and iPad, then capture real stages.
3. Build and sign Windows using the existing Azure Trusted Signing identity;
   build Linux; sign and notarize universal Mac Catalyst using the existing
   Developer ID identity. Windows/Linux use the original v4 desktop presentation
   and procedural music. Apple/Android use the shared native presentation and
   original four-song playlist.
4. Render current store cards and gameplay videos with Remotion and FFmpeg.
   WARLOCK narration is committed losslessly with request/audio hashes; hosted
   runners need no connection to the homelab.
5. Sign and deliver mobile betas using the `maglava-uat` environment. TestFlight
   internal/external groups are updated and external beta review is requested.
   Google Play uses `alpha` (closed testing), never production. If Console setup
   prevents rollout, save a draft and record the exact blocker.
6. Publish signed desktop downloads, videos and SHA-256 checksums as a GitHub
   **prerelease**. Desktop publication is independent of mobile store approval.

A failed job blocks its dependents. Unsigned Apple archives and diagnostic
captures are retained even on job failure. A rerun keeps its build number; a new
merge or manual workflow dispatch gets a new one. Use a new run when replacing
an already uploaded binary. Dispatch only against `uat`.

## Credentials

The `maglava-uat` environment allows only the `uat` deployment branch. Existing
repository macOS signing secrets and Azure OIDC settings are reused. New mobile
credentials are listed by:

```sh
python3 mobile/ci/configure-secrets.py
```

Exporting local signing identities and publisher keys to GitHub requires explicit
approval. `configure-secrets.py --apply` performs that transfer only after
approval; it never writes keys into the repository or prints their values. Until
configured, the mobile publisher fails with an actionable message. Build,
media and desktop-download jobs remain available.

## Local mobile delivery

This path keeps private keys in the existing local Keychain/files. Download
artifacts from one successful source run into an empty directory; do not mix
builds. Requires macOS, Java, Android build-tools 36.1.0, the existing SPT signing
identity/profile and publisher credentials. Set `MAGLAVA_REVIEW_PHONE` only when
updating the contact; otherwise the saved Apple contact is retained.

```sh
gh run download RUN_ID --pattern 'maglava-*' --dir mobile/.build/uat/local-RUN_ID
python3 mobile/ci/deliver.py --incoming mobile/.build/uat/local-RUN_ID --build BUILD_NUMBER
```

`deliver.py` checks provenance, signs locally, verifies the results and attempts
both stores independently. Receipts are in `mobile/.build/uat/receipts/`; signed
files are in `mobile/.build/uat/signed/BUILD_NUMBER/`. Publisher scripts can resume
metadata and TestFlight processing without uploading a duplicate binary.

Google Play declarations, tester membership and country availability may require
Console completion. Apple external beta review and production review are separate.
A successful upload is not evidence that testers can install a release. Inspect
receipts and the store state before advertising availability.

See [release status](../store/RELEASE.md) for executed runs and outstanding steps.
