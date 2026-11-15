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

These instructions apply to the current source. If you downloaded a
published DLL, use the installation instructions for that version on the
[Releases page](https://github.com/MadMike616/d2rl-melee-splash/releases).

You need D2RLoader with Plugin ABI 4 support and an unpacked, writable mod.
Melee Splash v1.3.9 was tested with D2R 3.4.95032 and D2RLoader 1.3.1-beta.
In the standard layout, the mod's loose data is under
`mods/<ModName>/<ModName>.mpq/data/`, where `<ModName>.mpq` is a directory.

1. Keep the mod's existing loose copies of the five files below. Extract
   any missing originals from your own game's CASC into the matching
   directory under the mod's `<ModName>.mpq` folder. Blizzard's base data
   is copyrighted and is not bundled with the DLL.
2. With the game closed, copy `d2rl-melee-splash.dll` to
   `mods/<ModName>/d2rloader/plugins/`. Start D2RLoader with that mod selected.
3. Look for **first-run install complete** in
   `mods/<ModName>/d2rloader/logs/d2rl-melee-splash.log`. Exit and restart
   D2RLoader with the same mod so the game loads the changed files.
   Splash becomes active on that next launch.

| Directory inside `<ModName>.mpq` | Required files |
| --- | --- |
| `data/global/excel/` | `itemstatcost.txt`, `properties.txt`, `magicsuffix.txt` |
| `data/local/lng/strings/` | `item-modifiers.json`, `item-nameaffixes.json` |

If any files are missing, setup stops before editing the mod. The plugin
log lists every missing file and its full destination path. Supply those
files and launch again. If you previously extracted files to the plugin's
old `d2rloader/config/d2rl-melee-splash/base/` folder, copy any still-missing
files into the matching loose data directories above.

The DLL contains the rows and translations it merges. It prefers stat ID
398 and property ID 314 and selects unused IDs when those are taken. It
checks the mod's loose localization files before assigning string IDs.
The charm suffixes roll 1–3% on small charms, 2–6% on large charms, and
3–9% on grand charms. Mod authors can assign `meleesplash` to other
items in their own item tables.

The installer backs up each file it changes under
`mods/<ModName>/d2rloader/backups/d2rl-melee-splash/<timestamp>/` before
writing. If a write fails, it attempts to restore changes made in that
run. It shares an install lock with The Unruned Oath and Quest Reward Reset
1.0.24 or later.

To upgrade, close the game, replace the DLL, and start D2RLoader with the
same mod. The plugin recognizes existing or manually staged splash data
and keeps its IDs, custom rows, strings, and TOML settings. A restart is
only required if the installer reports that it changed the mod files.
The old `[installation]` TOML section is optional; the mod data itself
determines whether installation is needed. On each data table load the
DLL verifies `item_melee_splash` by name in the compiled tables for
Classic, LoD, and Reign of the Warlock. If it cannot verify the stat for
the character's game mode, item splash is disabled for that mode.
Fixed skill values can still apply.

To uninstall the data, first remove splash items from characters and
stashes that will continue to use the mod. With the game closed, remove
the DLL and restore the affected files from the install backup, merging
any later mod edits you want to keep. Older backups may include a
`created-from-casc-base.txt` list of files created by the old installer;
remove those files or merge any later changes you want to keep.

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

## Diagnostics

The `melee-splash` status command reports counters, hook state, and
verified stat IDs. Startup messages and warnings are always logged.
Detailed combat logging is off by default because writing a message
for each hit and target can cause hitches. To collect a short combat
trace for troubleshooting, set `diagnostic_logging = true` under
`[melee_splash]` and restart D2RLoader. Set it back to `false` when done.

## Build

Use D2RLoader PluginSDK 0.4.0 / Plugin ABI 4, Windows x64, and an x64 Visual Studio Developer Command Prompt. Run `build-plugin.cmd <path-to-PluginSDK-SDK-0.4.0>` from the repository; it builds with MSVC Release optimization (`/O2 /Ob2 /DNDEBUG`), the static release runtime (`/MT`), and linker optimizations, without debug information. Pass an optional output directory after the SDK path; otherwise build output goes under the system temporary directory. The SDK is available from the [official PluginSDK repository](https://github.com/D2RLoader/PluginSDK).

## Third-party source

The native damage-packet capture and secondary-hit implementation adapts RuffnecKk's `MeleeSplash` source, with explicit permission from the original coder. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for attribution. The expected-byte signatures for skill input hooks were verified against RuffDood's D2RLoader Suite; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
