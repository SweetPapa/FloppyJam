# Shared game translations

`catalog.json` is the UTF-8 source for iOS, Android, Mac Catalyst and the raylib
Windows/Linux desktop. English phrases are lookup keys. Every entry has all seven
translations in the declared language order. Existing game translations were
imported from SweetPapa/magLava `src/i18n/locales` (website checkout 5496b00);
native UI, campaign hints, renamed/new stages and desktop text extend that catalog.

Run `python3 v4/tools/gen_i18n.py` after editing. The generated C header is committed
so native builds need no network or JSON parser. `--check` verifies completeness,
format argument signatures and generated output; the shared test checks every
stage name and hint in every supported language. Unknown locales fall back to
English; regional Spanish/Portuguese variants map to es-419/pt-BR.

Desktop uses Barlow for Latin text and compact OFL Noto CJK Sans 2.004 derivatives
for Japanese/Chinese. `v4/tools/subset_fonts.py SOURCE_DIRECTORY` regenerates the
committed subsets with fontTools from the official Japanese/Chinese OTF files.
Their modified family names are MagLava Sans Japanese/Chinese; original copyright
records are retained. Distribute both OFL.txt and Noto-OFL.txt with the desktop.
Native UIKit/Android text uses the system's font fallback for CJK.
