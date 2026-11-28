<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# QA: dual watch calls heard in panels (14a47736..5df78487)

Reviewed in worktrees pinned at 5df78487 (review, and a copy for mutations) and 14a47736 (base sizes and renders).
All three worktrees and their build trees are removed. No commits. No radio.

## Verdict: NOT APPROVED

There is one blocking item: the cost rule. Everything else matches the Nov 28 decisions. The build draws the approved
mock A pixel for pixel, no reference-theme frame changed, and the new tests fail without the change.

## Blocking

**B1. Builds without panels pay for a check that only panels needs (AGENTS.md: "a switch that's off costs nothing").**

`dual_activity` in `core/ui/ui_screens.c` now draws "Receiving on A and B" only where its width fits:

```c
} else if (d->pane[0].rx && d->pane[1].rx && lines == 1 &&
	   gfx_text_width(lf, TR(DUAL_STATE_RECEIVING_ON_BOTH, "Receiving on A and B")) <= w - inset) {
```

- **Without panels it is always true.** There, `lf` is always `L->small` and `inset` is 0. The widest case in the
  reference theme is 20 characters: 180 px in 9x18 on a 220 px panel, 100 px in 5x8 on 120 or 128 px, and 120 px in
  6x12 on 160 px. Every catalog fits as well (fr "Réception sur A et B" has 20 characters; the zh text is 4 CJK
  characters plus " A 和 B" in 8x16). The check can be false only in panels, where 8x16 meets the 148 px compact line.
- **What it costs:**

  | Build | 14a47736 | 5df78487 | Change |
  |---|---|---|---|
  | DM-1701 (no panels) | 710952 | 710968 | +16 B, all in `ui_home_draw` (0x1948 to 0x195c) |
  | MD-UV380 (panels off by default) | 712284 | 712300 | +16 B |
  | DM-32 with `CONFIG_UI_THEME_PANELS=n` | 785668 | 785704 | +36 B, all in `reference_home_dual` (0x6e6 to 0x70a). Not in the plan's flash notes |

- **Repro:** build DM-1701 at 14a47736 and at 5df78487, then compare `arm-none-eabi-nm -S --size-sort` output. Only
  `ui_home_draw` differs.
- **Fix, checked:** put the width term under `#if CONFIG_UI_THEME_PANELS`. With that change, DM-1701's
  `ui_home_draw` drops back to base size (0x1944; my build was "-dirty", so LTO shuffled other symbols by a few bytes
  either way). DM-32 with panels off goes from +36 B to +8 B. The 8 B left over is probably the new `lf` parameter's
  plumbing. Worth one look when fixing, not blocking on its own.

## For the user to decide (not approved or rejected by QA)

**U1. 160 px panels: "Receiving on A" with calls on both panes.** On 160x128-panels and 160x128iL2T1-panels, with a
call on each pane (`home_dual_two_rx`), the line under the panes reads "Receiving on A". It doesn't say B is
receiving, and it drops ", muted" (on one line only the first pane's line is drawn, with no meter). The reference
theme on 160x128 says "Receiving on A and B". Before this change the compact layout had no line there at all. The
proposal's 160x128 row says "Receiving: the state line only (no meter)" and doesn't cover two receivers. "Receiving on
A and B" is 160 px in 8x16, against 148 px of room. CLAUDE.md's text rule is "pick a shorter word, don't drop words".
B's pane still shows its RX pill and caller.

## Logged (not blocking)

- **L1. The list row's age is cut on 160 px.** When the caller's shortest form fills the row (a 16-character contact
  name), the age gives way completely. In Amateur at 158x127 the newest call's age then shows nowhere, because pane A
  has no room for it either. This follows the approved fit rule (the caller's shortest form keeps its room first), so
  it is logged, not blocking. Repro: a contact name of 16 characters calling on A; frames are in my scratch renders.
- **L2. The pane's age doesn't show everywhere.** On 220x310 (both profiles) and on the selected pane at 210x176, the
  pane has no age, before or after the change: row 1 has no room beside the name and PTT. The newest call's age is in
  its list row there. The decision kept both places; it didn't ask the panes to make room.
- **L3. No test checks the list row's age.** Mutation M7 (the age never drawn) survives every test. No test claims
  to check it, so it isn't overclaiming, but the decision names the age.
- **L4. The clearing rules have no host test** (shared `ui_dual.c`, not changed here). Mutations M8 (a pane's calls
  kept after its channel changes) and M9 (the list not cleared when the watch starts) survive uiThemeTestPanels,
  uiPrototypeTest and uiFlowTest. In the sim they work in every panels class: the A and B channel changes, the VFO key,
  and a stop with `#` `5` then a restart (shows "No calls heard yet").
- **L5. "The panes move up 2 to 6 px" is inaccurate.** At 310x240, pane A's text moves up 2 px and pane B's 10 px
  (each pane's padding 2 to 0 on both sides, plus the gap 6 to 2). That geometry is exactly what the decision names,
  and it matches the approved mock. Only gui-plan.md's "2 to 6 px" wording is off.
- **L6. Docs.** `docs/user/12-scan.md` says "two on most screens and the newest call only on a small screen", which
  leaves out Commercial's 3 on 220x310. "As many as fit" covers it. gui-plan.md says "RAM unchanged": MD-380 is
  -8 B (110920 to 110912), which is fine.
- **L7. Commercial's pane naming outside the test.** The talker alias is used only where Show talker alias has one,
  else the contact name or ID. Checked in the sim (alias "QD9ABC Chris…W, Boston MA" cut at its end). Amateur's pane
  still shows the contact or ID, not the alias, as before.

## What I checked, and the results

**1. Behavior.** I ran uiSim in all 5 panels classes, both profiles, `expect:layout=0` on every frame (0 failures):
idle, four calls, receiving on B with four calls, the talker alias on A, a long name, channel changes on A and on B,
the VFO, stop and restart, TX, two receivers, FM, large text, one call, and 6 minutes old.

- **Order and count:** newest first. Rows: 310x240 2 (state + 1 while receiving or talking), 220x310 Commercial 3
  and Amateur 2, 210x176 2, 160 px 1.
- **Caller:** the contact name ("Q1ABC Joe TG 91", short form "Q1ABC Joe") or the ID. The alias only in Commercial's
  pane.
- **Age:** the newest call's age is in the pane and its row wherever the pane has room (L2).
- **Channel changes:** a pane's calls go when its channel changes, and with the VFO key.
- **Restart:** the list is empty after a stop and restart.
- **Large text:** panes only, as before.
- **Geometry:** 210x176 and 220x310 Commercial are unchanged. 220x310 Amateur and the 160 px classes lose their
  padding, as decided.
- **Against the approved mock:** the design pass's own `render.sh` (5 classes x 2 profiles x 5 states, 50 frames)
  renders pixel-identical to `mock/A`.

**2. Frames.**

- **Against `../rf-base-step8`:** 2939 compared, 0 differ.
- **Against `../rf-base-step7`:** 2755 compared, 164 differ. The 164 are 4 panels classes x (20 dual watch Home + 6
  `__night`), 4 sheets, and 4 x 14 profile `home_dual*` renders. No frame differs in 128x64, 160x120i5, 160x128,
  220x176, 240x320 or 320x240 (the reference theme).
- **`uiSim --layout`:** 17056 screens, 0 problems.
- **uiSim:** builds with no warnings.

**3. Tests.**

- **At 5df78487:** uiThemeTest and uiThemeTestPanels (Homebrew LLVM, ASan) pass. uiPrototypeTest: OK (1 skip, as
  before).
- **With 533cf58f's code reverted and the tests kept:** uiThemeTestPanels has 16 failures (rows, offsets, Commercial's
  caller in each display), and `test_dual_watch_lists_the_calls_heard` fails in every class and profile.
- **Mutations:**

  | # | Mutation | Caught by |
  |---|---|---|
  | M1 | Compact `list_rows` 1 to 0 | uiThemeTestPanels, uiPrototypeTest |
  | M2 | Amateur full layout `list_rows` 2 to 1 | uiThemeTestPanels, uiPrototypeTest |
  | M3 | The room tries' gap 2 to 3 | uiThemeTestPanels (310x240 name offsets), uiPrototypeTest |
  | M4 | Commercial never uses the alias | uiThemeTestPanels |
  | M5 | Alias used even when empty ("Receiving") | uiPrototypeTest |
  | M6 | The list drawn oldest first | uiThemeTestPanels, uiPrototypeTest |
  | M7 | The list row's age never drawn | **survives** (L3) |
  | M8 | A pane's calls kept on channel change (`ui_dual.c`) | **survives** (L4) |
  | M9 | The list not cleared at watch start (`ui_dual.c`) | **survives** (L4) |
  | M10 | The width check removed | `uiSim --layout` (16 problems, 160 px `home_dual_two_rx`) |
  | M11 | `dual_make_room` always returns the small font | uiThemeTestPanels, uiPrototypeTest |

**4. Cost rule:** see B1.

- **Built:** DM-1701, MD-UV380 and DM-32 with `CONFIG_UI_THEME_PANELS=n`, at both commits, all with 0 warnings.
- **GD-88:** +688 B (798552 to 799240).
- **MD-380:** +632 B (618660 to 619292).

**5.** See U1.

**6. Stack.**

- **GD-88:** UI 1664 deepest, 204 free, the same as at 14a47736. Least free overall: start, 68 B.
- **MD-380:** UI 1532 deepest, 324 free, the same as at 14a47736. Least free overall: start, 16 B.
- **RAM:** GD-88 unchanged, MD-380 -8 B.

**Not run:** the full `ci.sh` and `ci-par.sh`, the DB25-D build, and the maximal builds.
