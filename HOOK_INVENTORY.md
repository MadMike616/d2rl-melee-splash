# Hook Inventory — D2R Melee Splash

## Plugin metadata

| Field | Value |
|---|---|
| Plugin name | Melee Splash |
| Plugin ID | d2rl-melee-splash |
| Release version | 0.3.7 |
| Inventory updated | 2026-11-08 |
| Source revision | v0.3.7 (release tag) |
| D2R build(s) audited | Not validated |
| D2RLoader SDK / Plugin ABI | PluginSDK 0.4.0 / Plugin ABI 4 |
| Audit status | Partial — source sites audited; in-game target-build validation not performed |

## Scope and method

Inspected src/plugin.cpp, src/pre_conversion.asm, src/plugin.rc, build-plugin.cmd, README.md, and RELEASE_NOTES_v0.3.7.md. This inventory covers plugin-owned inline detours and call-site patches. D2RLoader callbacks and native functions called only as helpers are not hook sites. The v0.3.7 rebuild changes compile/link settings and version strings only; hook sites and validation witnesses match v0.3.6.

All addresses are RVAs relative to the D2R executable module base and are written in hexadecimal. The plugin checks expected instruction bytes and structural witnesses before patching. Inline hooks use SDK InstallInlineHook; call-site redirects encode a rel32 CALL to a nearby relay and apply it through expected-byte-checked PatchBytes. Physical-conversion and source-roll call patches are conditional on configuration; skill-input hooks are native executable inline hooks, not OS input hooks.

## Hook sites

| Site ID | Type | Target | Address / identifier | Operation | Guard / validation | Source reference | Notes |
|---|---|---|---|---|---|---|---|
| H-01 | Inline | FillDamageValues | RVA 0x44C030 | InstallInlineHook to HookFill | FillExpected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-02 | Inline | AllocateCombatRecord | RVA 0x4507B0 | InstallInlineHook to HookAllocate | AllocateExpected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-03 | Inline | ConsumeCombatRecord | RVA 0x44B2B0 | InstallInlineHook to HookConsume | ConsumeExpected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-04 | Inline | ExecuteEvents | RVA 0x44CE80 | InstallInlineHook to HookExecute | ExecuteExpected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-05 | Inline | EventFunc7 | RVA 0x583E00 | InstallInlineHook to HookEvent7 | Event7Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-06 | Inline | EventFunc9 | RVA 0x583400 | InstallInlineHook to HookEvent9 | Event9Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-07 | Inline | EventFunc14 | RVA 0x583580 | InstallInlineHook to HookEvent14 | Event14Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-08 | Inline | EventFunc15 | RVA 0x584170 | InstallInlineHook to HookEvent15 | Event15Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Same entry also hooked by RuffDood Cast Triggers in the current workspace inventory. |
| H-09 | Inline | EventFunc16 | RVA 0x583150 | InstallInlineHook to HookEvent16 | Event16Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Same entry also hooked by RuffDood Cast Triggers in the current workspace inventory. |
| H-10 | Inline | EventFunc19 | RVA 0x5849D0 | InstallInlineHook to HookEvent19 | Event19Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-11 | Inline | EventFunc20 | RVA 0x583B30 | InstallInlineHook to HookEvent20 | Event20Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Same entry also hooked by RuffDood Cast Triggers in the current workspace inventory. |
| H-12 | Inline | EventFunc21 | RVA 0x5837F0 | InstallInlineHook to HookEvent21 | Event21Expected bytes checked before mutation and by the SDK hook install | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Installed when plugin is enabled. |
| H-13 | Inline | Player position-skill input | RVA 0x4FDB40 | InstallInlineHook to HookPlayerSkillPositionInput | PlayerSkillPositionInputExpected and active-skill layout witness at RVA 0x33DBA0 checked before mutation | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Resolves skill identity for whitelist and per-skill settings. Native game routine, not a Win32 input hook. |
| H-14 | Inline | Player unit-skill input | RVA 0x4F8DE0 | InstallInlineHook to HookPlayerSkillUnitInput | PlayerSkillUnitInputExpected and active-skill layout witness at RVA 0x33DBA0 checked before mutation | src/plugin.cpp — ValidateOwnedSites / InstallHooks | Resolves skill identity for whitelist and per-skill settings. Native game routine, not a Win32 input hook. |
| H-15 | CALL | Pre-critical damage capture | RVA 0x44C2AC | PatchBytes writes rel32 CALL to relay slot 0 | PreCriticalContextExpected and PreCriticalCallExpected checked before mutation | src/plugin.cpp — ValidateOwnedSites / InstallHooks / PatchCallToRelay | Installed when plugin is enabled. |
| H-16 | CALL | Pre-Event3 capture | RVA 0x44CB9E | PatchBytes writes rel32 CALL to relay slot 1 | PreEvent3ContextExpected and PreEvent3CallExpected checked before mutation | src/plugin.cpp — ValidateOwnedSites / InstallHooks / PatchCallToRelay | Installed when plugin is enabled. |
| H-17 | CALL | Pre-conversion capture | RVA 0x44CBA3 | PatchBytes writes rel32 CALL to MeleeSplashPreConversionRelay | PreConversionExpected context checked before mutation; patch extent is 8 bytes | src/plugin.cpp / src/pre_conversion.asm — ValidateExtensionSites / InstallHooks | Conditional: physical conversion is enabled by default settings, or a skill override requests weapon-source capture. Relay preserves state and replays the displaced MOVZX. |
| H-18 | CALL | Elemental source roll 1 | RVA 0x44C611 | PatchBytes writes rel32 CALL to HookSourceRoll relay | SourceRollContexts[0] checked before mutation | src/plugin.cpp — ValidateExtensionSites / InstallHooks / SourceRollCalls | Conditional: a skill override requests weapon-source-only capture. |
| H-19 | CALL | Elemental source roll 2 | RVA 0x44C679 | PatchBytes writes rel32 CALL to HookSourceRoll relay | SourceRollContexts[1] checked before mutation | src/plugin.cpp — ValidateExtensionSites / InstallHooks / SourceRollCalls | Conditional: a skill override requests weapon-source-only capture. |
| H-20 | CALL | Elemental source roll 3 | RVA 0x44C6E1 | PatchBytes writes rel32 CALL to HookSourceRoll relay | SourceRollContexts[2] checked before mutation | src/plugin.cpp — ValidateExtensionSites / InstallHooks / SourceRollCalls | Conditional: a skill override requests weapon-source-only capture. |
| H-21 | CALL | Elemental source roll 4 | RVA 0x44C749 | PatchBytes writes rel32 CALL to HookSourceRoll relay | SourceRollContexts[3] checked before mutation | src/plugin.cpp — ValidateExtensionSites / InstallHooks / SourceRollCalls | Conditional: a skill override requests weapon-source-only capture. |
| H-22 | CALL | Poison source roll | RVA 0x44CA47 | PatchBytes writes rel32 CALL to HookSourceRoll relay | PoisonRollContext checked before mutation | src/plugin.cpp — ValidateExtensionSites / InstallHooks / SourceRollCalls | Conditional: a skill override requests weapon-source-only capture. |

## Known collision overlaps

List only overlaps supported by the current source audit. Include the other plugin and shared target. If none are known, write “None identified in the current inventory; this is not a compatibility guarantee.”

| Other plugin | Shared target | Relationship | Evidence / notes |
|---|---|---|---|
| RuffDood Cast Triggers | EventFunc15, RVA 0x584170 | Same inline entry | Source directory plugins/cast-triggers; listed in the workspace aggregate inventory. |
| RuffDood Cast Triggers | EventFunc16, RVA 0x583150 | Same inline entry | Source directory plugins/cast-triggers; listed in the workspace aggregate inventory. |
| RuffDood Cast Triggers | EventFunc20, RVA 0x583B30 | Same inline entry | Source directory plugins/cast-triggers; listed in the workspace aggregate inventory. |

## Audit notes

Two validation-only locations are not patched: ActiveSkillLayoutWitnessRva at RVA 0x33DBA0 and DirectMeleeFillContextRva at RVA 0x43009E. The source contains no vtable hooks or OS-level input/message hooks. Expected-byte checks fail closed before mutation when a required fingerprint does not match. D2R build-specific runtime behavior was not validated in-game for this release.
