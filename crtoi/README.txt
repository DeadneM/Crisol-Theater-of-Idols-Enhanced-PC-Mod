CRISOL ENHANCED PC MOD V0.8A  |  AUTOCAPTURE FOV + SKIP INTRO TEST

INSTALL (2 FILES ONLY)
Extract dxgi.dll and CRToiEnhanced.asi beside
CRToiPrototype-Win64-Shipping.exe in CRToiPrototype/Binaries/Win64/.
Back up any existing dxgi.dll. Do not add or overwrite CRToiEnhanced.ini.
The mod uses built-in defaults and writes CRToiEnhanced.ini alongside the
EXE automatically when a setting is changed from the menu.

NEW: AUTOMATIC CAMERA CAPTURE
- After gameplay begins, the ASI observes the native first-person camera.
- After roughly 1.5 seconds of stable first-person camera data (plus gameplay
  owner-proximity checks), it captures World/FPS baseline automatically.
- No need to press Insert or manually click Capture gameplay camera first.
- When gameplay changes camera for a while, a new stable player camera can
  be captured automatically after a 2.5-second grace period.
- The old manual Recapture button remains as fallback if automatic detection
  is too strict for a particular gameplay state.
- AutoCaptureGameplay=1 by default; can be toggled from the mod menu.
- Zoom, scope, native FOV transitions and other cameras remain protected.
- This is a heuristic detector, not yet tested against every cutscene.

NEW: SKIP STARTUP VIDEOS
- SkipIntroVideos=1 by default; can be toggled in Startup / Skip Intro.
- Only affects Blu(m)house/developer/startup splashes:
  BlumLogo.mp4, AnimacionLogoNew.mp4, SplashVideo1.mp4.
- Uses narrow Win32 file-opening hooks (CreateFileW and CreateFile2) to
  make these reads appear missing. Does NOT touch or rename physical files.
- Does not target later gameplay story cinematics or saved games.
- Because the DXGI loader is initialized only once rendering begins, the
  movie player may have opened a video before the hooks were installed.
  In that case skip may not work: inspect 'blocked requests' in diagnostics.
- Toggle Skip Intro then RESTART to apply to startup sequence.
- This is an experimental implementation, NOT yet validated during startup.

PRESERVED
- World FOV 75..130 and independent First-Person/Weapon FOV 60..130.
- Original FOVs take priority during sniper ADS, zoom, scene transitions.
- Insert: ImGui menu. F1: experimental CheatShop with external SML.
- F2: experimental freecam; F3: native-teleport test to camera position.
  Both features remain unverified in actual gameplay.
- For freecam the captured camera is now detected automatically, but the
  F2/F3 mechanism itself is still experimental.

DIAGNOSTICS / ROLLBACK
Logs: CRToiLoader.log and CRToiEnhanced.log next to the game EXE.
For issues, try the validated V0.4A artifact in main as baseline.
This ZIP intentionally ships only dxgi.dll and CRToiEnhanced.asi.
SHA-256 checksums and build commit are recorded in the GitHub Actions
run summary, and the ZIP has a GitHub-signed provenance attestation.

COMPATIBILITY
Analyzed game EXE SHA256:
b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378
Only the corresponding game build is supported.
This is an unofficial mod and is not guaranteed safe or malware-free.
Do not disable antivirus protection to install it.
