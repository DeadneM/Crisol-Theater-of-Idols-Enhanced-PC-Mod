# V0.9B completed-chapter reentry audit

Status: **experimental implementation, not gameplay-validated**.
Baseline: V0.8A camera/zoom/cinematic/startup code reported working by the user;
V0.8B input correction and requested suspend-control removal retained. V0.8B
CheatShop behavior is still awaiting user validation. Stable `main` remains V0.4A.

## Objective and release boundary

Permit the Fair's existing chapter 1 and 2 entrances after those chapters are
complete, and retain their existing return routes. Chapter 3 is already allowed
by the original `IsCorrectDoor` after EndChapter2; this build does not override it.
Success requires an actual visit AND return with existing progression retained.

The option is OFF by default. Enable **Allow revisiting completed chapters** in
the menu, then reload a hub save. Changing it after a level has already initialized
does not replay its door initialization. No new hotkey is introduced.

## Local assets: verified routes and flow

The actual installed IoStore packages were read without modifying containers.
Native script imports were resolved through the local `global.ucas` ScriptObjects
table, not assigned guessed names. All four relevant scripts were decoded to the
declared VM lengths: IsCorrectDoor 286, SetLockState 480, GetMapSoftObject 295,
ExecuteUbergraph_CR_Teleport_BP 3212 bytes.

The package's declared Blueprint property order, nested unversioned Transform
headers and CDO defaults were used to inspect placed actors:

| Scope | ChapterNum | MapToLoad | LoadLevel | Keep Player Data |
|---|---:|---:|---:|---:|
| Fair chapter 1 entrance | 1 | 2 | true | true |
| Fair chapter 2 entrance | 2 | 3 | true | true |
| Fair chapter 3 entrance | 3 | 4 | true | true |
| Chapter 1 return actor | 0 | 1 | true | true |
| Chapter 2 return actor | 0 | 1 | true | true |

`GetMapSoftObject` maps enum 1 to Lobby_Level, 2/3/4 to Chapter1/2/3_Level.
The native transition is `CRGameInstance.LoadMap(Map, KeepPlayerData,
CustomGabrielPosOnNewMap, LevelName, true)`; its last argument is **AutoSave**.
No replacement map names, spawn positions, level loads or actor teleports are
introduced. The original graph's ordinary side effects, including
`ResetGamesPrice`, remain in that graph and are not rewritten.

Return actors have a locked class-instance default. The narrow override therefore
also covers those two existing returns, but only when their chapter is completed.
No return is opened in an unfinished chapter. Chapter 3's return is not modified.

## Verified executable ABI, not imported offsets from another game

Required SHA-256:
`b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378`.
The full on-disk executable is hashed once during initialization, followed by
checks of the two native prologues. A mismatch disables only this feature.

| Finding | Verified evidence in the supported executable |
|---|---|
| UObject::ProcessInternal RVA `0147DA20` | Reads FFrame Node +10, Object +18, Code +20 and dispatches through GNatives; return opcode 04 and expression execution match the VM |
| ProcessLocalScriptFunction RVA `01467690` | Called by execLocalFinalFunction `0148E2A0`, with ProcessInternal supplied as its body callback |
| UFunction::Invoke RVA `01352730` | ProcessEvent `0147D520` calls it with UFunction, UObject, FFrame, result; it dispatches UFunction.Func +D8 |
| GetCRGameInstance RVA `04DCFC50` | exec wrapper `04C91A40` marshals one UObject context and stores its pointer result; getter delegates to GameplayStatics GetGameInstance `038A5F60` and performs a native class check |
| FName pool RVA `096E8600` | FName::ToString `01263160`: block index >>16, offset *2, block array +10, entry length >>6 and wide bit 1 |
| UObject fields | GetObjectName wrapper `039E8E80` uses name +18; getter/class checks use Class +10; Invoke uses function Outer +20 |
| FField/FProperty | Property iteration uses Next +18; FField names +20; ProcessEvent uses ArrayDim +30, ElementSize +34, PropertyFlags +38 and Offset_Internal +44 |
| UStruct | ProcessEvent obtains ChildProperties +50 and PropertiesSize +58; function Script is at +60, count +68 |
| Bool masks | Native bool serializer `0143E690` reads ByteOffset +71, ByteMask +72 and FieldMask +73; masks are honored, never assumed |
| Out parameter records | execLocalOutVariable `0148E2F0` uses FFrame.OutParms +80, record Property +0, PropAddr +8, Next +10 |
| Frame locals | FFrame constructor `014694F0` stores Locals +28 and Node +10; ProcessEvent verifies ParmsSize at UFunction +B6 |

All offsets above are hexadecimal. Class/function/field names, reflected field
types, dimensions, sizes, offsets and page permissions are checked at runtime.
The dynamic field offset is read from validated reflection metadata; no fixed
ChapterNum, Locked or completion-flag offsets are written into the mod.

## Behavior and safeguards

- Only `CR_Teleport_BP_C` objects owned by the exact verified Fair/chapter-1/
  chapter-2 package, or the matching chapter's generated partition cells, qualify.
- The UFunction must belong to that exact actor class and be IsCorrectDoor or
  SetLockState with the expected script length and one-byte parameter buffer.
- Current frame object must equal the actor; memory reads/writes are bounded.
- LoadLevel and Keep Player Data must both be true. ChapterNum/MapToLoad must
  match one of the five verified routes above.
- EndChapter1/EndChapter2 are READ ONLY. A later completed chapter also establishes
  that chapter 1 was completed; no future entrance gains access.
- IsCorrectDoor runs normally first; only its false Out parameter may become true.
- SetLockState runs normally with a temporarily unlocked local argument; the
  caller's argument bytes are restored afterward. No arbitrary actor-coordinate,
  mesh, physics, boss, puzzle, inventory, collection or achievement edit is made.
- Direct save-file writes are absent. The normal game transition still autosaves,
  and the game may persist ordinary door state; a pre-test SaveGames backup remains
  the rollback. This is not a guarantee that all runtime transitions are safe.
- Off means the original script runs untouched. The original behavior is retained
  for incomplete chapters, interior doors, other maps and failed checks.

## Validation and test procedure

The build runs 29 C++ policy cases covering unfinished chapters, future chapter 3,
wrong maps/scopes, disabled map-loading/player-data retention, and return routes.
These are policy checks, not a substitute for running the actual game.

1. Keep the pre-test SaveGames copy and V0.8B DLL/ASI rollback.
2. Enable the new option, reload the Fair save, inspect gate-control availability.
3. Try chapter 1 first. Check existing boss/puzzle/collectible progress, then return
   through its original chapter exit. Verify hub progress after return.
4. Only after a successful return, repeat chapter 2. Chapter 3 should retain normal
   access. Test zoom, FOV, cinematics and CheatShop separately.
5. Record counters/logs. A ready hook, successful build or queued permission is
   never labeled user validation. Do not promote to stable before confirmation.

The screenshot's incorrect map denominators and missing Solari row remain a
separate issue. V0.9B does not claim to repair them.

Format references: [CUE4Parse unversioned fragments](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/Assets/Objects/Unversioned/FFragment.cs),
[ScriptObjects imports](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/IO/IoGlobalData.cs),
[VM tokens](https://github.com/FabianFG/CUE4Parse/blob/master/CUE4Parse/UE4/Kismet/EExprToken.cs),
[patternsleuth FName analysis](https://github.com/trumank/patternsleuth/blob/master/patternsleuth/src/resolvers/unreal/fname.rs).
