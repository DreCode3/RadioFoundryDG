<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Step 7 QA re-review: the fixes (0750b546..9fc4314a)

Reviewed in a worktree pinned at 9fc4314a (now removed).

## Verdict: APPROVED

Every fix fixes its defect. Each fails its tests when I revert it. Nothing nearby broke.

## The fixes

**B1, compact operational Home** (edee0597, 9fc4314a): fixed.

- At 160x128 and 158x127, the trunked Home, the SFR Home, and a trunked call that is waiting or admitted now draw with
  no overlap.
- They show no hint line, and the bar reads "Menu".
- The zone and system names are cut at the end.
- `--layout` now checks those screens through four new `audit_extra` entries.
- Reverting the fix's checks: the operational branch, the call screen and the zone end cut are each caught, by
  uiThemeTestPanels and/or `--layout`.

**B2, 158 x 127** (8a024c6b): fixed.

- The new `160x128iL2T1-panels` class matches the MD-380's insets. `--layout` there: 0 problems.
- "Profile" now reads "Profi…", cut at the end, and the focused row slides it.
- "Sats 9 used, 14 seen" fits.
- Reverting each part is caught: the alternates' end cut (uiThemeTestPanels and `--layout`), the slide of alternates
  (uiThemeTestPanels), the GPS spacing (`--layout`).

**The fit rule from 220 x 200** (34bf700c): correct.

- Removing the width condition in `panels_fits` is caught by uiThemeTestPanelsOriginal.
- Removing it in `panels_small` is caught by uiThemeTestPanels.
- DM-32 (220 wide) keeps the full layout, and its frames match the previous step.

**The 220x176 class, now `220x176iR10-panels`** (a0b4c059): 210 x 176 with Home keys. `--layout` gives 0 problems.

**Narrow Settings tiles, icon above the label** (90faf70e): reverting is caught by uiThemeTestPanels and
uiThemeTestPanelsOriginal.

**`FONT_AA_THEME_ONLY`** (af02fcd9): correct.

- **Panels off:** MD-380 and DB25-D have no anti-aliased font and no panels symbol. MD-380 is 584,516 B, against
  590,164 B before the fix. DB25-D is 734,260 B.
- **Panels on:** both font sets are linked, and the original look draws the bitmap ones.
- **Combinations:**
  - FONT_AA=n with panels on builds;
  - panels=n with FONT_AA=y builds and keeps anti-aliased text everywhere (no `FONT_AA_THEME_ONLY`).
- All 0 warnings.
- **Stack check:**

  | Build | UI stack free |
  |---|---|
  | MD-380, panels on | 324 B (548 words) |
  | MD-380, panels off | 196 B |
  | DB25-D, panels on | 204 B |
  | DB25-D, panels off | 204 B |
  | MD-UV380, panels=y | 224 B |
  | DM-32 | 272 B |
  | GD-88 | 204 B (as frozen) |

- Forcing the original look onto the anti-aliased fonts is caught by uiThemeTestPanelsOriginal.

**`ui_list_bar` for a measured list** (b9938edf): reverting is caught by uiThemeTestPanels and
uiThemeTestPanelsOriginal.

**The four missed mutations** (44f4beb3): m08, m10, m15 and m16 are all caught now, by uiThemeTestPanels.

**Two extra mutations on `panels_home_operational`**, both caught:

- skip restoring the regions;
- always reserve the hint row.

## Nearby

- **Frames:** `cc.sh ../rf-base-freeze`: all 940 profile renders 0 differ.
  - gallery/320x240 has 1 differ, `satellite.png`. That is the reference satellite hint from be8c30db, which is before
    this range and accepted (dfad6424).
  - The other differences are expected:
    - DM-32 (panels by default);
    - the Theme row in Display;
    - the hint short forms from be8c30db (`dmr_id`, `satellite`);
    - the crash_log line-break fix.
  - cc.sh prints "CC FAILED" only because of those expected differences.
- **GD-88:** 0 warnings, flash 799,272 B, RAM 205,848 B, UI stack 204 B free.
- **Host tests:** uiThemeTest, uiThemeTestPanels, uiThemeTestPanelsOriginal and gfxFbEdgeTest pass under ASan in
  local/rf-host-tests. uiFlowTest and uiPrototypeTest pass on the Mac.
- **Layout check:** `--layout` gives 0 problems on 160x128-panels, 160x128iL2T1-panels, 220x176iR10-panels,
  240x320i10x5 and 320x240.

## Logged, not blocking

1. **"Q1AB…Joe" is cut in the middle at 158 x 127.** After a call, the compact trunked Home's last-heard line reads
   "Q1AB…Joe  TG 3100 1s".
   - Cause: shared `draw_last_heard` shortens the caller's name in the middle in its one-line form, and the fix's end
     cut doesn't reach it.
   - At 160 px it fits whole.
   - Repro: `uiSim --geom 160x128iL2T1-panels --script "t3 dlpos:3 wait:3000 call:9991234,3100,g wait:500"`.
   - `--layout` misses it because no audit script makes a call on a trunked channel.
2. **MD-380 panels may no longer need the larger stack.** Its UI stack now has 324 B free with 548 words. The 516-word
   default may now be enough, which would save 128 B of RAM.

## Not run

- the full host suite;
- the full ci.sh matrix and the maximal builds;
- GD-88 switch-off builds (`af02fcd9` touches only the MD-380 and DB25-D config);
- random walks;
- HIL, the radio, on air.

Nothing was committed.
