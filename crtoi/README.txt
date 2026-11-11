CRToi Enhanced V0.4A - CAMERA-SCOPED FOV TEST (Windows x64)

CHANGE OF TECHNIQUE
V0.3A patched a global PI/360 constant shared by camera math across the engine.
This was unsafe for sniper scope, scripted puzzles and cinematics. REMOVED.
V0.4A detours UCameraComponent::GetCameraView, verified in the exact user EXE.
No global constants, no on-disk EXE changes, no developer console.

FEATURES
- Dear ImGui DX11/DX12 overlay, Insert, rebindable toggle.
- World FOV slider: 75 to 130 (independent of the native first-person view FOV).
- FPS / Weapon FOV slider: 60 to 130 (first-person primitives only).
- Both off by default, original FOV always wins while scope/ADS is held or
  when native world or first-person FOV differs from captured gameplay baseline.
- Controller left trigger / RMB also suspend changes while held.
- Cinematic camera switches are untouched (captured object identity check).
- Restore vanilla and Suspend controls; no persistent overwrite of camera fields.

IMPORTANT SAFETY LIMITS
Do not claim fully cinematic-proof until tested in game. Some scripted scenes
could reuse the SAME captured camera and SAME baseline FOV; these cannot yet
be identified without further game-specific cinematic hooks.
If a first-person camera is not using Unreal's native FirstPerson rendering
flags, this build deliberately refuses to modify it. In that case the FPS FOV
will have no effect; use the diagnostic counters to confirm and report.
World/FPS FOV changes apply to this exact EXE only. Other versions fail-closed.

INSTALL
Extract the ZIP's files beside CRToiPrototype-Win64-Shipping.exe
(...\\CRToiPrototype\\Binaries\\Win64\\). Back up any prior dxgi.dll.

TEST IN THIS ORDER
1. Start game, ensure fullscreen / overlay work with Insert.
2. Load a save and stand in ordinary gameplay with no aiming/cutscene.
3. Open overlay > 'Capture gameplay camera'. It should display captured
   native World/FPS FOV. If not, send logs, do NOT force hooks.
4. Enable World FOV, test 75/90/110, verify viewport updates in gameplay.
5. Enable FPS FOV separately, test 65/85/105, observe weapons/hands only.
6. Aim through a sniper scope with RMB or controller LT: native zoom should
   take priority, as should cinematic camera switches.
7. Re-test gameplay on returning from a cinematic, optionally recapture after
   level transitions. Test 'Suspend ALL overrides' and 'Restore vanilla'.

DIAGNOSTICS
CRToiEnhanced.log and CRToiLoader.log beside the EXE.
The 'About / Diagnostics' panel shows native GetCameraView calls, eligible
camera count, scope bypass count and actual World/FPS edits.
If eligible=0, the game does not appear to use the expected native first-person
camera path for this test, and NO modification should be applied.

EXE FINGERPRINT
b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378
