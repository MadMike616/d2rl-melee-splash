# D2R Melee Splash 0.3.1

This release lets the first-run installer create any of its five target files that are missing from the active mod, using base files supplied by the player. Existing mod files are backed up before changes.

## Supplying base files

Because these files are copyrighted Blizzard game data, the release does not redistribute them. If a target file is missing from your mod, extract its base copy from your own Diablo II: Resurrected CASC and put it in:

`mods/<ActiveMod>/d2rloader/config/d2rl-melee-splash/base/`

Use these CASC paths and filenames:

| CASC path to extract | Filename in the `base` folder |
| --- | --- |
| `data/global/excel/itemstatcost.txt` | `itemstatcost.txt` |
| `data/global/excel/properties.txt` | `properties.txt` |
| `data/global/excel/magicsuffix.txt` | `magicsuffix.txt` |
| `data/local/lng/strings/item-modifiers.json` | `item-modifiers.json` |
| `data/local/lng/strings/item-nameaffixes.json` | `item-nameaffixes.json` |

Only the base files corresponding to missing mod files are required. If all five targets already exist in the mod, no extraction is needed. Keep the extracted files for your own use; do not redistribute them.

After the first-run installer finishes, exit and restart D2RLoader so the game loads the new or edited data files.

## Other changes

- The installer still selects unused stat and property IDs, preferring 398 and 314.
- It still adds the splash stat strings and suffix rows for small (1–3%), large (2–6%), and grand (3–9%) charms.
- It does not edit item assignment tables.

## Install

Requires Windows x64 and D2RLoader with Plugin ABI 4 support. Put `d2rl-melee-splash.dll` in the active mod's `d2rloader/plugins` folder. After the first-run installer finishes, exit and restart D2RLoader so the game loads the new or edited files.

## SHA-256

`6DC7AE662FD7CCB2CA3662E59CDA32A32A8C42D3EE65277300287B72BC5B100B`

