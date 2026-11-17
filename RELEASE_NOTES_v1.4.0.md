## Changes

- First-time setup can fill in missing loose files from the shared `basefiles` folder. It uses your mod’s existing files when available and leaves the extracted originals untouched.
- Installer backups now go under `d2rloader/plugins/basefiles-and-backups/backups/d2rl-melee-splash/`. A `created-files.txt` lists any files added during setup.
- Removed per-hit debug logging. The `melee-splash` status command still shows counters and the verified stat IDs.

## Install or upgrade

Requires Windows x64, D2RLoader with Plugin ABI 4 support, and an unpacked, writable mod.

With the game closed, place `d2rl-melee-splash.dll` in `mods/<ModName>/d2rloader/plugins/`. To upgrade, replace the old DLL and keep your TOML and mod files.

If one of the five required files is missing from the mod, extract a copy from your own Diablo II: Resurrected CASC using Ladik’s Casc Viewer. Blizzard’s base files are copyrighted and are not included. Put the extracted files under `mods/<ModName>/d2rloader/plugins/basefiles-and-backups/basefiles/` using these paths, relative to the extracted `data/` folder:

| Folder under `basefiles/` | Files |
| --- | --- |
| `global/excel/` | `itemstatcost.txt`, `properties.txt`, `magicsuffix.txt` |
| `local/lng/strings/` | `item-modifiers.json`, `item-nameaffixes.json` |

Start D2RLoader with the mod selected. If the log reports **first-run install complete**, exit and **restart D2RLoader** with the same mod to load the changes.

## SHA-256

`67E68B1D2859C3F8AE9820D8EF496492BD5FB0C5F5FCA4A166C7923F63F31322`
