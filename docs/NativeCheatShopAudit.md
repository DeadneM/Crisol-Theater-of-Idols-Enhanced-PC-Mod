# V0.6 research: invoking `cheatshop` without a visible console

**Status: static analysis only, no new runtime hook or untested DLL.**
Working base: V0.4A (World / first-person FOV independently working after manual gameplay-camera capture).
Experiment base: V0.5A (F1, keyboard automation that might briefly open the console).

## SML dependency

The independently distributed [SML and CheatMenu mod](https://www.nexusmods.com/crisoltheaterofidols/mods/1?tab=docs) defines `cheatshop` as a toggle command. The project is **not** the Satisfactory SML project. Crisol's SML is an external Blueprint mod/modloader, and no documented stable native C API for calling `cheatshop` was found.

Do not copy or repack SML assets. Do not assume that the presence of the word `cheatshop` in a console proves a compatible direct-call API.

## Reference EXE and verified static observations

Reference file: `CRToiPrototype-Win64-Shipping.exe`
SHA-256: `b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378`

Read-only analysis of the existing reference EXE (no network transfer of the 168 MB binary) found:

- Narrow-string `ExecuteConsoleCommand` in `.rdata` at **file offset 0x07E25050**, virtual RVA **0x07E26650**.
- Two references to the string address in metadata tables. One table points to a candidate native wrapper around **RVA 0x039E14C0**; another entry points to an accessor around **RVA 0x03970540**. These are *candidates*, not verified callable API entry points.
- `cheatshop` and `CheatShop` do **not** occur in the unmodified game's EXE, as expected for a command supplied by SML's separate assets.
- Native `GetCameraView` is already used by the V0.4A camera-scoped FOV hook. Do not repurpose its pointers or add command execution inside that hot render path without a separate safety review.

A UFunction native wrapper may be a `FFrame` marshaller rather than a normal C++ static function. **Calling either candidate RVA with a guessed signature is forbidden** for the next experiment.

## Hypotheses to test, not assumptions

1. SML may register `cheatshop` with Unreal's console execution path, in which case `UKismetSystemLibrary::ExecuteConsoleCommand` or `APlayerController::ConsoleCommand` might dispatch it without opening the console.
2. Alternatively SML may intercept text through a Blueprint console-widget handler; in that case the engine's generic `Exec` path may not reach it at all.
3. A valid World/PlayerController and execution on the game thread are required for many Unreal console paths. A Windows `Present` thread is not a safe default.

## Proposed test ladder

- **T0, documentation only:** determine whether SML offers a reliable engine exec path and how its `cheatshop` handler registers.
- **T1, read-only:** identify reliable in-game World/PlayerController ownership on the game thread; log availability and stability, without writing anything or altering FOV.
- **T2, opt-in:** add a manually triggered, fail-closed native Exec test on the *experimental* branch. Confirm result with SML installed. No automatic fallback to synthetic input unless explicitly selected by user.
- **T3, usability:** F1 rebindable, no console window, correct toggle and safe debouncing, survives normal loads/transitions, no operation without SML.
- **T4, release gate:** comparison of FOV, sniper zoom, cutscenes, overlay and input against validated V0.4A; signed Windows CI; logs and user game test.

**Do not claim T2/T3 complete until observed in the actual game.**

## Antivirus

V0.5A's synthetic input helper and native hooks are reasonable candidates for heuristic alerts, but no particular detection can be attributed without the AV vendor/detection ID and exact file hashes. Preserve GitHub Actions SHA-256 and signed provenance; do not evade detection, disable Defender or assume a false positive.
