# D2R Melee Splash 0.3.5

Release date: 2026-11-08

## Changes

- Configure fixed splash percentages for individual skills, or enable whitelist mode so only listed skills splash. Skill keys accept names from the active mod's `Skills.txt` or numeric skill IDs.
- Configure separate default radii for normal weapons and exceptional/elite weapons, then optionally override the radius per skill.
- The generated TOML defaults to `skill_mode = "all"` and includes commented Whirlwind/Bash whitelist examples plus commented 8-tile radius examples.
- Adds skill-input hooks used to resolve the skill for whitelist and per-skill settings. See `HOOK_INVENTORY.md` for the release's checked hook and patch sites.

## Compatibility and installation

Requires Windows x64 and D2RLoader with Plugin ABI 4 support. Install `d2rl-melee-splash.dll` in the active mod's `d2rloader/plugins/` folder.

On first launch, the installer backs up and edits the required mod data files. If any target file is absent, supply its base copy extracted from the player's own CASC; see the README for exact paths and filenames. Restart D2RLoader after the first-run install so the game loads the new or edited files.

## Binary checksum

Asset: `d2rl-melee-splash.dll`

SHA-256: `CDD77F1E7658ADA77AE83EDC03EE864992392600E0564F7023764DF9D8D69AA3`

