# D2R Melee Splash

D2RLoader plugin that adds configurable melee splash damage to Diablo II: Resurrected. The `meleesplash` item stat is additive: values on charms and equipped items sum together, up to a total of 1000%.

## What it does

- A successful supported melee hit applies a scaled copy of that hit to nearby monsters. The primary hit must hit a monster; misses do not splash.
- Splash copies the primary hit's damage packet, including supported physical-to-element conversions. Berserk's converted physical damage is applied as magic damage; separately added elemental damage retains its type.
- The plugin disables the game's legacy splash callback and uses a 0% base value, so the `meleesplash` item stat controls the total.
- The default radius is configurable separately for normal weapons and exceptional/elite weapons; individual skills can override it in the plugin TOML.
- Skill-specific splash percentages can be configured in the plugin TOML with `all`, `additive`, or `whitelist` mode.
- The plugin supports normal attacks and eligible melee skills. Unsupported executable layouts fail closed without installing hooks.

## Install

Requires D2RLoader with Plugin ABI 4 support. Put `d2rl-melee-splash.dll` in the active mod's `d2rloader/plugins` folder, then launch the game through D2RLoader. The active mod's data files must be writable for the one-time installer to run.

After the first-run install completes, exit and restart D2RLoader before testing the mod. The restart lets the game load the newly created or edited data files; launch the mod again after restarting.

On first load, the plugin backs up and merges only these five files:

- `data/global/excel/itemstatcost.txt`
- `data/global/excel/properties.txt`
- `data/global/excel/magicsuffix.txt`
- `data/local/lng/strings/item-modifiers.json`
- `data/local/lng/strings/item-nameaffixes.json`

If a target file is absent from the active mod, the installer needs its base version to create a complete data file. Because the base files are copyrighted Blizzard game data, the release does not redistribute them. Extract the corresponding files from your own Diablo II: Resurrected CASC with a CASC extraction tool, then place them under the active mod's `d2rloader/config/d2rl-melee-splash/base/` folder using these filenames:

| CASC path to extract | Filename in the `base` folder |
| --- | --- |
| `data/global/excel/itemstatcost.txt` | `itemstatcost.txt` |
| `data/global/excel/properties.txt` | `properties.txt` |
| `data/global/excel/magicsuffix.txt` | `magicsuffix.txt` |
| `data/local/lng/strings/item-modifiers.json` | `item-modifiers.json` |
| `data/local/lng/strings/item-nameaffixes.json` | `item-nameaffixes.json` |

For example, if your active mod is `MyMod`, put them in `mods/MyMod/d2rloader/config/d2rl-melee-splash/base/`. Only files missing from the active mod need a matching extracted base file; if all five target files already exist, no CASC extraction is needed. The installer reads these files and adds the splash rows when it creates missing targets. It does not modify the extracted copies. Do not redistribute the extracted base files. Existing mod files are backed up before editing; the backup folder includes `created-from-casc-base.txt` listing targets created from the supplied files.

The installer selects an unused stat ID, preferring 398, and an unused property ID, preferring 314. It adds the stat/property rows, localized tooltip and suffix strings, and the charm suffix rows: small charms roll 1–3%, large charms 2–6%, and grand charms 3–9%. It does not edit unique, set, or other item assignment tables; assign the property to other items in your mod's item data files if desired.

Backups are stored under `d2rloader/backups/d2rl-melee-splash/`. The marker `d2rloader/config/d2rl-melee-splash.toml` records the selected IDs and prevents repeat edits. Deleting the marker reruns the installer; it does not restore the backups. To uninstall, remove the DLL, restore the five files from a backup, and remove the marker.

## Skill-specific splash settings

The same `d2rloader/config/d2rl-melee-splash.toml` file can assign a fixed splash percentage to individual skills. Use the internal skill name from the active mod's `Skills.txt` `skill` column, or its numeric `*Id`:

```toml
[melee_splash]
skill_mode = "all" # "all", "additive", or "whitelist"
normal_weapon_radius_tiles = 4
exceptional_elite_weapon_radius_tiles = 5

[melee_splash.skills]
Whirlwind = 100
Bash = 100
"Left Hand Swing" = 50 # Quote names that contain spaces
151 = 100 # Numeric Skills.txt *Id is still accepted

[melee_splash.skill_radius_tiles]
Whirlwind = 8 # This skill uses an 8-tile radius with either weapon tier
Bash = 8
```

Skill names are case-insensitive and resolve from the active mod's `Skills.txt` `skill` column. If `Skills.txt` is absent, the plugin checks D2RLoader's compiled mod data. Numeric `*Id` keys remain available when no skill table can be found.

The `skill_mode` value is case-insensitive.

In `all` mode, eligible melee skills use the summed item `meleesplash` stat unless they have a row; a listed skill uses exactly its row's percentage and ignores the item stat. In `additive` mode, every eligible melee skill uses the summed item stat, and a listed skill adds its row's percentage on top. For example, 25% from items plus `Whirlwind = 8` produces 33% splash for Whirlwind. In `whitelist` mode, only skills with rows in `[melee_splash.skills]` splash, each using exactly its row's percentage and ignoring the item stat. Values can be 0–1000; 0 adds nothing in `additive`, disables the listed skill in `all`, and yields no splash for that skill in `whitelist`.

`normal_weapon_radius_tiles` and `exceptional_elite_weapon_radius_tiles` set the default radius by weapon tier (defaults: 4 and 5 tiles). A skill listed under `[melee_splash.skill_radius_tiles]` uses that radius instead of either weapon-tier default. Radius values can be 0–32767 tiles. Radius entries only change the radius: they do not enable a skill or add it to the whitelist. To whitelist a skill, also add its fixed splash percentage under `[melee_splash.skills]`.

The plugin reads these settings when it loads. Restart D2RLoader after editing the TOML file.

The first-run TOML uses `skill_mode = "all"` by default. It includes commented Whirlwind and Bash examples under `[melee_splash.skills]` for trying any of the three modes, plus commented 8-tile per-skill radius examples.

The first-run installer shares a mutex with Quest Reward Reset 1.0.24 and later, so those two plugins serialize their initial edits and backups when they start together.

## Diagnostics

The `melee-splash` status command reports counters and hook state. Loader logs report damage-packet capture, conversion type, and secondary-hit results. The plugin checks expected native code before installing hooks and skips unsupported executable layouts.

## Build

Use D2RLoader PluginSDK 0.4.0 / Plugin ABI 4, Windows x64, and an x64 Visual Studio Developer Command Prompt. Run `build-plugin.cmd <path-to-PluginSDK-SDK-0.4.0>` from the repository; the script writes build output under the system temporary directory. The SDK is available from the [official PluginSDK repository](https://github.com/D2RLoader/PluginSDK).

## Third-party source

The native damage-packet capture and secondary-hit implementation adapts RuffnecKk's `MeleeSplash` source, with explicit permission from the original coder. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for attribution. The expected-byte signatures for skill input hooks were verified against RuffDood's D2RLoader Suite; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
