# FloppyJam

Games that fit on a 1.44 MB floppy disk. One directory per entry.

| | | |
| --- | --- | --- |
| [`v5/`](v5) | **BREAK PAR** | Mini golf with a cue stick. 18 holes, real pool physics, a neon city, a jazz combo. |
| `v1/`–`v4/`, `v6/` | earlier jams | |

Everything else in the root supports shipping them.

```
art/            source artwork + the generated .icns / .ico
promo-site/      the Vue site that lists the games
scripts/         build, sign, notarize and CI setup
.github/         CI on every push; a signed release on every push to prod
```

## Building a game

Each game directory is self-contained and has its own README. For BREAK PAR:

```sh
cd v5
make            # links against a system raylib, ~170 KB
make run
make test testcourse testcamera testmusic
```

That dev build links raylib dynamically. A **shippable** build is statically
linked and needs raylib compiled with the unused modules stripped out:

```sh
./scripts/build-raylib.sh --out build/raylib-host
cd v5 && make \
  RAYLIB_CFLAGS="-I../build/raylib-host/include" \
  RAYLIB_LIBS="../build/raylib-host/lib/libraylib.a"
make size       # fails over 1,474,560 bytes
```

The trim is not optional. A full static raylib puts the Windows executable at
1,837,056 bytes — over the floppy ceiling. BREAK PAR loads no models, no fonts
and no audio files (every sound is synthesised at runtime), so `dr_mp3`,
`stb_vorbis`, `jar_xm`, `dr_flac`, `cgltf` and `m3d` are all dead weight. Cutting
them gets Windows to **1,432,064** bytes and the macOS universal binary to
**1,166,416**.

## Shipping

| what | how |
| --- | --- |
| macOS, signed + notarized + stapled | `./scripts/sign-macos.sh v5/breakpar --dmg` |
| Windows, Authenticode via Azure | `./scripts/sign-windows.sh v5/breakpar.exe` |
| icons from the source art | `./scripts/make-icons.sh` |
| the release pipeline, one time | `./scripts/setup-ci.sh` |

### `scripts/sign-macos.sh`

Takes a bare executable, a `.app`, a `.dmg` or a `.pkg`. A bare executable gets
wrapped into a `.app` first, because **a bare binary cannot be stapled** — the
notarisation ticket has nowhere to live, so every launch needs the machine online
and able to reach Apple. Wrapping is the difference between "notarised" and
"actually opens on someone else's Mac".

```sh
./scripts/sign-macos.sh v5/breakpar                     # SweetPapa cert (default)
./scripts/sign-macos.sh v5/breakpar --identity redacted-org # <redacted org> cert
./scripts/sign-macos.sh v5/breakpar --dry-run           # sign + verify, no upload
```

Credentials come from the login keychain and are never printed. The two accounts
are presets; `--dry-run` does everything except talk to Apple, which makes it
safe to run while you are still editing the script.

Notarisation prefers an App Store Connect API key and finds it on its own — from
the keychain, or from `ASC_KEY_PATH` / `ASC_KEY_B64`. `--no-asc-key` forces the
Apple ID route instead, which needs a stored app-specific password:

```sh
security add-generic-password -s asc-mcp    -a <KEY_ID> -w "$(cat AuthKey_<KEY_ID>.p8)"
security add-generic-password -s spt-notary -a you@example.com -w
```

### `scripts/sign-windows.sh`

Authenticode signing through Azure Trusted Signing, which is a web service — so
this runs on **macOS, Linux or Windows**. Only the *build* needs a Windows
toolchain, and even that is handled by MinGW cross-compilation.

```sh
./scripts/sign-windows.sh --verify-setup     # check az login + the cert profile
./scripts/sign-windows.sh v5/breakpar.exe
```

Auth is whatever the environment already has: your `az login` session locally,
the federated OIDC identity in CI. Nothing here ever takes a password. Signing
itself needs the cross-platform [`sign`](https://github.com/dotnet/sign) tool,
which the script installs on first use if the .NET SDK is present.

## The release pipeline

`.github/workflows/ci.yml` runs on every push and PR: builds the game statically
on Linux and macOS, runs all four suites, and builds the promo site.

`.github/workflows/release.yml` runs on a push to **`prod`**:

1. **version** — an annotated tag on `HEAD` if there is one, else
   `v<date>-<sha>`. Release notes come from the commits since the last tag.
2. **verify** — all four suites. Nothing is published if anything fails.
3. **macOS** — static raylib for both arches, `lipo` into one universal binary,
   sign, notarize, staple, and build a `.dmg`.
4. **Windows** — MinGW cross-compile with the icon and version block linked in by
   `windres`, then Azure Trusted Signing from the same Linux runner.
5. **release** — checksums, generated notes, and a GitHub Release with the
   artifacts attached. Re-running replaces assets instead of failing.

Both signing steps degrade rather than fail: with no credentials configured the
release still publishes, just unsigned. A missing secret should not kill a build
at 90%.

`./scripts/setup-ci.sh` does the one-time setup — creates and protects `prod`,
pulls the notarisation key out of the keychain, and sets the secrets and
variables. `--show` prints what is currently configured.

| | | |
| --- | --- | --- |
| `ASC_KEY_P8`, `ASC_KEY_ID`, `ASC_ISSUER_ID` | secrets | notarisation, via an App Store Connect API key |
| `APPLE_TEAM_ID` | secret | the 10-character team id |
| `APPLE_CERT_P12`, `APPLE_CERT_PASSWORD` | secrets | the Developer ID certificate and its export password |
| `AZURE_CLIENT_ID`, `AZURE_TENANT_ID`, `AZURE_SUBSCRIPTION_ID` | variables | Azure OIDC — not secret |
| `APPLE_ID`, `APPLE_APP_PASSWORD` | secrets | only needed if there is no API key |

Two credentials, two jobs, and it is worth being precise about which does what:

- **Notarisation** uses an **App Store Connect API key**. Nothing to rotate, it
  can be revoked on its own without touching the certificate, and the same three
  values work locally and in CI. `sign-macos.sh` prefers it automatically and
  falls back to an Apple ID app-specific password if it cannot find one.
- **Signing** needs the **Developer ID certificate and its private key**. An API
  key cannot sign code. macOS also will not export a private key without a GUI
  prompt, so `--apple` walks you through that one step by hand; everything else
  is automatic.

Azure uses OIDC federation, so there is no client secret stored on GitHub at all.
The CI identity is scoped to exactly one certificate profile rather than the
subscription — the role is `Artifact Signing Certificate Profile Signer` (Azure
renamed Trusted Signing, and the old `Trusted Signing ...` role name no longer
resolves).

## The site

```sh
cd promo-site && npm install && npm run dev
```

Vue 3 + Vite, static output, hash routing so it works from a GitHub Pages
subpath. Adding a game is one entry in `src/data/games.js` plus a folder of
screenshots — see [`promo-site/README.md`](promo-site/README.md).

## Artwork

`art/icon-1024.png` and `art/keyart-1408.png` are the masters, generated with
Vertex AI and then recomposed. `scripts/make-icons.sh` derives everything else:

- `art/BreakPar.icns` — the macOS bundle icon. Capped at 512 px, because the
  full Apple set with an unoptimised 1024 frame came to 1.8 MB, larger than the
  floppy ceiling the whole game is built to.
- `art/breakpar.ico` — linked into `breakpar.exe` by `windres`. Capped at 128 px
  for the same reason; the executable has 42 KB of headroom to spare.
- `promo-site/public/games/breakpar/` — the site's icon, key art and social card.

An icon lives inside the executable or the bundle, so nothing is read from disk
at runtime and the games' no-asset-files rule still holds.
