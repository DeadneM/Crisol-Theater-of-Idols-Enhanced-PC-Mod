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
