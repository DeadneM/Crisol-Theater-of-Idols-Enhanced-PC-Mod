# V0.9 research: Fair chapter reentry and Solari #1 repair

**Status: read-only investigation. No door flag or save byte has been altered.**

This branch inherits V0.8A's automatic gameplay-camera FOV detection, two-file ZIP,
optional Skip Intro, and the existing experimental F1/F2/F3 work. The gameplay
mod is **unchanged** by this initial V0.9 investigation.

## 1. Reopen routes from Tormentosa Fair

Desired effect: the three entrances to the Mermaid / Ram / Anchor chapter
zones can be used even when those chapters were already visited. Nothing
else should be reset: bosses, chapter completion, puzzles, collectibles,
map progress, achievements or save data.

This is NOT the February 2026 save-load door-lock bug. The game deliberately
prevents some old-chapter backtracking:
https://steamcommunity.com/app/1790930/discussions/0/832739128180235612/

In particular, the chapter-1 entrance may be blocked by a guard's dialogue
rather than by the gate mesh's collision. An unconditional 'door open' bool
may be ineffective, or worse, permit the player to cross into an unloaded
map. The target must be the interactable/transition permission and the
corresponding destination map, after confirming the actual assets.

**Required from gameplay before implementation:** identify the Fair's three
interaction actors and their exact map/level destination names from the
user's PAK assets or a focused in-game object probe. Do not patch guessed
UE object offsets. A reentry must have a verified return path to the Fair
to avoid new soft-locks.

## 2. Solari Brooch #1 vs. Chest #1

Location: the southernmost room in the central section of the Fair. Absorb
the nearby corpse to receive the Solari brooch, then open chest #1:
https://www.100pguides.com/guides/crisol-all-collectible-locations

Two distinct problems:
- If the numbered chest is locked, the brooch/inventory state may be absent
  even though the corpse cannot be reabsorbed.
- If the chest opens/has opened but the counter is wrong, the progress
  record may be wrong instead of the inventory. Do not duplicate rewards.

Both Solari chests and collectible tracking have had official bug fixes:
https://steamdb.info/patchnotes/22179245/
https://steamdb.info/patchnotes/23240261/

### Save backups and read-only inspection

Close the game; copy the whole directory
`%LOCALAPPDATA%\CRToiPrototype\Saved\SaveGames\` to a different folder.
Steam's Cloud path uses this exact directory for `*.sav`:
https://steamdb.info/app/1790930/ufs/

You may upload the relevant `CRGame*.sav` backup to the project discussion
for private analysis. Do NOT commit player saves to public GitHub.

Optional locally run **read-only** first-pass scan:

```powershell
python tools/inspect_crisol_save.py
python tools/inspect_crisol_save.py "C:\\backup\\CRGame01.sav"
python tools/inspect_crisol_save.py --compare "C:\\before\\CRGame01.sav" "C:\\after\\CRGame01.sav"
```

This only prints magic bytes, SHA-256, candidate term offsets, and (if asked)
which file blocks differ. It **cannot** infer or repair flags by itself.
If no readable terms are found, serialized IDs/compression/encryption may
require deeper Unreal SaveGame structure analysis.

### Save repair criteria

Identify the actual persisted owner/field for Brooch 1, the actual opened
state of Chest 1, and whether the collection count is held in a separate
profile/Steam state. Use backups and produce a separate proposed fixed save,
never silently overwrite or touch all Solari state at once. A one-byte
`1` value must **not** be guessed without verifying encoding, checksums
and schema.

## Rollback

Validated independent World/FPS FOV baseline: `main` V0.4A.
More recent experimental features remain isolated on development branches.
