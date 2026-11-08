# D2R Melee Splash 0.3.6

Release date: 2026-11-08

## Changes

- Adds `skill_mode = "additive"`. Eligible melee skills use the summed item `meleesplash` stat, and a configured skill row adds its value to that total. For example, 25% from items plus `Whirlwind = 8` results in 33% splash for Whirlwind.
- Keeps `skill_mode = "all"` as the default. In that mode, a configured skill row replaces the item total. `skill_mode = "whitelist"` continues to limit splash to listed skills and use each row's value instead of item stats.
- Accepts mode names without case sensitivity and includes the mode behavior in the README and generated TOML comments.
- No hook, call-site patch, or validation witness changed from v0.3.5. See `HOOK_INVENTORY.md` for the verified targets.

## Compatibility and installation

Requires Windows x64 and D2RLoader with Plugin ABI 4 support. Install `d2rl-melee-splash.dll` in the active mod's `d2rloader/plugins/` folder.

On first launch, the installer backs up and edits the required mod data files. Restart D2RLoader after the first-run install so the game loads the new or edited files.

Because Blizzard's base data files are copyrighted, the release does not include them. If a target file is absent from the active mod, extract that file from your own Diablo II: Resurrected CASC and place it under `<active mod>/d2rloader/config/d2rl-melee-splash/base/`. Use the target filename shown in the README table (`itemstatcost.txt`, `properties.txt`, `magicsuffix.txt`, `item-modifiers.json`, or `item-nameaffixes.json`). Only extract files that are missing from the mod. Do not redistribute the extracted files.

## Binary checksum

Asset: `d2rl-melee-splash.dll`

SHA-256: `BA30C97E948BA930985DB32F0B824FA09E23AD8506171EC5E60D8B096A233B37`
