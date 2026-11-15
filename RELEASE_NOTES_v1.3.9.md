## Changes

- Melee Splash finds `item_melee_splash` by name in the game’s loaded tables for Classic, Lord of Destruction, and Reign of the Warlock. A mod can use a different stat ID without changing the DLL.
- Detailed combat logging is off by default to prevent hitches when a hit splashes onto several monsters.
- First-time setup checks for all five required loose files before making any edits. It backs up the files it changes and rolls back if a write fails. Existing splash rows and settings are reused.

## Install or upgrade

Requires Windows x64, a compatible D2RLoader with Plugin ABI 4 support, and an unpacked, writable mod. Tested with D2R **3.4.95032** and D2RLoader **1.3.1-beta**.

With the game closed, put `d2rl-melee-splash.dll` in `mods/<ModName>/d2rloader/plugins/`. If upgrading an existing install, replace the prior DLL and keep your TOML.

For first-time setup, keep the mod’s existing loose copies and extract any missing files from your own Diablo II: Resurrected CASC. Blizzard’s base data is copyrighted and is not included.

| Destination inside `mods/<ModName>/<ModName>.mpq/` | Required files |
| --- | --- |
| `data/global/excel/` | `itemstatcost.txt`, `properties.txt`, `magicsuffix.txt` |
| `data/local/lng/strings/` | `item-modifiers.json`, `item-nameaffixes.json` |

Launch D2RLoader with the mod selected. If the installer reports **first-run install complete**, exit and **restart D2RLoader** with the same mod so the game loads the updated files. If setup stops, the plugin log lists where to put each missing file.

## SHA-256

`23898BC8BE61265237A8F1D399009055B63817A611C3C3AE36F5D08D4CD41876`
