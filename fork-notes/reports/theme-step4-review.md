# Step 4 (panels layout rework) QA review

Range `e0c529a5..4659ca37` on `feat/gd88-gui`, reviewed at `4659ca37` (the code is the same as `3cdbc1ba`; the last
commit touches `fork-notes/` only). I read the code and the screens without reading any implementer report. Evidence
files are in this directory.

**Verdict: NOT APPROVED.** Two blocking findings, both about the GPS clock in the status strip.

## Blocking

### B1. The clock leaves the strip while the strip still has room for it

- **Where:** `core/ui/themes/panels/panels.c`, `panels_draw_status` (about lines 651-662).
- **What is wrong:** the loop that decides whether the mode word drops from 12x24 to 8x16 (`need`) runs before
  `panels_clock()` fills `t.clock`. So the clock's 116 px never count toward `need`. In a common state the mode stays
  at 12x24 and the clock is dropped, with 113 px of the strip left empty.
  - The spec says the opposite. `docs/spec/ui.md` (step 4 text) says "The mode takes the 8x16 font (from 12x24) before
    anything goes", and the comment above the function says the same.
  - The state that triggers it: GPS fix, GPS clock on, an unread message, AES privacy, scanning. A commercial radio
    with privacy on hits this easily.
- **How I confirmed it:** I rendered that state with uiSim at HEAD (`clock_fix_evidence.png`, row 1: no clock, large
  "DMR", empty middle). Then I moved the `panels_clock` call above the `need` loop and rendered again. Row 2 shows the
  mode at 8x16 and the clock back with room to spare. No test catches this:
  - `test_panels_min_font` frame 3 checks only text size and overlap.
  - uiPrototypeTest only checks that the clock is hidden in an extreme crowd.
- **Smallest fix:**
  1. Move `if (!panels_clock(...)) t.clock[0] = 0;` above the `need` loop.
  2. Add a uiPrototypeTest case:
     `GPS_CLOCK long:back gps:fix msg:9991234,Hello wait:9000 prototype:crypt_aes scankey wait:3000 expect:shows=14:08 expect:layout=0`.

### B2. The clock's low priority breaks a recorded rule, and the test that held it was inverted

- **Rule (gui-plan.md Layout, decided Nov 26):** "The GPS clock stays, in the status strip on every screen (while GPS
  time is valid and the GPS clock setting is on)."
- **What the code does:** `PANELS_SI_CLOCK` comes after IP, power, receive-only and VOX in the order items get room
  (`panels.c` enum at about line 345). Even with B1 fixed, the clock goes in a crowd of a lock, an unread message, AES
  and scanning (`clock_fix_evidence.png`, rows 3-4). Step 2 kept the clock in this crowd: render 35 has it in Home's
  mode bar.
- **Expectations loosened:**
  - `test_commercial_crowded_status_keeps_the_clock_and_device_icons` (step 2: `expect:shows=21_Nov
    expect:shows=14:08`) became `test_crowded_status_gives_way_in_a_fixed_order` with `expect:hides=14:08`.
  - The D-STAR crowded-strip test changed from `expect:shows=14:08` to `expect:hides=14:08`.
  - No recorded decision allows either change. The user docs (`06-home-screen.md`) and `ui.md` now describe the clock
    as low priority.
- **Why it needs the user:** in the step 2 crowd (35), the strip's 290 px can't hold every shared state plus the clock,
  even with the clock high in the order: about 307 px are needed. So something must give, and only the user can say
  what. Options:
  - (a) Move the clock ahead of IP, power, receive-only and VOX (smallest change). It then shows in every state except
    the extreme crowd.
  - (b) Option (a), plus Home's mode bar holds the clock when the strip can't. The mode bar now shows only the mode
    word, and step 2 put the clock there.
  - (c) The user accepts that the clock can drop in a crowd, and the rule is reworded.
- **Smallest fix:** (a), restore the step 2 expectation, or record the user's choice in gui-plan.md first.

## Not blocking: defects, latent risks and test gaps (to log)

1. **Three of the first attempt's five defects have no test.** gui-plan.md step 4 lists them; all five look fixed in
   the renders.
   - Quick's typed TX frequency (defect 1): the mutation `bool typed = false;` in `panels_quick` passes
     uiThemeTestPanels, uiPrototypeTest and uiFlowTest.
   - Quick text at 6x12 (defect 5): the mutation `f = &font_aa_6x12` in `panels_quick` passes all three.
     `test_panels_min_font` never draws Quick.
   - The dual watch banner (defect 3): I found no test aimed at it. A mutation of `panels_banner`'s `x0` passed too.
   - The French bar (defect 2) and the crowded strip overlap (defect 4) are tested: the mutations fail
     `test_hint_structure_survives_translation` and `test_panels_min_font`.
   - Fix: add a Quick frame to `test_panels_min_font`, and a uiPrototypeTest case for `ok 4 up ok quick 5 1 4 6 5`
     that checks the tile shows `146.5`.
2. **The boot call that sets the name-first default is untested.** The mutation `ui_svc_setting_default` →
   `ui_svc_setting_set` in `ui_app_init` (`ui_app.c:2009`) passes radioCoreTest. `test_setting_default` calls the
   helper directly. The helper is well tested: mutations R2, R3, R4 and R5 all fail it, covering a foreign settings
   area, a failed read, and an UPGRADED load with a stored 0 and 1. Fix: a test that runs `ui_app_init` after
   `sim_flash_bad_read` and checks that nothing is written.
3. **`settings_end_default` is fragile** (`settings.c:318-330`, `ui_svc_storage.c:453-458`).
   - It copies whole bytes from `live` into `saved`. An unrelated pending bit in the same byte would be marked saved.
   - It reads `before` from a `settings_begin` whose lock was already released, so another task's begin/end in between
     replaces `before`.
   - I traced both paths: today they are harmless, because the label bit stays dirty and a full-record save follows.
   - Fix: copy by bit mask (`sv[i] = (sv[i] & ~(l[i]^b[i])) | (l[i] & (l[i]^b[i]))`), or make it a
     `settings_end`-replacing call made under the same lock.
4. **The French hint rewrites change other display classes.** On 160x128 and 160x120 (reference theme), 20 rewritten
   French hints are now longer than the row with no short form. Examples (`ref_fr_before_after.png`,
   `fr_small_display_hints.png`):
   - Find loses "# plus".
   - Zones lose "0-9 chercher".
   - The emergency screen shows "OK ac…tter Retour masquer".
   - Quick's split hint loses "écart".

   Some screens got better: "Retour fin" now sits at the right, and a typo ("cahe") is fixed. Step 3 left French cuts
   at 160 x 120 for the port step, so log this there. Fix: `{long|short}` forms for those keys.
5. **French wording.** It is natural overall and keeps the meaning; the infinitive labels ("OK ouvrir") are standard
   French UI style. Problems:
   - Clipped abbreviations, the style AGENTS.md says not to copy: "Maint. OK" (7 strings), "param.", "enreg.",
     "modif./valider". "Appui long OK" or a long/short pair would read better.
   - "OK détail" should be "OK détails".
   - "# ignorer  Retour arrêt" drops "balayage".
   - `QUICK_HINT_SPLIT`'s first form "Saisie : fréq TX" replaces the 0-9 key with a word.
6. **Two guard and coverage nits.**
   - `reference_list_geom`'s new `g->across = false` is not covered: deleting it passes uiThemeTest (an uninitialized
     stack field).
   - `settings_end_default` and `ui_svc_setting_default` aren't guarded like their caller (`UI_THEME_SELECT`). This
     costs nothing with gc-sections, but CLAUDE.md asks for matching guards.
7. **Already there before this step:**
   - The Settings gear's black outline is nearly invisible at night (`x/night_menu.png`; step 5 icons).
   - Chinese bar labels sit on the bar's bottom edge (`zh_bar.png`, already so in step 2). It shows more now that the
     bar has two labels.
   - `pppTest.c:363` has a `-Wsign-compare` warning under Linux gcc in the host suite (file not touched).

## Design questions for the user (visible differences from step 2 that no rule settles)

1. **The mode word appears twice on Home**, in the strip and in the mode bar. Step 2's status marks moved from the mode
   bar into the strip, so the mode bar is mostly empty (see B2 option b).
2. **Hint row off still hides the soft key bar**, as in step 2 (`x/hintoff_home.png`). `test_panels_geometry` now
   asserts this.
3. **Home's left label shows only on the idle hint.** In the reply window, scanning, dual watch and digital monitor,
   the bar has no left label, but OK still opens the Menu (checked with `expect:screen=hub`). The rule "exactly two
   labels" conflicts with "picked from what the hint declares" (`cmp_07_reply_window.png`, `cmpD.png`).
4. **The keys-locked Home has an empty soft key bar.** The sentence moved into the 8x16 hint row; step 2 had it on the
   bar in large text (`cmp_31_busy_strip.png`).
5. **Labels a third of the screen can't hold:**
   - English Quick: "TG" (the decided name is "TG / Contact"), "DMon".
   - French Quick: "Puiss", "Dble écoute".
   - Menu tiles cut with "…": "Journal…" (fr), "Registro…" (es, it), "Impostazio…" (it, Settings), and the scan
     value "Désa…" (fr).
6. **Zones' value in the menu grid is "Loca…"** (4 letters of "Local repeaters"). Is that the "room for Zones' value"
   the user wanted?
7. **The VOX and dual watch A/B marks shrank.** Step 2 drew VOX on Home in 12x24 text and A/B as large boxed letters.
   Now both are about 10 px bitmaps on every screen, lettering below the 8x16 floor in spirit (`cmpA.png`, `cmpD.png`).
8. **The Layout section says "Commercial soft keys: Menu | Quick"**, but the renders show "Menu | Back" (the same since
   step 2).

## What is right

- **Panes:** rounded panes on every screen (strip, rows, tiles, bar, Home's mode bar and channel pane). Day and Night
  in both layouts look right.
- **Text size:** no text under 8x16 on the screens I checked (Quick isn't covered by a test, see item 1).
- **Main Menu:** a 3x3 grid in keypad order, and Up/Down follows the numbers.
- **Soft key bar:**
  - Two labels picked by the hint's structure, never by English words. French and Chinese bars are correct.
  - The value editor shows OK | Back with "OK save  Back cancel" in the hint row.
  - `test_hint_structure_survives_translation` holds every catalog to it.
- **Hint row:** in 8x16, holding the other keys.
- **Lists:** 5 rows, the n/N badge, the accent bar.
- **Home:** the channel name is the main line. A call shows the caller and the talkgroup, so behavior 1 is met.
- **Shared code:**
  - The `ui_list_geom_t` change (3 columns, `across`, `int8_t col_gap`, bit fields) keeps the struct the same size.
  - The reference theme is unchanged: 1363 frames, 0 differ outside `profile/`, and the GPS sky empty state is the
    same at 4 reference geometries.
  - No theme tests in shared code (`ui_gps.c` changes are layout-based).
- **Translations:** `i18n.py --check` passes.
- **Settings:** the step 3 settings risks are fixed and tested (foreign area, failed read writes nothing, a stored 0 in
  an UPGRADED load is kept). The one gap is the boot call's wiring (item 2).
- **Sizes:**
  - GD-88: +1472 B flash, RAM unchanged, UI stack unchanged (1664 deepest, 204 free).
  - DM-1701 against my own `e0c529a5` build: +192 B flash, +8 B .bss (alignment; no symbol changed size). Stack
    unchanged (1640, 88 free).
  - GD-88 with panels off: 774112 B, 0 warnings.

## Checks

**Ran:**

- **`fork-notes/tools/cc.sh --except profile ../rf-base-s4 ../rf-qa5-work`:** CC OK (`cc.log`).
  - uiSim 0 warnings.
  - 1363 frames compared, 0 differ, 840 left out.
  - uiThemeTest and uiThemeTestPanels pass; uiPrototypeTest OK.
  - GD-88 0 warnings; stack check passes.
- **Full host suite in Docker:** rc 0 (`hostsuite2.log`). The first run failed only `run-ui`, because cc.sh had left a
  macOS uiSim binary; the rerun after `make clean` passed. Cleaned afterwards.
- **Firmware:** GD-88, GD-88 with `CONFIG_UI_THEME_PANELS=n` and DM-1701, all 0 warnings. Stack checks for GD-88 and
  DM-1701. DM-1701 baseline built by me at `e0c529a5`.
- **Checkers:** `tools/i18n.py --check`, `check_generic.py`, `check_doc_links.py` and `check_keys.py` all pass.
- **Mutations (17):**
  - Caught (12): U1-U4, U7, U9, U12, T2 (French hint) and settings R2-R5.
  - Survived (5): R1 (item 2), U5 and U6 (item 1; each passed three suites), U11 (item 6), and T3 (item 1; passed two
    suites).
- **Renders:**
  - The 37-screen sheets, compared side by side against step 2.
  - My own HEAD uiSim renders of 30 screens in en, fr, es, it and zh, Day/Night × Commercial/Amateur on Home and the
    menu.
  - French reference theme at 160x128 and 160x120 against my `e0c529a5` build.
  - Crowded-strip combinations.
  - Theme switch to Original and back.
  - 80 random walks with `expect:layout=0` in 5 languages (no failures), and 3 long walks (no crash).
  - Clean-up: worktrees `rf-qa5-head` and `rf-qa5-base` and the `build/t-qa5-*` trees are removed; the main checkout
    is clean.

**Not run:**

- MD-9600 build, the full `tools/ci.sh` matrix, HIL and on-air checks.
- No rebuild of the step 2 binary: I used `rf-step4-out/uiSim-step2` and its `step2/` renders, which match the
  approved look in the brief.
- No rebuild of the GD-88 baselines: the `e0c529a5` numbers (790992 B, and 773892 B with panels off) are from
  `rf-step4-out/base-*.log`, and the +220 B with panels off (probably the longer French catalog) is not confirmed.
