# MagLava UAT delivery

Work on a feature branch, validate it, then merge into `uat`. Pushing that merge
builds iOS, Android, macOS, Windows and Linux from the same commit. No job promotes
production. TestFlight and Google Play closed testing are the mobile destinations;
desktop files are versioned GitHub prerelease downloads.

See `mobile/store/RELEASE.md` for the latest executed run and store status.
