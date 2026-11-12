# Cumulative development log

## Baselines and retained status

- `main` V0.4A, `2cfc39d`: World/FPS FOV validated by the user.
- V0.7A: last installed startup identified in local logs; freecam/native teleport
  success messages are runtime evidence, not comprehensive user validation.
- V0.8A: `351a57a` branch head; successful Actions run `43076274091` built
  `4221f8a`. Subsequent changes through initial V0.9 are docs/tools only.
- V0.9 initial research: `4c247e3`; inherits experimental features; no door fix.

## 2026-11-11 local research checkpoint

Objective: identify the actual chapter permission and determine whether Solari #1
is missing from persisted records, without resetting or editing progression.
Success criterion: evidence from the user's supported EXE, packages and backed-up
saves, with no guessed offsets and no save writes.

Outcome: permission logic identified in `IsCorrectDoor`; Solari ID `1` already
exists in the achievement save and the user confirms the chest is open.
Added a read-only actor/achievement inspection tool with six safety tests.
Local proprietary extraction and private save reports remain outside Git.

Unchanged: all `crtoi/` and `src/` gameplay sources, installed DLL/ASI/INI,
shortcuts, World/FPS FOV behavior, cinematics, zoom, saves and security settings.
No Darksiders repository was accessed or changed. No gameplay ZIP/build/release
was produced for this checkpoint; diagnostic work is not a reentry feature.

## TODO / next implementation gate

1. Fully decode `SetLockState`, hub-instance chapter/map values, `GetMapSoftObject`
   and transition callbacks, including side effects that could replay progression.
2. Establish a verified return route from each old chapter to the hub.
3. Implement a narrow opt-in override of the chapter permission on the experimental
   branch, with fail-closed executable checks; do not change completion flags.
4. Only then build and inspect a ZIP containing exactly `dxgi.dll` and
   `CRToiEnhanced.asi`; preserve the installed INI and rollback backup.
5. User test: enter and return from chapters 1/2/3, preserve bosses/puzzles/items,
   and verify FOV, native zoom and cinematics. Until then: EXPERIMENTAL.
6. Solari: compare the specific missing UI entry against the persisted IDs before
   considering any repair. No forced collection is warranted by current evidence.

Known CI gap: the current workflow excludes `dev/v0.9-hub-reentry-solari` from its
push branches and package-name switch. Add support with the first actual V0.9
gameplay build; do not label a docs/tools-only checkpoint as a new gameplay build.

## V0.8B CheatShop input / menu cleanup

Branch: `dev/v0.8b-cheatshop-input`, based on research checkpoint `0d1562d`,
whose gameplay code is identical to V0.8A. User feedback: everything else appears
to work; F1 CheatShop fails; remove the redundant suspend control. The user confirms
manual `cheatshop` works and console opens with ` / ~.

Objective: restore the one-key command path and remove only that redundant control.
Success criterion: F1 opens/closes SML's shop with the console initially closed;
FOV, zoom, cinematics, input and startup behavior retain prior behavior.

Change: window-local WM_KEYDOWN/WM_KEYUP and WM_CHAR replace global SendInput /
VK_PACKET text injection; default opening key is VK_OEM_3 (Tilde), with existing
explicit VK / launch F-key precedence. Log wording still requires SML validation.
Removed the suspend flag and its menu checkbox. Individual FOV controls remain.
Added experimental branch packaging to Windows CI. Runtime success is not yet
validated; never infer it from a successful PostMessage return or CI build.

### Concurrent progression UI report

The user supplied a screenshot showing Maremanto 1/1, crow relics 6/1,
holy blood 2/1 and vinyls 1/1, and says the Solari row disappeared.
A new private SaveGames snapshot was made and hash-checked.
Every saved file is byte-for-byte identical to the pre-V0.8A-install snapshot;
the achievement list still includes Solari ID 1. This does not establish that
runtime/UI state is correct, or that the older save was free of this issue.
Do not rewrite counters based on the screenshot. The map widget / total-source
requires separate investigation and is outside V0.8B's correction.

### V0.8B delivery verification

- **BUILT:** source `71dafa45352415dd4c06961800cd7f968872bf3d`, successful
  [Actions run 43100509478](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43100509478).
- **CONTROLLED:** artifact 13891831223, wrapper SHA-256
  `0663231d2132a239b0feb34d37b61f0057e96691595e8b3360e564532ee03e26`.
  Inner install ZIP SHA-256
  `dbb193456605c822d86f98992f08a707ff8c12a94b89032587b891e02f6e0ae0`.
  Exactly two root entries; both are x64 PE binaries; suspend label absent.
- ASI SHA-256 `8b7f7d0548a8ba75d0dc410edc6449c059107a342abbbba154445b85c0eed1af`.
- DXGI SHA-256 `5a34482505157ebc11bc6f4acd87084a8a4b150e89833029d406e258fad2bd92`.
- Defender custom scan: no threats detected. Protection settings unchanged.
- Installed over V0.8A after verified rollback copies. Installed file hashes match
  the staged build. Existing INI byte-for-byte unchanged. No save write performed.
- **EXPERIMENTAL:** F1 open/close, native zoom/cinematics/FOV regression and absence
  of the removed checkbox still need the user's game test. No stable promotion.

## V0.9B hub reentry TEST

Base: `6a7445d` V0.8B delivery checkpoint. V0.8A camera/startup features were
reported working; V0.8B's CheatShop correction remains experimental.
Branch: `dev/v0.9b-hub-reentry-test`; stable main is not changed.

Objective: revisit completed chapters without resetting progression. Success:
actual entry and return, retained boss/puzzle/items, and FOV/zoom/cinematics intact.

Implemented a separate, opt-in, full-hash-gated script hook for IsCorrectDoor and
SetLockState. It uses verified reflection to restrict changes to five existing
routes in the Fair and completed chapters 1/2; future access is refused, chapter 3
is native. Added 29 boundary cases and CI packaging for this branch. Full static
evidence, native ABI and gameplay checks are in `HUB_REENTRY_NATIVE_AUDIT.md`.
No game containers or save files are patched; native transitions keep their normal
autosave behavior. Runtime results remain unvalidated. The map display bug remains
separate; the user agrees this may be a display issue after returning to the hub.

### V0.9B build / installation checkpoint

- **BUILT:** `f6d97b90859881a765b59a78e77a3f895b3b798e`,
  [run 43105954754](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43105954754), success.
- **CONTROLLED:** 29 C++ route-policy cases passed in CTest; 6 read-only save-tool
  tests passed locally. No compile warnings were found in the retrieved job log.
- Artifact 13893458616; wrapper SHA-256
  `b376bb6149cd3b4678516425abc8719117c9f6c07996abd605d78217fa92a45c`.
- Install ZIP SHA-256 `b9170b50205340c8595a76f71d8c62df5c6a86e82a0af05170c0d5e319e37fab`.
  Exactly `dxgi.dll` + `CRToiEnhanced.asi` at root. x64 PE headers, expected exports
  and BCrypt import checked. Defender scan: no threats detected.
- ASI SHA-256 `44f2a9d8bb6811b72c72f8f2c85854b3acf3c78108ff8560537e41d4f270793f`.
- DXGI SHA-256 `52e2f60668fdd6f193b5960530e41d5f25a91382b3fa7ab8104840dbcb24d0cc`.
- Installed over V0.8B after verified DLL/ASI/INI and whole SaveGames copies.
  Installed hashes match the build; INI remains byte-identical (120/105 FOV,
  menu VK 36). All eight source save files still match the immediate backup.
- New option remains OFF by default. **EXPERIMENTAL, NOT USER-VALIDATED**:
  enable it, reload the Fair save, test chapter 1 and return before chapter 2.
  Hook readiness/counters and actual progression retention need game verification.
  No stable promotion or map-count/Solari repair is claimed.

## 2026-11-12 feedback / V0.9C

- **V0.9B access confirmed by user, persistence REJECTED:** enemies and pickup
  objects reappear. Successful access is not the requested safe reentry feature.
- **V0.8B/V0.9B keyboard CheatShop macros REJECTED:** manual `cheatshop` still works.
- Gameplay FOV appears to work; loading into a cinematic can leave the override
  stuck. The user's latest clarification is to keep the mod FOV in cinematics.
- Current saves copied/verified: both manual slots unchanged; autosave and
  cumulative achievement counters changed; collection lists unchanged. No restore.
- The installed INI's hub option was temporarily set to zero with an exact copy
  of the preceding INI. The next source branch removes the rejected door hook.

V0.9C objective: replace keyboard automation with verified native game-thread
dispatch and retain cinematic FOV without acquiring a cinematic gameplay baseline.
Success: actual SML open/close, moving/ending cinematics, FOV recovery after load,
native zoom intact, and no door/progression hooks. Static ABI evidence and limits
are recorded in `V09C_NATIVE_CONSOLE_CAMERA.md`. Validation remains experimental.
## V0.9C delivery checkpoint

- CONSTRUCTED: code `3547e08a1918a2067647c1cf8ed88036d2bd243f`, GitHub run [43205129608](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43205129608) succeeded.
- CONTROLLED: unsupported-host native-call CTest passed; six read-only save inspector tests passed locally. Package contains exactly dxgi.dll and CRToiEnhanced.asi at root; x64 headers/exports checked. Defender reported no threats.
- Artifact 13936424260. Install ZIP SHA-256: `8a0d54414358c49396fa1963267ff4672148498296f9783ce1e6d75299b13e4e`.
- Installed after verified binary/INI and eight-file SaveGames backup. Personal INI preserved byte-for-byte, including FOV 120/105 and HOME menu. No saves restored or edited.
- V0.9B door hook removed; prior saved world state is not repaired. Native CheatShop and cinematic FOV remain EXPERIMENTAL, awaiting the user's F1 open/close and load-to-cinematic-to-gameplay test. No stable promotion.

## V0.9D user correction

User explicitly requests restoration of working V0.9B gates; respawn was an observation, not a rejection of access. The earlier removal was an incorrect scope decision. Restore identical gate implementation, preserve V0.9C native CheatShop/cinematic FOV, personal settings and saves. Goal: original access plus retained camera/input changes. Criterion: chapter entrance/return works as before; respawn remains documented, not repaired. No additional reverse engineering in this iteration.

V0.9D delivery: code 1336719c26e4f763fd3ec31138753d7f417759cf; run [43207517711](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43207517711) succeeded, both CTests passed. Gate source/header/policy tests match V0.9B exactly. Artifact 13940190524; install ZIP SHA-256 d5343c3784acd3eb7576816742291476f014d2bc7655b2ae3dbf2afef2779028, exactly two root binaries. Defender: no threats. Installation attempted after verified binary/INI/save backup, but Windows reported the ASI in use. Installed ASI remains V0.9C, no INI/save changes; waiting for game closure before replacement. No claim of combined runtime validation.

V0.9D installation completed after user confirmed game closure. Both installed binaries match the checked artifact. Full binary/INI/save rollback copy verified. Only INI change: AllowCompletedChapterReentry 0 to 1; all other settings preserved, source saves unchanged. Runtime confirmation pending.

## V0.9E gameplay-only FOV

Latest user instruction supersedes cinematic-FOV request. Base V0.9D, original gates and CheatShop unchanged. No FOV writes to dedicated cinematics, non-player view targets, paused ticks, ignored move/look input, cursor-visible menus, unfocused game or mod menu. Controller fields resolved by bounded reflection; unknown schemas refuse override. Gameplay FOV eases toward configured values with 0.15s exponential time constant; resets on native zoom, camera change or interruption. Native menu/cinematic output is never blended by the mod. Criterion: native menu/cinematic FOV, smooth return after load, zoom intact, gates unchanged. Experimental until user confirms.

V0.9E CONSTRUCTED/CONTROLLED: code b2e407cc89201a70aef41852f62ffe4eb929af28, [run 43208506688](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43208506688) success, both CTests passed. Artifact 13939173173, install ZIP cf16135c0800032d6cfd4931f26e7227f92d51f85f98071ebaaac6e96885e842; exactly dxgi.dll and CRToiEnhanced.asi at root. Defender no threats. V0.9D remains installed pending installation authorization; no personal settings or saves changed. Reflected control flags and actual game/menu transitions still require runtime confirmation; missing schema suppresses override instead of guessing offsets. Native zoom/cinematic output is untouched immediately; easing applies when normal gameplay resumes, not by writing into menus.

V0.9E installed with explicit user authorization. Both installed binary hashes match the checked artifact. Binary/INI and full SaveGames rollback copies verified; personal INI byte-identical, source saves unchanged. Gameplay validation remains pending.

## V0.9F feedback correction

User validates CheatShop opening and remaining features, reports V0.9E FOV regression and unsafe shortcut closing. Logs show no V0.9E camera acquisitions. Remove nonexistent reflected IgnoreMoveInput/IgnoreLookInput fields; use ABI-verified read-only native virtual getters from exec wrappers 037E0EB1 (slot 830) and 037E0E81 (slot 848), signature/hash guarded. Replace strict inside-Tick camera restriction with recent matching-world tick observation; camera evaluation can occur outside tick nesting. Keep active-player, cinematic, cursor, input-ignore and focus guards. FOV = native current + eased(configured normal - captured normal), for independent world/weapon FOV; native ADS delta remains exact except finite projection clamping. No example values hardcoded. CheatShop is OPEN ONLY while no cursor is active; repeated calls are suppressed and closure uses its working native Close button. No guessed input-reset writes. Base V0.9D normal-camera behavior, V0.9E native menu guards/easing retained; gates unchanged. Goal/test: capture and adjustable FOV restored, aim delta retained, second F1 cannot hide shop while trapping input, Close button restores control. Experimental pending user test.

V0.9F CONSTRUCTED/CONTROLLED: code 22c2e626fe502c51feaecb81865a14976cbf75f0; [run 43211504255](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43211504255) success. All three CTests passed (unsupported host, existing gate policy, relative aim/easing). Artifact 13941262320. Install ZIP SHA-256 2094bcbf63415cec3e6685a10332f97293619df5b4ced45c3bc768658e95d2b3, exactly two root binaries; Defender no threats. Original V0.9B gate implementation unchanged. V0.9E still installed pending user authorization; no saves/settings changed. Actual FOV capture, menu guards, ADS delta and safe shop closure require gameplay validation. F1 close is explicitly unavailable; use the shop Close button, whose success was reported by the user.

V0.9F installed with explicit user authorization. Both installed binaries match the checked artifact. Binary/INI and complete SaveGames rollback copies verified; personal INI byte-identical and source saves unchanged. Runtime validation pending: FOV acquisition, relative aiming delta, smooth recovery, CheatShop open-only guard and native Close button.

## V0.9G narrow FOV rollback

User rejects V0.9F FOV: still absent since menu filter was added. Current log has no gameplay capture and repeated F2 no-camera messages. Remove BOTH pre-capture blockers introduced after the working V0.9D path: ObserveCinematic normal-camera gate and PlayerCameraControllable. Retain original first-person vtable/flags, original auto/manual acquisition, focus/mod-menu guard; dedicated cine cameras are never modified. Restore native projection bypass when not aiming, allow native aim delta while aim is held, preserve offset easing. No gate/CheatShop changes. Criterion: AUTO CAPTURED or manual capture followed by actual world/weapon FOV change in gameplay; then test aim and native menu/cinematic behavior. Broad generic menu/control detection remains unresolved; do not add another mandatory predicate without runtime evidence. Experimental until user validates.

V0.9G CONSTRUCTED/CONTROLLED: code 43c345ad9a0edc10d13da7979f1a28a42da06bd3; [run 43214894996](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43214894996) success, three CTests passed. Artifact 13943615765; install ZIP SHA-256 76e81bebfae3fa0b954bcdcf25b55e03c437f50bcfa8163cbc47c5654d4669f2, exactly two root binaries. Defender no threats. Not installed pending user authorization. No save/INI changes. Runtime success is NOT inferred from CI: first confirm gameplay camera capture and actual world/weapon FOV, then aiming/native menu transitions. Gates/CheatShop code unchanged.

V0.9G installed with explicit user authorization. Installed ASI/DXGI match the checked artifact. Complete binary/INI/SaveGames backup verified; personal INI byte-identical, source saves unchanged. Await user gameplay camera capture and actual FOV confirmation; gates/CheatShop retained.

## V0.9H independent aiming / native shop closure

- VALIDATED by user: V0.9G normal gameplay FOV works again. Preserve that acquisition path and original gates. V0.9G native-delta aim is abrupt; replace it at user request, not the normal camera path.
- User clarification: CheatShop's own Close button works; only F1 cannot close (the previous open-only limitation). New closure uses the same native Close button graph, not another console toggle.
- AimFOV / SniperFOV independently adjustable, default 80. Normal world and weapon settings preserved. Absolute exponential easing on aim entry/release; release native animation bypass grace avoids the former vanilla insertion. Dedicated cine projection still untouched.
- Read-only weapon identity: supported EXE execGetWeaponManager load at 04CC0395 = owner+B60; execGetActualWeapon at 04CE0415 = manager+D8; execGetIsShowScope at 04CE0695 = manager+13E. All byte signatures checked; actor/weapon reads bounded and destruction flags checked. Sniper class CRDivineSniper_BP_C confirmed in local item/weapon assets. Unknown identity uses generic aiming target, never blocks normal FOV.
- Shop asset: ShopCheater_WBP Construct calls ExecuteUbergraph at entry 178; close button calls the same graph at VM entry 10. Decoded graph fully parses (1292 VM bytes); entry 10 restores game-only input, cursor false, unpauses and RemoveFromParent. Alternate CloseCheaterButton_2 entry 677 resets upgrades and is NOT used.
- Native UObject::ProcessEvent body at 0147D520 forwards three arguments and builds native FFrame internally; hook observes Construct only and forwards all normal events unchanged. It obtains the graph pointer from the live Construct EX_LocalFinalFunction instruction, no global object search. Graph name/class, parameter size/property, script length and close-path opcode landmarks checked before invocation. Live viewport membership replicates verified execIsInViewport 03279335..0327935A via native getters. No fake FFrame, input-flag resets or save writes.
- Criterion: normal FOV unchanged; independent aiming and sniper slider effects with smooth entry/release; F1 open then close restores control like the button. CI/math/static evidence is not gameplay proof. Fallback: if widget/graph check fails, leave it open and use its working Close button.

V0.9H CONSTRUCTED/CONTROLLED: code 0abeee928e0fdc993e89e56268c02e39beb911ec; [run 43252791173](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43252791173) success. All three CTests passed (unsupported host incl. weapon/close guards, gate policy, independent ADS/sniper targets and smooth entry/exit). Artifact 13961360488; install ZIP SHA-256 ecfba975b109c7c36c7d987c3a12f809b9ec13ea5883d1e7c37e20e584d0d3b2; exactly dxgi.dll and CRToiEnhanced.asi at root. Defender no threats; no compiler warning/error lines in fetched log. Original gate implementation unchanged. V0.9G remains installed; no personal settings/save changes. Candidate awaits installation authorization and actual sniper/aim/F1-close gameplay validation.

V0.9H installed with explicit user authorization. Installed ASI/DXGI hashes match the checked artifact. Full binary/INI/SaveGames rollback copy verified; personal INI byte-identical and source saves unchanged. New absent aiming/sniper INI values default to 80 in code. Runtime validation pending for independent aiming/sniper transitions and F1 native closure; V0.9G normal-camera acquisition and original gates retained.
