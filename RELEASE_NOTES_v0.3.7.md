# D2R Melee Splash 0.3.7

Release date: 2026-11-08

## Changes

- Reissues the v0.3.6 feature set as an explicit optimized Release build: `/O2 /Ob2 /DNDEBUG`, static release runtime `/MT`, and linker optimizations `/OPT:REF /OPT:ICF`.
- No plugin behavior, native hook site, call-site patch, or validation witness changed from v0.3.6.
- Documents the Release build flags in the README and build script.

## Compatibility and installation

Requires Windows x64 and D2RLoader with Plugin ABI 4 support. Install `d2rl-melee-splash.dll` in the active mod's `d2rloader/plugins/` folder.

On first launch, the installer backs up and edits the required mod data files. Restart D2RLoader after the first-run install so the game loads the new or edited files.

Because Blizzard's base data files are copyrighted, the release does not include them. If a target file is absent from the active mod, extract that file from your own Diablo II: Resurrected CASC and place it under `<active mod>/d2rloader/config/d2rl-melee-splash/base/`. Use the target filename shown in the README table (`itemstatcost.txt`, `properties.txt`, `magicsuffix.txt`, `item-modifiers.json`, or `item-nameaffixes.json`). Only extract files that are missing from the mod. Do not redistribute the extracted files.

## Binary checksum

Asset: `d2rl-melee-splash.dll`

SHA-256: `EDFA3E75219240C2021CF569A745C0581581CEDACE400CA43651395A3916F314`
