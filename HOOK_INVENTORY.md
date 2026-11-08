# D2R Melee Splash hook inventory

Updated for release **v0.3.5**, 2026-11-08. This inventory is derived from `src/plugin.cpp` and records executable mutation sites only. RVAs are relative to the D2R executable module base.

Every installed site is checked against expected bytes before mutation. Inline hooks use D2RLoader SDK `InstallInlineHook`; call-site changes use expected-byte-checked `PatchBytes` to a nearby relay. The plugin fails closed if a required signature or structure witness does not match.

## Inline hooks installed by v0.3.5

| Target | RVA | Handler |
| --- | ---: | --- |
| FillDamageValues | `0x44C030` | `HookFill` |
| AllocateCombatRecord | `0x4507B0` | `HookAllocate` |
| ConsumeCombatRecord | `0x44B2B0` | `HookConsume` |
| ExecuteEvents | `0x44CE80` | `HookExecute` |
| EventFunc7 | `0x583E00` | `HookEvent7` |
| EventFunc9 | `0x583400` | `HookEvent9` |
| EventFunc14 | `0x583580` | `HookEvent14` |
| EventFunc15 | `0x584170` | `HookEvent15` |
| EventFunc16 | `0x583150` | `HookEvent16` |
| EventFunc19 | `0x5849D0` | `HookEvent19` |
| EventFunc20 | `0x583B30` | `HookEvent20` |
| EventFunc21 | `0x5837F0` | `HookEvent21` |
| Player position-skill input | `0x4FDB40` | `HookPlayerSkillPositionInput` |
| Player unit-skill input | `0x4F8DE0` | `HookPlayerSkillUnitInput` |

The last two inline hooks were added after v0.3.1 to identify skill names/IDs for whitelist and per-skill settings.

## Call-site patches

| Call site | RVA | Relay target | Condition |
| --- | ---: | --- | --- |
| Pre-critical damage capture | `0x44C2AC` | `HookPreCriticalBonuses` | Always |
| Pre-Event3 capture | `0x44CB9E` | `HookPreEvent3` | Always |
| Pre-conversion capture | `0x44CBA3` | `MeleeSplashPreConversionRelay` | Physical conversion is enabled by the plugin defaults; also needed for weapon-source capture |
| Elemental source-roll calls | `0x44C611`, `0x44C679`, `0x44C6E1`, `0x44C749`, `0x44CA47` | `HookSourceRoll` | Only when a skill override requests weapon-source capture; not enabled by the shipped TOML settings |

## Validation-only locations

These locations are checked as structural witnesses and are not patched:

- Active-skill record layout witness: `0x33DBA0`
- Direct melee Fill call context: `0x43009E`

The source contains no vtable hooks or OS-level input hooks. The skill-input hooks above are inline hooks on game executable routines.

