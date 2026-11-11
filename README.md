# Crisol: Theater of Idols | Enhanced PC Mod

Independent x64 ASI mod for **Crisol: Theater of Idols**. Built around an in-game Dear ImGui menu and a DXGI ASI loader.

## Versions

- **`main`: v0.4A, gameplay-validated World FOV + independently adjustable FPS/weapon FOV.** This is the stable baseline. Manual **Capture gameplay camera** is required after loading gameplay.
- **`dev/v0.5-cheatshop`: experimental F1 `cheatshop` shortcut** for the separately installed SML and CheatMenu mod. This feature has not yet been confirmed in-game.

## Experimental V0.8A: auto camera capture + Skip Intro

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

The dark ImGui UI layout was adapted from [Darksiders Genesis Enhanced PC Mod](https://github.com/DeadneM/Darksiders-Genesis-Enhanced-PC-Mod); the Crisol project has its **own repository and build pipeline**. See [project notes](crtoi/README.txt).
