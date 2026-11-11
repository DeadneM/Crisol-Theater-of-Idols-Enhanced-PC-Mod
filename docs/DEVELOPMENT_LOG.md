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
