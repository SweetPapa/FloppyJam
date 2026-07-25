/* Resolve the newest release at runtime, falling back to the build-time data.
 *
 * Asset names carry the version, so a download URL cannot be written as a
 * static "latest" link — which meant every release needed somebody to
 * regenerate releases.js and redeploy the site, and the site was wrong in
 * between. Asking the public releases API on mount removes that chase entirely.
 *
 * The baked-in `latest` from scripts/update-site-downloads.sh is still the
 * fallback and renders immediately, so the buttons are never empty and never
 * broken: unauthenticated api.github.com allows 60 requests an hour per IP, and
 * a rate-limited or offline visitor simply gets the last known release.
 */
import { ref, onMounted } from 'vue'
import { latest as fallback } from './releases.js'

const API = 'https://api.github.com/repos/SweetPapa/FloppyJam/releases/latest'

function mb(n) {
  return n >= 1024 * 1024 ? `${(n / 1024 / 1024).toFixed(2)} MB` : `${Math.round(n / 1024)} KB`
}

/* Same selection rule as the generator: prefer the zip, never the raw
 * breakpar.exe (that one is the unsigned build hand-off). */
function pick(assets, suffixes) {
  for (const s of suffixes) {
    const hit = assets.find((a) => a.name.endsWith(s) && a.name !== 'breakpar.exe')
    if (hit) return { name: hit.name, size: mb(hit.size), url: hit.browser_download_url }
  }
  return null
}

export function useLatestRelease() {
  const release = ref(fallback)

  onMounted(async () => {
    try {
      const res = await fetch(API, { headers: { Accept: 'application/vnd.github+json' } })
      if (!res.ok) return
      const d = await res.json()
      const assets = d.assets || []
      const windows = pick(assets, ['-windows-x64.zip', '-windows-x64.exe'])
      const macos = pick(assets, ['-macos.dmg', '-macos-universal.zip'])
      if (!windows || !macos) return // a partial release is worse than the fallback
      const sums = assets.find((a) => a.name === 'SHA256SUMS.txt')
      release.value = {
        tag: d.tag_name,
        notes: d.html_url,
        checksums: sums ? sums.browser_download_url : null,
        windows,
        macos,
      }
    } catch {
      /* offline, blocked or rate-limited — the fallback is already rendered */
    }
  })

  return release
}
