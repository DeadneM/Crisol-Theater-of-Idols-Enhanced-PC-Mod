CRToi Enhanced V0.5A - F1 CHEATSHOP SHORTCUT TEST (Windows x64)

VALIDATED BASE: V0.4A World and independent FPS FOV
- World FOV: 75..130, FPS/Weapon FOV: 60..130.
- Both remain independently adjustable.
- Manual "Capture gameplay camera" remains REQUIRED. Not modified.
- Keep original camera protection against zoom, sniper scope and swaps.
- No old global engine constant patch. No developer console menu.

NEW FEATURE
- Press F1 in normal gameplay to issue the external SML mod's
  "cheatshop" console command automatically.
- Press F1 again to toggle that same shop off if supported by SML.
- Configurable CheatShop shortcut in ImGui > CheatShop (SML external mod).
- "Open / close CheatShop" button in that section.
- Debounced one action per keypress (no repeat if F1 is held).
- No changes or redistribution of SML and CheatMenu.

EXTERNAL REQUIREMENT
- The "SML and CheatMenu" mod (Nexusmods /crisoltheaterofidols/mods/1)
  must be installed independently and working.
- This is a keyboard macro: the game console opens briefly automatically,
  "cheatshop" is typed, then Enter executes it. It avoids manual console
  interaction, but does NOT yet provide a truly invisible native Exec bridge.
- The console may flash briefly. The ASI cannot verify SML's execution.
- Default console opening key = physical key beneath Escape (scan 41).
- If SML uses Steam launch option "-consolekey=F10" (F1..F12),
  this shortcut autodetects that option.
- Otherwise, configure SMLConsoleVirtualKey in CRToiEnhanced.ini:
  e.g. F10 = 121, F5 = 116. Set 0 for autodetection/physical default.
- These settings are independent of CheatShopVirtualKey, which defaults
  to F1 (112).

INSTALL
Extract dxgi.dll + CRToiEnhanced.asi + associated files directly into
CRToiPrototype/Binaries/Win64 beside CRToiPrototype-Win64-Shipping.exe.
Back up any existing DXGI proxy DLL.

IMPORTANT UPGRADE NOTE
Keep your current CRToiEnhanced.ini if you have personalized FOV values.
V0.5A reads the new CheatShop key with F1 as the built-in default,
even if the entry does not exist in your old INI.

TEST
1. Launch Crisol with SML and CheatMenu installed; confirm manual
   console command "cheatshop" works once before testing automation.
2. Load gameplay, with your V0.4A camera capture already performed.
3. Press F1. Expect automatic console-command sequence and shop toggle.
4. Press F1 again to close the shop (SML toggles cheatshop).
5. Check both FOVs were preserved and scoped zoom/cinematics remain native.
6. Rebind F1 from ImGui; ensure new key functions.
7. Check CRToiEnhanced.log: "sequence sent" only confirms the
   keyboard sequence, NOT that the shop actually opened.

IF THE SHOP DOES NOT OPEN
- If the console does not open, check -consolekey Steam option and
  SMLConsoleVirtualKey (e.g. 121 for F10).
- If the console opens but text doesn't type, send both logs.
- Never assume the other mod is installed just because the shortcut exists.

EXE build identity from user's analyzed file:
b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378
