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
