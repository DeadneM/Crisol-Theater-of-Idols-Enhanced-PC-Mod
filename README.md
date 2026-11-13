# Crisol: Theater of Idols | Enhanced PC Mod

Independent x64 ASI mod for **Crisol: Theater of Idols**. Built around an in-game Dear ImGui menu and a DXGI ASI loader.

## V0.10D bindings / overlay / native console

Save/load keys (defaults F5/F8), console, overlay, CheatShop, freecam and teleport are rebindable with conflict checks and INI persistence. The mod overlay no longer vetoes FOV/camera acquisition. Save/load requests explicitly originating in that overlay allow its UI state; backup, ownership, loading and file checks remain.

Console key opens an in-mod command window, with input/output and game-thread command execution through the already-verified native ConsoleCommand ABI used by CheatShop. Static audit proved base APlayerController::ConsoleKey is a parameter-destructor stub in this shipping executable; no ineffective native toggle/constructor experiment is delivered. Escape closes the command window. Actual command execution and overlay/rebind behavior require gameplay confirmation. Earlier FullAim sight test remains withdrawn.

## V0.10C F5 save / F8 load / console-key routing

F4 is free again; Alt+F4 remains passed to the game. F5 invokes native AutoSave, backs up the previous file and stores a byte-verified dedicated snapshot outside SaveGames. F8 backs up the current AutoSave, stages that last F5 snapshot and calls native CRLoadGame(world,0). Native automatic checkpoints do not overwrite the separate quick snapshot. Load/snapshot behavior still requires gameplay confirmation; no manual slot is targeted. Mod overlay closes and forwards the configured console key directly to the game.

The FullAim-only sight experiment is REJECTED after the user's no-effect report and no longer runs/is offered. True per-weapon sight alignment remains unresolved; do not label flag changes as iron sights. Existing personal INI remains untouched, including the obsolete experimental key, which is ignored.

## V0.10B save/input fixes and opt-in sight experiment

Base: V0.9I accepted behavior plus V0.10A native quick-save infrastructure. V0.10A F4 was refused before the native call; Alt+F4 was swallowed. V0.10B forwards Alt+F4 before all mod input handlers and removes the overbroad cinematic-mode save veto, keeping the working camera heartbeat, cursor checks, native save guards and verified backup.

Iron-sight experiment is OFF by default (`Camera/ExperimentalIronSights=0`). It requests the game's advanced aim transition on non-sniper weapons only, releases through the same native setter, and leaves the normal sniper untouched. This is not verified sight alignment: test actual front/rear sight alignment, animation recovery and absence of an unwanted scope overlay. Disable if incorrect. FOV/doors/working shop closure are retained.

## V0.10A F4 native quick save (experimental)

Base V0.9I confirmed by the user. New `dev/v0.10-quicksave-f4` branch adds F4 to the game's AutoSave path, with a byte-verified backup of the preceding AutoSave before invoking the native writer. Manual slots are not targeted. A short notification reports whether the AutoSave file changed; restore correctness still requires a reload test. Backup files are under the game's Saved/CRToiEnhancedBackups directory. Existing FOV, gates, shortcuts, personal INI and collapsed menu behavior are preserved. No F5/quick-load added.

## V0.9I personal defaults / collapsed menu

Uses the user's current defaults: world 120, weapon 105, aim 105, sniper 30, FOV enabled, HOME menu. Existing INI remains authoritative. All eight menu groups start collapsed, then remain user-expandable. F1 requests during the existing debounce wait rather than being discarded. Base V0.9H FOV and native shop closure retained; doors unchanged.

## V0.9H independent aim/sniper FOV and shop close

Base: user-confirmed V0.9G normal FOV recovery; original doors retained. AimFOV and SniperFOV default to 80 and are independently adjustable. Absolute eased targets replace relative native aiming deltas. CheatShop F1 closure attempts the verified native Close button graph on the actual visible widget, rather than relaunching cheatshop or writing input flags. EXPERIMENTAL: sniper detection, transitions and shortcut closure require user confirmation.

## V0.9G restore working camera acquisition

V0.9E/F control-state predicates failed in gameplay. Restore the V0.9D first-person camera acquisition path without the pre-capture controller/cinematic gates. Retain smooth offset, native aiming delta and original non-aim native-projection guard. Dedicated cine hook never writes projection. Gates and CheatShop unchanged. Generic menu/player-control detection is not claimed solved; user gameplay verification required.

## V0.9F FOV recovery / native aiming delta

Corrects V0.9E control-state checks; gameplay output preserves the native aiming difference on top of configured FOV. Menus/cinematics remain native, gameplay offset eases back. CheatShop shortcut opens only; close with its working native Close button. Original gates unchanged. Runtime confirmation pending.

## V0.9E gameplay-only FOV

Latest instruction: native menus/cinematics, mod FOV only with player control, eased gameplay recovery. Original V0.9B gates and V0.9C CheatShop retained. Experimental, requires gameplay confirmation.

## V0.9D restoration requested by user

Restores the exact V0.9B chapter-door mechanism while retaining V0.9C native CheatShop and cinematic FOV changes. Access was confirmed by the user; enemy/pickup respawn is an observed limitation, not a request to remove access. No save repair or restoration. Experimental pending combined gameplay confirmation.

## Versions

- **`dev/v0.9c-native-console-camera`: V0.9C experimental native CheatShop dispatch
  and player-camera cinematic FOV.** Removes the V0.9B door hook after the user's
  level-state reset report. No save repair or automatic restoration is performed.
  Keeps your FOV on active cinematic cameras and waits for normal gameplay
  projection before automatic acquisition. [Native audit / test plan](docs/V09C_NATIVE_CONSOLE_CAMERA.md).
  [V0.9C build / artifact](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43205129608/artifacts/13936424260): built, package-checked and installed; gameplay confirmation remains required.

- **`dev/v0.9b-hub-reentry-test`: V0.9B WITHDRAWN — level-state persistence failed.**
  Historical test information follows; do not treat successful access as a safe replay.
  Enable **Allow revisiting completed chapters** in the menu, then reload the Fair
  save. Completed chapter 1/2 entrances and their existing returns are targeted;
  chapter 3 retains native access. No progression-reset or Solari repair is included.
  Full EXE hash and reflection checks fail closed. First test one round trip before
  another chapter. See [native audit and test plan](docs/HUB_REENTRY_NATIVE_AUDIT.md).
  [V0.9B test build / artifact](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43105954754/artifacts/13893458616)
  is constructed and package-checked; gameplay validation remains pending.

- **`main`: v0.4A, gameplay-validated World FOV + independently adjustable FPS/weapon FOV.** This is the stable baseline. Manual **Capture gameplay camera** is required after loading gameplay.
- **`dev/v0.5-cheatshop`: experimental F1 `cheatshop` shortcut** for the separately installed SML and CheatMenu mod. This feature has not yet been confirmed in-game.

## Experimental V0.8A: auto camera capture + Skip Intro

### V0.8B input correction (experimental)

Based on the V0.8A gameplay code. The user reports that its other features appear
to work, while the F1 CheatShop macro fails. Manual `cheatshop` works via ` / ~.
V0.8B queues console-key and text messages to the game window instead of global
synthetic Unicode input. Default console key is now Tilde; an existing INI VK
override or F-key launch argument still takes precedence. Start with the console
closed. This correction needs a gameplay test; queuing text is not proof SML acted.
The redundant "Suspend ALL overrides" control was removed at the user's request.
Individual FOV toggles and Restore vanilla settings remain available.

FOV values, zoom/cinematic guards, auto capture, freecam/teleport, skip intro and
existing INI values are retained. Install ZIP still contains exactly the two
binaries. No progression, map-count or Solari repair is included.

- **Auto FOV camera acquisition:** after loading into gameplay, a stable first-person camera is selected automatically when ownership/proximity checks pass. The manual capture button remains a fallback. Existing native zoom, sniper and camera-swap safeguards are preserved. This detector is **not yet gameplay-validated**.
- **Skip startup splash videos (experimental):** attempts to suppress reads for only `BlumLogo.mp4`, `AnimacionLogoNew.mp4` and `SplashVideo1.mp4`. Physical files are never altered and story cinematics are not targeted. `[Startup] SkipIntroVideos=1` is on by default; switch it off in the in-game menu for subsequent startups. If the game cannot boot, create a text file `CRToiEnhanced.ini` next to the EXE with `[Startup]` on its own line followed by `SkipIntroVideos=0`.
- **Minimal installation ZIP:** exactly two files at archive root: `dxgi.dll` and `CRToiEnhanced.asi`. The mod's default settings are embedded, and the existing INI stays on your disk. When you change a setting, the mod creates/updates `CRToiEnhanced.ini` automatically.
- **Build transparency:** SHA-256 hashes appear in the GitHub Actions run summary, and the ZIP receives an attestation.
- **Gameplay caveat:** the prior V0.7 freecam/teleport experiment and F1 CheatShop are carried over unchanged and are not yet validated in-game.

[Download V0.8A test from GitHub Actions](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/runs/43076274091) (Artifacts section). The V0.4A World/FPS FOV release on `main` remains the validated rollback.

## V0.4A controls

- `Insert`: in-game ImGui menu, rebindable
- World FOV: **75–130**; FPS/weapon FOV: **60–130**
- Camera capture is intentionally manual; only the captured gameplay camera is modified, and native zoom/ADS/FOV changes take priority
- The old global camera-scalar patch was removed
- The mod does **not** enable the developer console

### Compatibility

Hooks target one specific executable build (SHA-256:
`b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378`).
Other versions fail closed. Cutscenes that reuse the exact same gameplay camera and native FOV may still require additional safeguards.

## Installation

Build with the included GitHub Actions workflow or CMake on Windows x64. For V0.8A and later development builds, extract only `dxgi.dll` and `CRToiEnhanced.asi` alongside `CRToiPrototype-Win64-Shipping.exe` in `CRToiPrototype/Binaries/Win64`. Back up an existing `dxgi.dll` before replacing it.

Avoid overwriting a customized INI when upgrading. Logs are written beside the executable: `CRToiLoader.log` and `CRToiEnhanced.log`.

## Building from source

The [Windows build workflow](.github/workflows/windows-build.yml) uses MSVC, CMake, **Dear ImGui v1.91.9** and **MinHook v1.3.4**. The ZIP artifact places installation files at its root and includes third-party license notices.

```pwsh
cmake -S crtoi -B crtoi/build -G "Visual Studio 18 2026" -A x64
cmake --build crtoi/build --config Release --parallel
```

## Security / antivirus notes

DXGI proxy DLLs, native hooks and synthetic input can trigger antivirus detections, but a detection must not automatically be considered a false positive. **Do not disable Defender or blindly whitelist files.** Inspect the public source, verify the GitHub Actions build provenance, check SHA-256 values, and submit suspicious binaries for review before deploying them.

This project does not distribute game binaries or files from SML and CheatMenu. It is an unofficial, community-developed mod and is not affiliated with the game's developer or publisher.

## Development references

### V0.9 local investigation, 2026-11-11

The supported game executable was verified locally. The actual hub teleport Blueprint
checks chapter-completion state in `IsCorrectDoor`; changing gate collision alone
would not address this permission. No reentry patch has been built or installed.
The inspected local achievement save already records Solari ID `1`; the user confirms
that chest is open. No collectible or reward has been granted or rewritten.
See [cumulative investigation](docs/HubReentrySolariResearch.md) and
[technical history / next steps](docs/DEVELOPMENT_LOG.md).

`tools/inspect_crisol_records.py` adds read-only candidate actor records and
achievement-list decoding. Its reports are private player data and must stay local.

The dark ImGui UI layout was adapted from [Darksiders Genesis Enhanced PC Mod](https://github.com/DeadneM/Darksiders-Genesis-Enhanced-PC-Mod); the Crisol project has its **own repository and build pipeline**. See [project notes](crtoi/README.txt).

