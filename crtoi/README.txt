Crisol Enhanced PC Mod V0.7A: FREECAM + TELEPORT TEST (Windows x64)

CONTROLS
- INSERT: the existing in-game Dear ImGui mod menu
- F1: experimental SML CheatShop command (requires SML separately)
- F2: toggle freecam. Move through scenery.
- F3: teleport the player to the freecam position and restore normal camera.
- During freecam: WASD / ZQSD move, Space rises, Ctrl descends, Shift accelerates.
- F2 again cancels flight WITHOUT teleporting. Both F2/F3 can be rebound.
- Set flight speed in the mod menu or the [Freecam] section in the INI.

VALIDATED FEATURES PRESERVED
- World FOV 75..130 and independent FPS FOV 60..130.
- Gameplay camera must first be manually captured in the overlay.
- Scope/zoom/native FOV changes and camera swaps retain priority.
- dxgi.dll loader, overlay and F1 CheatShop code unchanged.

HOW TO TEST
1. Use a backup save, stand in a SAFE area and capture gameplay camera.
2. F2 turns on flight. Try WASD, Space and Ctrl.
3. Fly to an OPEN, unoccupied area (not inside a wall or hazard).
4. Press F3: if native AActor::TeleportTo accepts destination, it moves the
   character and disables freecam. If collision rejects destination, nothing
   should happen to the player's world position.
5. F2 cancels any flight. Test menu / FOV / sniper / cinemagraphic scenes.

TELEPORT METHOD
On the verified Crisol EXE, the native UCameraComponent::GetCameraView hook
locates the component owner, validates the actor/root location, and invokes
the native collision-tested AActor::TeleportTo vtable slot. The saved EXE is
not modified. The input callback only QUEUES F3; actor teleport is done in
the native gameplay camera callback, not the Windows window procedure.

SAFETY LIMITATIONS
This is an EXPERIMENTAL build: native owner and collision checks have been
derived from static executable analysis, not yet validated during gameplay.
The game may use a separate camera actor, in which case flight will safely
refuse to start. Standard game input while freecam is enabled might still
move the pawn through raw-input paths; flight auto-cancels if movement is
detected. Repeated teleports, scripts, cinematics and loading screens remain
untested. Avoid high drops and obstacles. The native TeleportTo call returns
false on some obstructed positions; do NOT force teleport with collision
checks disabled. The V0.4A baseline remains available on main.

GAME BUILD REQUIREMENT
SHA-256 of analyzed shipping EXE:
b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378
Other EXEs must fail closed.

INSTALL
Extract dxgi.dll, CRToiEnhanced.asi and CRToiEnhanced.ini next to:
CRToiPrototype/Binaries/Win64/CRToiPrototype-Win64-Shipping.exe
Backup previous dxgi.dll; preserve customized CRToiEnhanced.ini.
Build output includes BUILD_INFO.txt, SHA256SUMS.txt and licenses.

BUG REPORT
Send CRToiLoader.log and CRToiEnhanced.log from the same game folder.
Report whether F2 activated, camera actually moved, and whether F3 restored
character to freecam destination. Do not disable security software.
