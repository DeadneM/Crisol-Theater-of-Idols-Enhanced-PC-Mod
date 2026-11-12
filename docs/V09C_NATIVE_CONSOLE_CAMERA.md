# V0.9C: native console and cinematic FOV test

**EXPERIMENTAL.** Source base: retained V0.8A camera/freecam/startup behavior,
V0.8B menu cleanup, and the V0.9B investigation. Both keyboard macros have failed
the user's game test. The V0.9B door access worked but level-state retention failed;
that hook is removed from this build, rather than passed forward as a safe feature.

## Save investigation, 2026-11-12

The current SaveGames directory was copied and hash-verified before analysis.
Relative to the immediate pre-V0.9B installation copy:

- CRGame01 and CRGame02 are byte-identical: neither manual slot was overwritten.
- AutoSave differs and records the hub after the return trip. Several serialized
  chest/teleporter payloads also differ from the unchanged manual hub slot.
  Individual payload bits are not assigned guessed meanings.
- AchievementData differs in cumulative coin/enemy counters; collection lists,
  including the recorded Solari identifiers, are unchanged.
- Keyboard/config-related saves and the Cloud metadata file are unchanged.

No save was restored, merged or edited. A separate current snapshot and the older
backups are retained locally. The older pre-test autosave predates the manual hub
save, so comparing only the two autosaves is not a complete causal test.

Finding: successful native travel and retained chapter-completion booleans did NOT
establish retained world/actor state. The reported enemies/items respawn is a real
failed acceptance criterion. Do not simply re-enable the former permission hook.
Future reentry work must establish capture/load of destination actor state; no old
state may be invented if it is absent from the available saves.

## Native CheatShop command

F1/menu requests now queue the literal `cheatshop` command. There is no SendInput,
PostMessage keyboard sequence or automatic fallback. A verified world-tick callback
processes the request on the game thread, including while normal gameplay is paused.
The request expires after ten seconds or is cancelled if the game loses focus.
Only an explicit user request can cause a dispatch.

The full supported EXE SHA-256 is checked before enabling this bridge:
`b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378`.
Native code bytes are checked independently. Wrong executables refuse all native
calls; CTest exercises that refusal with invalid object/window pointers.

Verified ABI in this exact executable (all offsets hexadecimal):

| Evidence | Implementation boundary |
|---|---|
| UWorld tick at `03A6CFF0`, anchored by `TickInGamePerfTrackersRT`; native body stores RCX, EDX and XMM2 as world/type/delta | Hook forwards the original three arguments unchanged and services requests afterward |
| Startup writes OS GetCurrentThreadId to `096DD2E8` and initializes `096DD2F4` at `040861BC` onward | Calls additionally require the current OS thread to equal the initialized engine game-thread ID |
| execExecuteConsoleCommand at `039E14C0`, dispatch at `039E162E` | Controller vtable +DE0: RCX controller, RDX 16-byte FString result buffer, R8 const FString command, R9b write-to-log |
| Wrapper resolves world through `03F2BC10`, GEngine pointer `09944860`; first controller getter `03FA9D70` | World/controller objects must be live and have the correct reflected base class |
| Wrapper frees ConsoleCommand result storage through `0112ABC0` | Same one-pointer allocator free; no arbitrary memory writer or FFrame thunk invocation |
| execGetViewTarget at `037E05F0`, call at `037E0611` | Controller vtable +7A8 is a zero-argument actor-pointer getter |
| FName pool/reflection layouts already exercised by V0.9B's actual door matches | Read-only, bounded field lookup verifies name/type/dimensions/size and boolean masks |

The exec wrapper itself is NOT called with a guessed C++ signature. Its verified
virtual ConsoleCommand call is used. A returned call or empty response does not
prove the SML shop opened; the in-game response is still a test requirement.

## Cinematic FOV and acquisition

The user's latest preference is to KEEP the mod FOV during cinematics.
`KeepFOVInPlayerCinematics=1` defaults on; the menu label is **Keep mod FOV during
cinematics**. Native aim/scope input still takes priority.

- Standard camera handling checks the controller's reflected bCinematicMode.
- Dedicated cine components are also supported when the owner is the active
  local player's view target. Background/unrelated cameras are excluded.
- Cine GetCameraView `032BF020` forwards RCX/XMM1/R8 to base GetCameraView
  `0363A3C0`, then evaluates lens settings. Its vtable is `07B15C30` (+620 method,
  +600 `032BFD20`, +608 `032BFE70`). The second hook applies FOV AFTER that native
  lens work; the original camera motion, timing and focus work still run.
- The active view target and world/controller queries use the verified getters
  above on the verified game thread. No control-lock flags are changed.
- Automatic gameplay acquisition accepts only the supported normal 90/80
  projection. A cinematic projection cannot become a stable gameplay baseline.
  On cinematic exit, automatic acquisition resets and recovers; manual capture
  remains available for non-default native settings.

This addresses a plausible cause of the reported stuck override: the old detector
could accept a stable cinematic FOV, then indefinitely reject normal gameplay as
a deviation from that baseline. The exact user symptom and recovery still require
gameplay confirmation; the build is not described as validated.

## Retained / removed / test

Retained: user INI/FOV values, menu/freecam/teleport shortcuts, native collision
teleport, zoom guard, skip intro and two-file ZIP. Removed: rejected door hook and
both ineffective keyboard macros. No save restoration or progression repair.

Test F1 open/close with SML, a loading transition into a cinematic, mod FOV during
that cinematic, automatic return to gameplay FOV, and native aim/zoom. Confirm the
cinematic moves/ends normally. Review command response and camera logs if a check
fails. Do not re-enable V0.9B reentry as part of these tests.

Primary reference for locating the world tick:
[patternsleuth UWorldTick resolver](https://github.com/trumank/patternsleuth/blob/master/patternsleuth/src/resolvers/unreal/game_loop.rs).
