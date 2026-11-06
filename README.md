# D2R Melee Splash

D2RLoader plugin that adds configurable melee splash damage to Diablo II: Resurrected. The `meleesplash` item stat is additive: values on charms and equipped items sum together, up to a total of 1000%.

## What it does

- A successful supported melee hit applies a scaled copy of that hit to nearby monsters. The primary hit must hit a monster; misses do not splash.
- Splash copies the primary hit's damage packet, including supported physical-to-element conversions. Berserk's converted physical damage is applied as magic damage; separately added elemental damage retains its type.
- The plugin disables the game's legacy splash callback and uses a 0% base value, so the `meleesplash` item stat controls the total.
- The radius is 4 tiles with a normal weapon and 5 tiles with an exceptional or elite weapon.
- The plugin supports normal attacks and eligible melee skills. Unsupported executable layouts fail closed without installing hooks.

## Install

Requires D2RLoader with Plugin ABI 4 support. Put `d2rl-melee-splash.dll` in the active mod's `d2rloader/plugins` folder, then launch the game through D2RLoader. The active mod's data files must be writable for the one-time installer to run.

On first load, the plugin backs up and merges only these five files:

- `data/global/excel/itemstatcost.txt`
- `data/global/excel/properties.txt`
- `data/global/excel/magicsuffix.txt`
- `data/local/lng/strings/item-modifiers.json`
- `data/local/lng/strings/item-nameaffixes.json`

The installer selects an unused stat ID, preferring 398, and an unused property ID, preferring 314. It adds the stat/property rows, localized tooltip and suffix strings, and the charm suffix rows: small charms roll 1–3%, large charms 2–6%, and grand charms 3–9%. It does not edit unique, set, or other item assignment tables; assign the property to other items in your mod's item data files if desired.

Backups are stored under `d2rloader/backups/d2rl-melee-splash/`. The marker `d2rloader/config/d2rl-melee-splash.toml` records the selected IDs and prevents repeat edits. Deleting the marker reruns the installer; it does not restore the backups. To uninstall, remove the DLL, restore the five files from a backup, and remove the marker.

The first-run installer shares a mutex with Quest Reward Reset 1.0.24 and later, so those two plugins serialize their initial edits and backups when they start together.

## Diagnostics

The `melee-splash` status command reports counters and hook state. Loader logs report damage-packet capture, conversion type, and secondary-hit results. The plugin checks expected native code before installing hooks and skips unsupported executable layouts.

## Build

Use D2RLoader PluginSDK 0.4.0 / Plugin ABI 4, Windows x64, and an x64 Visual Studio Developer Command Prompt. Run `build-plugin.cmd <path-to-PluginSDK-SDK-0.4.0>` from the repository; the script writes build output under the system temporary directory. The SDK is available from the [official PluginSDK repository](https://github.com/D2RLoader/PluginSDK).

## Third-party source

The native damage-packet capture and secondary-hit implementation adapts RuffnecKk's `MeleeSplash` source, with explicit permission from the original coder. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for attribution and reuse scope.
