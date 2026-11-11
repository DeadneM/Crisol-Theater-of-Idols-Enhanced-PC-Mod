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

## Local evidence collected 2026-11-11

Status: **CONTROLLED read-only evidence; no new gameplay BUILD; no user VALIDATION
of reentry.** The executable SHA-256 matches the supported
`b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378`.
The installed mod's last startup log identifies V0.7A, not V0.8A.
The existing INI, shortcut and binaries were preserved.

The full SaveGames directory and installed mod/INI/logs were copied locally before
analysis. Every copied save's SHA-256 matched its source. No save, proprietary asset,
or private report is included in the repository.

### Confirmed chapter-permission logic

The local, unencrypted IoStore directory index was completely consumed and checked
against its declared length. Only seven targeted packages were extracted (about
16 MB, with cached outputs), including the hub and `CR_Teleport_BP`.
Container files were opened read-only; the 15 GB UCAS was not downloaded or copied.

`CR_Teleport_BP` contains `Locked`, `ChapterNum`, `MapToLoad`, `DoorIsOpen`,
`IsCorrectDoor`, `SetLockState`, `GetMapSoftObject`, and native transitions.
The complete serialized script of `IsCorrectDoor` was decoded (426 disk bytes,
286 VM-memory bytes). Its property accesses and integer comparisons establish:

```text
if GameInstance.EndChapter2:
    Out = (ChapterNum == 3)
else if GameInstance.EndChapter1:
    Out = (ChapterNum == 2)
else:
    Out = (ChapterNum == 1)
```

This is concrete evidence for a progression-based permission, not proof that
overriding it alone safely restores travel. `SetLockState`, map selection,
transition side effects and chapter-to-hub returns still require inspection.
Do not clear `EndChapter1` / `EndChapter2`: these are progression state.
Do not globally bypass every teleport interaction.

The inspected hub save contains three `CR_Teleport_BP` actor records with
33-byte payloads. Their byte meanings have NOT been mapped to Blueprint fields.

### Solari #1: persisted evidence, no justified repair

The user explicitly confirms the corresponding chest is **already open**.
The local `AchievementData.sav` has `ach_all_solari` entries
`5, 2, 3, 4, 6, 1, 7, 8, 9, 10, 11`: ID `1` is present.
`ach_all_bloodechoes` also includes `SecretAdmIrerI` and `SecretAdmIrerII`.
The local `CR_AcolyteBrooch_BP0` package names `SecretAdmIrerI`;
`CR_AcolyteBrooch_BP1` names `WithoutRemorse`. Internal BP numbering is therefore
not a safe substitute for player-facing numbering.

These are persisted local records, not verification of the inventory UI, current
Steam achievement state, or every counter. A missing item after use may be distinct
from missing collection credit. No duplicate key, reward or achievement is granted.

### Repeatable read-only inspection

```powershell
python tools/inspect_crisol_records.py "C:\backup\CRGame01.sav"
python tools/inspect_crisol_records.py "C:\backup\CRGame01.sav" --compare "C:\backup\AutoSave.sav"
python tools/inspect_crisol_records.py "C:\backup\AchievementData.sav"
python -m unittest discover -s tools -p "test_inspect_crisol_records.py"
```

Actual local checks: 19 targeted records in CRGame01 (3 teleports, 15 chests,
1 absorbable corpse); all 19 calculated record ends land at the following valid
class FString. Corpse and nearest chest positions differ by 5.58 game units.
The achievement reader supports both UTF-8 and UTF-16 strings and retains its
uninterpreted trailer. Six safety tests pass, including every truncation of
the sample records and achievement maps. None of these checks is an in-game test.

Format references used for read-only resource analysis:
[CUE4Parse IoStore](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/IO/Objects/FIoStoreTocResource.cs),
[Zen export entries](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/IO/Objects/FExportMapEntry.cs),
[Kismet tokens](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/Kismet/EExprToken.cs).
