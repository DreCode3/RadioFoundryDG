<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Step 7 QA review: panels theme port (226ad2d4..6acb11a3)

Reviewed in a worktree pinned at 6acb11a3 (now removed).

## 1. Verdict: NOT APPROVED

The GD-88 is untouched: every 320x240 frame and all 940 profile renders show 0 differ. The builds, the stack and the
switch-off builds are clean. The two blocking defects are both in the compact layout on the 160x128 class, in screens
the gallery and the layout check never reach.

## 2. Blocking findings

### B1. The compact layout's trunked and SFR Home draws text over other text (MD-380, DB25-D)

- **Where:** `core/ui/themes/panels/panels.c:1639-1643` (`panels_home`) sends an operational channel to
  `ui_reference_home` (`core/ui/ui_screens.c:2625`). Operational means a trunked channel (`t3.sys`), a DMR/P25 trunk
  kind, a kind the build lacks, or a DMR SFR repeater.
- **Cause:** that body lays out with `ui_layout_regions` (`ui_screens.c:158`). With the compact metrics (strip 18,
  every font 8x16 except a 24 px large font, `hint_y` 91, or 90 at 127 px high), `activity_h` comes out as 1 px. Three
  lines then land on the same rows:
  - "No calls heard yet" is drawn over the chips line ("County Ready 1W");
  - the hint row "OK Menu  * Quick" is drawn on top of both.
- **Decision not built:** gui-plan.md says Home on this class shows no hint line. This Home still shows one.
- **Why it blocks:** wrong drawing on a radio that ships. DMR_TRUNK is on by default on both radios, and panels is
  selectable on both. 220x176 and 210x176 are fine.
- **Reproduction:**
  - `tests/host/uiSim --geom 160x128-panels --script "t3 dlpos:3 wait:3000" --out t3.png`
  - The same overlap at 158x127.
  - `--layout` doesn't see it: the gallery's only trunked script (`incoming-call`) ends on the ring screen, and the
    layout check only looks at the last frame.
  - Renders: `rf-qa11-out/sim/t3x.png`.

### B2. At the MD-380's real 158 x 127, English text is cut in the middle or runs off the screen

MD-380 panels is selectable, and its insets leave 158x127. The simulator has no class for that size: 160x128-panels has
no insets. I added a temporary class (`{ "158x127-panels", 160, 129, 4, 1, 1, true, false, false, 0, false, true }`)
and ran `uiSim --layout 158x127-panels`. It reports 20 problems; 160x128-panels reports 0.

- **Settings > Display > "Profile" reads "Pro…le"**, a middle cut, focused or not. Its value is "Commercial".
  - The label has alternates, so `ui_row_text` sends it to `gfx_text_fit`'s middle cut even though the compact layout
    asks for an end cut. That happens at `core/ui/ui_screens.c:3672-3675`, called from `panels.c:2024`.
  - The slide doesn't rescue it: `panels_row_slide` returns early for any label with alternates.
  - The same cut shows in every flow that passes Display (`settings_display`, `settings_editor`, `home_large_text`,
    the three dual large-text flows).
- **The trunked Home's zone line reads "Local r…eaters"**, a middle cut (the `incoming-call` flow; B1's screen).
- **GPS Status "Sats 9 used, 14 seen"** is 160 px wide on a 158 px screen, so the "n" is clipped. The layout check
  reports it "outside the screen" (`position`, `w_status_marks`, in every color scheme).
- **Why it blocks:** English text cut in the middle on a radio this step enables. aeb73816 says "uiThemeTest checks
  it", but it checks only the row count at 158x127.
- **Renders:** `rf-qa11-out/sim/t3.png` (Profile, trunked Home) and `rf-qa11-out/sim/L.png` (GPS Status).

## 3. Logged findings (not blocking)

**Decisions and design**

1. **The fit rule's full layout at narrow widths.** `panels_fits` accepts any color area of at least 158 wide and 200
   high, in the full layout. uiThemeTest even checks 158x240 picks panels. At 158x240 nearly every tile and Quick
   label is middle-cut ("Cont…", "To…FO", "3…0", "1…%"). No radio has that size today, so it is a latent trap for the
   next board (`rf-qa11-out/sim/edge-158x240.png`).
2. **The GD-88 220x176 path is really 210 x 176.** `CONFIG_DISPLAY_INSET_RIGHT=10` applies to both GD-88 panels, but
   the 220x176-panels sim class has no inset.
   - A temporary 210x176-panels class: `--layout` 0 problems, 8 end cuts, the same as at 220. The class should match
     the radio.
   - The sim class also lacks the GD-88's Home keys (`board.home_keys`), so its Home shows only "Menu". The real radio
     shows "Menu | Back".
3. **Settings > Display still offers "GPS clock" on the compact layout**, where the clock is never drawn.
4. **The compact list's height jumps with focus.** It shows 4 rows on a row whose hint is empty (Theme) and 3 rows on
   one with "OK change" (GPS clock, Brightness). This is as decided, but noticeable while scrolling Display.
5. **DM-32 Settings tiles are cut at the end in English** on its default look: "Call setti…", "Tones & ale…",
   "Receive…", "Securit…", "Program…". These are end cuts, so `--layout` passes. gui-plan says "a label still cut goes
   to the user", and these aren't in the list of DM-32 cuts the user decided (`rf-qa11-out/sim/m6.png`).
6. **FONT_AA stays on with panels off.** MD-380 and DB25-D set `CONFIG_FONT_AA ?= y` whatever the panels switch says,
   so a panels=n build keeps the ~6 KB and still changes the original look to anti-aliased text. FONT_AA is its own
   switch and the user decided it, but the gallery's 160x128 class (aa=false) no longer shows what the MD-380 and
   DB25-D draw.

**Code and test gaps**

7. **`ui_draw_list` fetches the focused item one extra time per frame** (`ui_screens.c`, under
   `CONFIG_UI_THEME_PANELS`). This also happens in the original theme on MD-380 and DB25-D builds. I found no item
   function with side effects, so it's harmless, but it is one more codeplug read per frame.
8. **`ui_list_bar` is a global set by `ui_draw_list`.** A `list_geom` call outside a draw (navigation) sees the last
   drawn list's flags.
9. **Mutations no test catches:**
   - **m08:** the toast keeping "Hold * to unlock" off one line.
   - **m10:** the Zones tile's value appearing in the compact title row (decided off).
   - **m15:** the value not drawn while the label slides. The marquee test's `find_text("On")` is satisfied by the
     other rows.
   - **m16:** compact Quick not taking the hint row's room with Hint row off.
   - **m07** was an equivalent mutant.
10. **A stale comment:** the `config/features.mk:211` comment still says panels needs "240 px high with 310 px across".

**Languages (logged for native review)**

11. **Spanish:** GPS signal "9 de 14 en la fijación" runs past 160 px, and one middle cut on `zone_imported`.
12. **French:** "Rechercher : 9999876_" middle cuts.
13. **Many end cuts** in es, fr and zh on the compact class. The non-English `--layout` runs also fail because the
    scripts' `row:` tokens are in English.

## 4. What I ran and what I didn't

**Frames**

- `cc.sh ../rf-base-freeze`: uiSim 0 warnings.
  - 2203 frames compared. All 183 gallery/320x240 frames and all 940 profile renders: 0 differ.
  - The 259 that differ are all expected:
    - DM-32 (panels by default): 183 + 50;
    - the Theme row in Display on 160x128, 220x176 and 240x320;
    - the crash_log line-break fix on 128x64;
    - 5 sheets.
  - uiThemeTest and uiThemeTestPanels pass. uiPrototypeTest OK.
  - GD-88: 0 warnings, flash 798,268, RAM 205,848 (+56 B against the freeze), UI stack 204 B free (as frozen).
  - cc.sh itself ends in "CC FAILED" only because of those intended other-class differences.
- `--layout`:
  - 160x128-panels, 220x176-panels and 240x320i10x5: 0 problems each;
  - temporary 158x127: 20 problems (B2);
  - temporary 210x176: 0.
- `--ellipsis` on all three classes: no middle cuts in English.
- **Targeted scripts:**
  - Home, Menu, Settings, Display, Quick (Hint row on and off), dual watch, keys locked after the toast (English and
    Spanish), the call lines, trunked Home;
  - the slide over time: rest, slide, end and restart (correct timing; the value stays; it restarts after a screen
    change);
  - the slide in es, fr and zh;
  - the fit edges: 158x127, 158x240, 220x199, 320x200.

**Random walks**

- Each class, interactive mode: 3 seeds x 1500 tokens, the slide on, keys, calls, messages, Hint row toggled. No
  failure.
- One more walk on each under ASan and UBSan in local/rf-host-tests. Clean.

**Host suite**

- Whole suite in local/rf-host-tests: rc 0, 0 warnings. That includes uiThemeTest, uiThemeTestPanels,
  uiThemeTestPanelsOriginal and gfxFbEdgeTest.

**Mutations**

- 17 run: 13 caught, 1 equivalent, 3 missed (logged item 9).

**Firmware**

All builds below have 0 warnings and pass the stack check.

| Build | Flash | RAM | UI stack free |
|---|---|---|---|
| MD-380 default (panels, FONT_AA) | 607,356 | 110,920 | 212 B (548 words) |
| MD-380 panels=n | 590,164 | 110,784 | 196 B |
| MD-380 FONT_AA=n | 601,176 | | 224 B |
| MD-UV380 default (panels off) | 712,596 | | 96 B |
| MD-UV380 panels=y | 729,656 | | 224 B |
| DB25-D default | 757,188 | | 204 B |
| DB25-D panels=n | 739,864 | | 204 B |
| DB25-D FONT_AA=n | 750,964 | | |
| DM-32 default | 804,464 | | 272 B |
| DM-32 panels=n | 785,936 | | 272 B |
| GD-88 panels=n | 775,424 | | 204 B |
| GD-88 FONT_AA=n | 777,472 | | 204 B |

- **Symbols:**
  - MD-380 panels=n has no panels symbol;
  - MD-UV380 default has none;
  - MD-380 panels keeps only the 16 px icon set.

**Merge a7ae3db2**

- Read its conflict resolutions with `--remerge-diff`: debuglink audio, `ui_talk_step`, the APRS hint's ok_acts, the
  host Makefile run-core list, the uiFlowTest flows under the new menu paths, `#if CONFIG_AM_RX` placement. No
  problem found.

**Not run**

- the full `tools/ci.sh` matrix and the maximal builds (known overflows);
- other boards;
- `reuse lint` and the doc checkers;
- the other languages' layout beyond the spot checks;
- HIL, the radio, on air.

**Cleanup**

- Worktree and build/t-* removed.
- Outputs are in `rf-qa11-out/` (`sim/`, `fw/summary.txt`, `cc/`, `host-suite.log`).
