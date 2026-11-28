<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# GUI plan and decisions (fork-only)

Decisions made with the user for the GD-88 GUI work. Upstream docs (`docs/ux/`, `docs/spec/ui.md`) are updated
as each piece is built; this file is the working record until then.

## Goal

A shared theme framework: one stack for behavior, themes for presentation, so a developer can add a new look
without touching menus or behavior. The maintainer has wanted this; the proposal goes to the upstream Discussions
board first ([draft](discussion-ui-themes.md)).

| Shared by every theme | A theme provides (presentation only) |
|---|---|
| Menu tree, settings, keys, navigation, Find, Quick, translations | Palettes (day and night), fonts, metrics, icon set, how lists, grids, toggle tiles, pickers, the soft key bar, the status strip and Home are drawn |

Themes never change the menu structure or behavior. The current look becomes the reference theme ("Original
RadioFoundry"); the user's look is the second theme, with Commercial and Amateur as its two Home layouts. No feature
or menu item is hidden by theme or layout.

### Behavior every theme shares (decided)

The two looks differed in these places; each is now shared behavior, and a theme only decides how it looks:

1. During a DMR call Home shows the caller (name or ID), the destination talkgroup and the signal.
2. The status strip shows the same set of states in every theme: TX/RX, signal, battery, lock, scan, monitor, unread
   messages, GPS (including searching), USB network, privacy. Icon or word is the theme's choice.
3. Large text works in every layout.
4. "Talkgroup name first" (key and setting) works in every theme.
5. Menu tiles show their values (unread count, current zone, scanning), as a badge or a second line.
6. Dual watch keeps its call history and the caller's contact name.
7. Each theme declares its own appearance options; Settings > Display shows the active theme's rows (the reference
   theme: Colors, Highlight color, Filled status bar; ours: Day/Night). Appearance options are not features.

Themes are layered (decided): data (palettes, fonts, metrics, icons) that most users can write, and an ops table
for elements whose composition differs (Home, status strip, list cell, soft key bar) for advanced contributors. The
ops table also lets a new radio with a new resolution or new features get a fitting look without reworking the
shared UI.

Technical choices: per-cell list drawing ops; icons as a data table drawn by one shared blitter; the reference code
stays in `ui_screens.c` until upstream agrees on the shape; the theme id goes in the spare settings byte with 0 = the
build's default. Work goes ahead without waiting for the maintainers' reply to the discussion.

## Menu tree (shared by all radios, commercial conventions first)

Main Menu, a 3x3 grid matching the keypad:

| # | Item | Contents |
|---|---|---|
| 1 | Contacts | Talkgroups, People, Listen lists, Manual dial |
| 2 | Messages | Inbox, New, Quick text, Sent, Drafts, Zone offers |
| 3 | Call log | Missed, Incoming, Outgoing, last heard |
| 4 | Zones | Zone picker, the zone's channels |
| 5 | Scan | Scan on/off, scan lists, Roaming |
| 6 | Location | GPS status, APRS, Satellite, Location settings last |
| 7 | Connections | Hotspot, USB network, SFR (with its settings), Data services, PC and network settings |
| 8 | Settings | Below |
| 9 | (empty) | Planned: a user-chosen favorite |

Settings (Settings > group > item, never deeper):

| # | Group | Contents |
|---|---|---|
| 1 | Display | Theme (builds with more than one theme; first row), the active theme's rows (reference: Colors, Highlight color, Filled status bar; panels: Display profile (Commercial / Amateur), Day/Night, GPS clock), Brightness, Backlight, Large text, Hint row, Talkgroup name first, Language, Battery saver, Auto power off, Low battery warning |
| 2 | Tones & alerts | Tone volume, Key tones, Voice prompts, Vibrate; sub-pages Alert tones (the on/off tones) and Signaling tones |
| 3 | Audio | Mic gain, Mic AGC, Mic noise reduction |
| 4 | Call settings | Reply window, TX timeout, Show talker alias, DMR monitor at start, Answer radio checks, Answer call alerts, Message format, Accept zone shares |
| 5 | Receive & VFO | Default squelch, VFO step, FM broadcast step, Dual watch talk back, Dual watch delay |
| 6 | Privacy | Encryption: key type, key |
| 7 | Security & keypad lock | Auto lock, PTT when locked, Knob when locked (later: a PIN) |
| 8 | Buttons | Long press time, Key repeat, Up/Down on Home, programmable keys |
| 9 | Programming | Channels (edit, new), zone editing, VFO, Diagnostics, Crash log, Freq trim, RX past bands, Hardware, Factory reset (last) |
| - | Radio info | Read-only: battery, DMR ID and name, versions, GPS, codeplug. No digit; reached by Up/Down |

0 stays Find inside Settings. Channels leaves the main Menu (Programming has it). Tools is gone (Connections,
Scan, Location and Programming take its items).

## Naming

Call log, Keypad lock, Radio info, Tones & alerts, Call settings, Talkgroup / Contact (not "Talk to"),
Connections. Sentence case everywhere; a screen's title is the text of the row that opens it. Full words, except
common abbreviations (VFO, SFR, DMR, GPS, APRS, TG, CTCSS, DCS).

Decided Nov 26 (user):

- "Talk to" goes everywhere, action rows included ("Talk to TG 91" in Find and caller actions becomes "Use TG 91", "Use Joe").
- "DMR monitor at start" and Quick tile 8 "TG / Contact" are accepted for room; the picker title stays
  "Talkgroup / Contact".
- The theme choice is Settings > Display's first row, named **Theme**, shown only on builds with more than one theme
  that fits the display. The reference theme's palette row, named Theme until now, becomes **Colors**.
- "Talkgroup name first" works the same in both themes; a radio that starts in the panels theme starts with it on,
  so panels shows the name first by default ("USA · TG 3100"). A radio that starts in the reference theme (its
  display doesn't fit panels) starts with it off, as before. Decided by the theme the radio starts in, not by panels
  being compiled in.
- The Theme row's choices are named "Original" and "Panels".
- Messages' "Zone offers" row shows the offered zone's name, as the Tools row did (menu entries show their values).

## Behavior of settings rows

- On/off: OK flips it, the row shows the new state.
- Several values: a pick list with a check mark on the current value.
- Numbers: a value editor with left and right markers, in the theme's accent color. Amber only for warnings.

## Layout (the second theme, judged on the GD-88)

Decided Nov 26, after step 3's first attempt dropped them:

- **Rounded panes on every screen are the look.** The status strip is a rounded pane, the same on every screen;
  list rows and grid tiles are rounded tinted panes; the soft key bar is a rounded pane; Home keeps its mode bar and
  main pane. The visual reference is the step 2 panels renders (`70a91cc3`, the `profile/` part of the pixel
  baseline). `docs/ux/wireframes/gd88-home-profiles.html` is an earlier prototype, not the reference for the look.
- **The GPS clock stays**, in the status strip on every screen (while GPS time is valid and the GPS clock setting is
  on).
- **No text smaller than 8x16.** A label that doesn't fit gets a shorter word (CLAUDE.md: pick a shorter word, don't
  drop words), never a smaller font.
- **Hints for the other keys** (#, 0-9, PTT, hold OK) go in the hint row when Settings > Display > Hint row is on;
  the soft key bar keeps exactly two labels. The theme picks them from what the hint declares, never by comparing
  shown text (translations).

Decided Nov 26, after step 4's second attempt:

- **Home's strip has no mode word**; Home's mode bar shows the mode (as in step 2). Every other screen keeps the mode
  word in the strip. This is the one exception to "the status strip is the same on every screen".
- **Hint row off hides only the hint line.** The panels soft key bar with its two labels stays on every screen.
- **The hint row shows only what the bar can't**: other keys ("0-9 find", "# more", "PTT talk", "Hold OK save") and
  meanings that differ from the usual ones ("Back undo", "Back stop scan", "OK save  Back cancel" in the value
  editor). A plain "OK open" or "OK choose" is not repeated there.
- **Commercial Home stays as built**: the channel name only, the talk target only when it differs (the rule below).
- **The GPS clock in a crowded strip** (refines "The GPS clock stays" above): the strip can't hold every state plus
  the clock, so the clock ranks right after battery, TX/NO TX, signal, mode, lock, privacy, scan, monitor and unread,
  ahead of GPS, IP, power, receive-only, VOX, dual watch and roaming. On Home, when the strip can't hold it, the clock
  moves into the mode bar (as in step 2). On other screens it can drop in the worst crowd.
- **Keys locked:** Home's soft key bar shows the unlock sentence in large text (as in step 2), not two labels.
- **The Zones tile in the 3x3 Menu shows no zone name**; Home already shows the zone. Messages ("1 new") and Scan
  ("On") keep their values.
- **Home's right soft key stays Back by default** in both layouts (Settings > Right soft key can make it Quick).
- **Quick tile 8 reads "TG/Contact"** in panels (no spaces around the slash; "TG / Contact" is 96 px and a tile
  holds about 88 px at 8x16). **VFO Quick tile 5 keeps "TX freq"**; the hint row says "0-9 TX frequency".
- Forced by the 8x16 rule, accepted: the clock in Home's mode bar is one 8x16 line (step 2: two 6x12 lines).
- The hint row's "only what the bar can't" applies a key at a time: it hides OK's plain open or choose and Back's
  usual job (a plain "Back cancel" on a confirmation included). An OK or Back that does more ("OK send", "OK add",
  "OK in/out", "OK go", "OK use", "OK edit/save", "Back keep editing") keeps its hint, marked by separate OK and Back
  flags on the screen, never by comparing text. So the discard prompt shows "Back keep editing" and not "OK choose",
  and Programming > Channels shows "# go" and not "OK details".
- Text entry, emergency and the hotspot screen keep their hint line with Hint row off (they need their instructions).
- Italian Settings title "Opzioni" (fits the Menu tile) is kept until an Italian speaker reviews it.
- French hints longer than a 160 px row (160 x 128, 160 x 120) go to the port step: their English keys have no short
  forms, so short French forms mean new English alternates too.

- Grids: one fixed column split for the whole list, not per page.
- One row height everywhere; lists show 5 rows on 310 x 240.
- ~~Lists of only on/off rows with short labels: 2-column toggle tiles with a state chip.~~ Dropped Nov 27: plain rows.
- Focus: filled highlight with a drawn accent bar, no "|" character.
- Scroll position: an "n/N" badge in the title.
- Soft key bar: exactly two labels, one per soft key.
- Quick settings in the theme's style, full words.
- The status strip is the same on every screen.
- One 24 px icon family in the theme ink, accent on focus; color only where it means something (TX red, RX green,
  warnings amber).
- Home: the CPS channel name is the main line, zone above it. Commercial shows the talk target only when it
  differs from the channel's own (temporary target, reply window, a call). Amateur always shows it with the details
  row. Home's soft keys: Menu | Back (the right one is Settings > Right soft key).
- A GD-88 starts in the Commercial layout (`gui_flags` 0).

## Process (decided)

Each step is QA-clean before the next starts. An implementer subagent does the work; an independent adversarial
reviewer (QA) reviews it without the implementer's conclusions; findings go back to the implementer and the loop
repeats until QA approves. Work is pushed as it is committed, but a step counts as done, and the next step starts, only
after QA approval. A disagreement between the implementer, QA or
Claude goes to the user, with both positions, and the user decides.

Testing is sized to the risk, not exhaustive (the user wants safe, not past diminishing returns):

| When | Checks |
|---|---|
| Each commit (~5 min) | uiSim builds with zero warnings; pixel comparison against the baseline; `uiThemeTest`, `uiPrototypeTest`; one GD-88 build with zero warnings |
| End of a step | Whole host suite; GD-88, DM-1701, MD-9600 and the relevant switch-off builds; stack check GD-88 and DM-1701; sizes. Other boards only when shared code outside `core/ui` changed |
| QA, once a step (~30 min target) | Read the diff; experiments where the risk is; old-versus-new frames on the baseline set, targeted scripts and one random walk; about a dozen mutations aimed at new code; switch combinations only for switches the step touched |
| QA re-review | The fix and the lines it touched, not a full re-run |
| Milestones only | Full `tools/ci.sh` matrix: at the GD-88 freeze and before an upstream pull request |

Only defects and real latent risks block approval. Test gaps and nits are logged and carried into the next step.

Decided Nov 26, after step 4 took a review and four re-reviews (the user: ensure correctness, but no time or
resources spent on diminishing returns):

- **One QA review per step, and at most one re-review**, limited to the fixes and the lines they touched. Anything QA
  finds outside them is logged, not blocking.
- **A small fix after approval gets one check of that fix only**: it fixes the defect, its test fails without it, and
  nothing nearby broke (reference renders 0 differ, builds clean). Claude may run that check itself.
- **What doesn't block:** wording in languages that aren't a board's default (logged for native speakers), test gaps
  for behavior the renders show working, and nits.
- **Per-commit checks stay as in the table above.** The layout pass in four languages and four variants runs once, at
  the end of a step, not on every commit.

## Order of work

1. Discussion post (draft ready).
2. Framework: extract the reference theme's drawing behind a theme descriptor with no visual change (gallery
   identical), then move the profile look onto it as the second theme.
   - Step 1 (descriptor, list frame, banner box, stack-check-safe dispatch, `uiThemeTest`): QA-approved at
     `2e46cb85`. Cost against `dc9e29d6`: GD-88 +436 B, DM-1701 +112 B, MD-9600 -100 B flash; RAM unchanged.
   - Step 2 starts with two `uiThemeTest` cases QA listed (not blocking step 1): a theme with `top` NULL measures
     through the reference strip (`theme_top`'s fallback passes `draw`), and a list's strip shows the view's state
     while receiving (`reference_list_head` passes `v->state`). Then QA's step-2 notes: the palette in use across
     `ui_theme_use`, and the profile palettes bypassing the theme's palettes and `colors` op.
   - The first firmware build with a second theme is checked with `STACK_CHECK_ARGS=-v` for unresolved theme calls.
   - Decided for step 2: the second theme is named **panels** (`core/ui/themes/panels/`, `CONFIG_UI_THEME_PANELS`).
     The theme id is stored in its **own spare settings byte** (the remaining `reserved11` byte, offset 407; 0 = the
     build's default theme), with `gui_flags` left as it is.
   - Decided for step 2 (Nov 25): the behavior differences that stay until step 4 are **declared theme options**,
     never a test of the theme in shared code. The Display settings rows a theme shows and the talkgroup-name-first
     key are options on the descriptor; "hub tiles show no values, hub titled Menu" moves into the panels list ops.
   - Items 1-4 (rename, move to `core/ui/themes/panels/`, descriptor and pick, `gui_theme` at offset 407) are in
     `6590540a`..`83ec470d`. Theme numbers: 0 the build's default, 1 reference, 2 panels; a stored theme that is
     missing or doesn't fit falls back to the default, then the reference theme.
   - Step 2 (the panels theme: items 1-7, every op moved, appearance options, `UI_THEME_PANELS` only where the
     drawable area is color, >= 310 px wide and 240 px high): QA-approved at `70a91cc3`
     ([review](reports/theme-step2-review.md)). Cost against `3f7ca184`: GD-88 +1912 B flash, +8 B RAM; the switch
     off costs nothing; UI stack unchanged (GD-88 1664 deepest, 204 free).
   - Step 3 starts with what step 2's QA logged (not blocking): a few panels frames or checksums in
     `uiThemeTestPanels`, so the host suite holds the drawing-order rule without the external pixel baseline (finding
     1); restore or pass down `dual_style` instead of a file-static pointer left set (3); a warning at the `UI_THEME_*`
     enum that those palette numbers are the reference theme's only (4). For the menu step (now step 3): no Settings row sets the theme
     yet, so a radio with theme 1 stored can't get back to panels (5).
3. Menu tree, settings row behavior, naming (moved ahead of the layout on Nov 26, so the layout is judged on the
   real labels and the 3x3 keypad grid). First: fix the UI simulator's division by zero on an empty zone
   (`tests/host/sim/ui_sim.c` near the zone stub's `zone_size`; found by QA in step 1, pre-existing, test code only),
   with a test that reproduces it, since this step exercises zone and channel editing. (shared; updates task flows,
   press budgets, IA and translations).
   - QA-approved at `e94841a2` (`7f1299eb`..`e94841a2`; reviews: [step 3](reports/theme-step3-review.md),
     [re-review](reports/theme-step3-rereview.md), [re-review 2](reports/theme-step3-rereview2.md)). Cost against
     `46dad875`: GD-88 about +2.9 KB flash, +64 B RAM; UI stack unchanged (GD-88 1664 deepest, 204 free; DM-1701
     1640, 88 free). The user's decisions are in Naming (Nov 26).
   - Not built, since none exists as a function today: Contacts' Talkgroups / People / Listen lists / Manual dial
     split, Messages' Inbox / New / Quick text / Sent / Drafts pages, the call log's Missed / Incoming / Outgoing
     filters, Scan's scan lists. The VFO stays in Zones as well as in Programming; Volume (radios without a knob) is in
     Tones & alerts.
   - Left for the port step: French labels the 160 x 120 display cuts in the middle ("Sécurité et…rrouillage",
     "Moniteu… départ", "Format…essage", "Tonalités de…gnalisation"); the Chinese user manual pages under
     `docs/i18n/zh/user/` still describe the old menus.
   - For the layout step: the panels grid has no room for Zones' value (the zone's name) beside the label, so it is
     left out there; Messages and Scan show a short form.
4. Layout fixes in the second theme, to the Layout section above.
   - Starts with what step 3's last re-review logged (not blocking): a radio installed over another firmware's
     settings area (`SETTINGS_FOREIGN`, claimed later) never gets the name-first default; on a panels radio the boot now
     saves defaults when the load reports `SETTINGS_DEFAULTS`, which a one-off failed read of the newest slot also
     reports, so a real record could be superseded (keep the bit in RAM until something else saves, or set it only
     when no slot holds a record); and a test that an `UPGRADED` load keeps a stored 0 (number first).
   - Also for this step: the panels value editor's bar shows "Back cancel" (two labels only); the 3x3 Menu grid; room
     for Zones' value in the grid; "Security & keypad lock" shortened in the Settings grid and drawn smaller as a title.
   - A first attempt is on the branch (`596fdd3e`..`55b2bac8`, not QA-approved). Keep: `compare.py`/`cc.sh
     --except`, the step 2 carry-overs (`uiThemeTestPanels` structural checks, `dual_style` restored, the
     `UI_THEME_*` warning), 5 rows of one height, the accent bar, the n/N badge, the channel name as Home's main line.
     Rework: the panes it removed, the GPS clock it moved off the strip, the 6x12 fallbacks, the dropped hints.
   - Defects QA found in it before the review was stopped (evidence in `../rf-qa3-out/`): Quick's TX frequency
     entry no longer shows the typed digits; the soft key bar filters hints by English words, so French shows
     "PTT Parler OK #+"; a banner cuts off the second line of dual watch Home; a busy status strip overlaps its
     icons; Quick in Chinese is unreadable at 6x12.
   - Second attempt on the branch (`d82a2621`..`3cdbc1ba`, not QA-approved): step 3's risks first (the name-first default kept
     in RAM with `ui_svc_setting_default`, new on a foreign area too, an `UPGRADED` test), then the Layout rules on
     the step 2 look. Open questions for the user are in the implementer's report: the mode word in both the strip
     and Home's mode bar; Hint row off still hiding the bar; the Quick and menu labels a third of the screen can't
     hold in 8x16 ("Digital monitor", "TG / Contact", some translations).
   - QA review of the second attempt: [reports/theme-step4-review.md](reports/theme-step4-review.md), not
     approved (the GPS clock dropped from a strip with room, and ranked below VOX, IP and power). The user's answers
     to the open questions are in the Layout section ("Decided Nov 26, after step 4's second attempt").
   - For the port step's translation pass: the older clipped Spanish and Italian hints, now in the panels hint row
     (es "Mant OK guardar", "Atrás fin resp"; it "Tutte impost.", "Tieni OK", "fine risp.", "ferma scan."), and the
     French hints longer than a 160 px row (above).
   - **QA-approved at `7ab0bf3f`**, after the review and four re-reviews in `reports/` (theme-step4-review.md,
     theme-step4-rereview.md to -rereview4.md). Cost against `e0c529a5`: GD-88 +2448 B flash, RAM unchanged; UI stack
     unchanged (GD-88 1664 deepest / 204 free, DM-1701 1640 / 88). Not run: `tools/ci.sh`, `reuse lint`, HIL, on air.
   - Logged for later, not blocking:
     - Port step: `CONFIG_UI_THEME_PANELS=n` costs +316 B flash on the GD-88 (shared code and catalogs), against "a
       switch off costs nothing"; French and Italian cuts on 160 px displays (the reference toast's "…n enregistré");
       the MD-9600 Hardware hint (`HW_HINT`) and the In zones title (`ZONE_TITLE_IN_ZONES`) are untranslated `TRX`.
     - A text fix of its own (it changes 70 reference gallery screens): English "hold OK save" becomes "Hold OK save".
     - Shared wrap: `ui_wrap_line` can start a line with French ":" or "?" (it breaks at the space before them).
     - Native speakers to review: French (the hint rewrites, "Tenir OK"), Italian "Opzioni".
   - After approval, the user found the battery's cap 1 px above the strip's pane (every frame; plain on Home's
     gutter and at night). Fixed in `42f3557a`: marks centered on the pane, the battery at 1.25x (22 px), and a
     uiPrototypeTest check that no strip mark reaches the pane's outer two rows. Checked in one pass, scoped to the fix.
5. Icons and palettes. Also: the VOX and dual watch A/B marks, large on step 2's Home and about 10 px bitmaps since
   step 4; the Settings gear's dark outline at night; Chinese hint row and bar labels touching the bar's edge.
   - Design pass (mock-ups in `../rf-step5-out/`, not committed): inventory, contrast, conflicts with the layout.
   - Decided Nov 26 (user), for the panels theme:
     - **One icon family:** a 24 px canvas (20 px live, 2 px strokes) in the theme ink, the accent on focus, and no
       decorative tints.
     - **Pictures:**
       - Scan: the zigzag with an arrowhead that the strip already draws (`docs/spec/ui.md`), also on the Menu tile.
       - Audio: headphones (a speaker would look too much like the strip's monitor mark). Zones: a folder. Receive & VFO: a tuning dial (a knob with a pointer), not an antenna
         with arcs.
       - Call settings: a handset. Connections: two opposite arrows. Radio info: an "i" in a circle.
       - The rest as in the proposal: contacts, messages, call log clock, location pin, settings gear, display,
         tones bell, privacy key, padlock, buttons keypad, programming sliders.
       - All of them are drawn from first principles (clean room), never copied from another firmware's artwork.
     - **3x3 Menu tiles** are 2 px apart vertically and 39 px high.
     - **The privacy key** is drawn in the theme ink, with a red slash when a call can't be decrypted (no longer green
       while privacy is on).
     - **The soft key bar** is 32 px with a 2 px gap above it, in every language. A bar with Chinese text puts both
       labels at the Chinese text height.
     - **Defaults taken from the proposal, no objection:**
       - Strip words (VOX, RX, TX, IP, the mode) are in 12x24 where the whole strip fits, else in 8x16.
       - The Settings tile icon sits left of the text.
       - Palette fixes: Day strip ink is the palette's fg, not black; Day amber is RGB(122,86,0); the key's grays
         come from the palette.
     - The drawings approved as in `../rf-step5-out/icons_revised.png` (the tuning dial with its ticks outside, as
       drawn). The strip's scan animation is a small gap moving along the stroke, then the arrowhead lit (it stays
       visible with the new Day ink, where the old lighter highlight fades).
     - Built in `934d80c5`, then checked by the user: the scan gap's last step covers the arrowhead (it blinks out
       for one 100 ms step), kept as built. Accepted as side effects: 12x24 strip words 1 px higher (clear of Home's
       mode bar), Home's channel pane 2 px taller (the bar is 2 px shorter), and Home's strip now has room for the
       clock in the states that used to push it into the mode bar.
   - **QA-approved at `934d80c5`** in one review ([reports/theme-step5-review.md](reports/theme-step5-review.md)), no
     blocking findings. Cost against `d51930b7`: GD-88 -1832 B flash, RAM and UI stack unchanged; panels=n 0 B. Not
     run: `tools/ci.sh`, `reuse lint`, HIL, on air.
   - Logged, not blocking: no test reaches the clock's move into Home's mode bar any more (a crowd with
     `rxonly:on pttdown` does); the 12x24 placement fix (`934d80c5`) has no test of its own; the Chinese locked sentence is left-aligned on the bar (older than step 5);
     `panels.c:383`'s comment is stale. Flash sizes on this toolchain change with the build path's length: compare
     sizes from paths of equal length.
     - Logged by the design pass for the build: two roaming animation frames reach the strip pane's outer rows.
5b. Cleanup before the freeze (the user, Nov 26): fix the low-priority issues logged in steps 4 and 5 and fill their
   test gaps, one round and one check. The key under the red slash drawn dim is accepted as built.
   - Done at `5e8fa5f1` (`db0ae971..5e8fa5f1`), checked once. All eleven items and the test gaps are done: panels-only
     list code guarded, the shared wrap never starts a line with ": ; ? !", HW_HINT and the In zones title
     translated, "Hold OK save" (70 gallery PNGs), French, Italian and Spanish short forms and whole words, the
     unlock sentence centered in every language, the bell and scan-speck fixes, the pppTest warning, the zh manual's
     menus, and seven new tests. The panels cell's one-column icon branch was dead and is removed (-84 B).
   - panels=n on the GD-88 is +736 B against `e0c529a5` (same path length). None of it is panels-only code: the
     shared GPS empty-state fix (194 B), translation text (508 B: English short forms, Spanish fixes), the wrap fix
     (26 B). The rule "a switch off costs nothing" holds for the panels code itself.
   - Visible on another display class: 240 x 320 Find now shows "OK open  # more" (was "PTT talk  OK open").
   - Left for the port step: French hints the English also cuts on 160 px (HINT_TYPE_OK_EDIT_SAVE_BACK,
     LOCK_HINT_HOLD_BACK, QUICK_HINT_ALL_SETTINGS, the satellite hints).
   - For native speakers: the French, Spanish, Italian and Chinese wording listed in the round's report, and the
     whole zh manual update. It rewrote some passages; 8 pages have a different heading count from the English
     (04, 06, 07, 10, 13, 15, 19, 23); zh.po's `SET_LABEL_PRIVACY_KEY` and `_KEY_TYPE` may read as a button. Also to
     check: whether `docs/user/18-satellite.md` names the site's Tools menu correctly.
6. Freeze on the GD-88 (the user, Nov 27): frozen at `226ad2d4`.
   - Upstream `main` merged at `66574269` (12 commits, #914 to #1167) with no change to any frame: `cc.sh` against
     `dcf9580a` compared 2203 frames, the 940 profile renders included, 0 differ. Upstream's dual watch digit entry
     (#1154) and volume bar (#1096) show only in states the snapshots don't reach.
   - Two merge fixes: NET_WEBCTL in the version reply's features (`f9019890`) and the feature matrix's bring-up block
     regenerated (`226ad2d4`).
   - Full `tools/ci.sh`: host (Linux image, 187 suites), registry, bench, python, licenses, and firmware for dm1701,
     md380, md9600, mduv380 (Mac) and the whole gd88 matrix (C-SKY image) passed, except two maximal builds that
     already overflowed before the merge: DM-1701 1828 B over, MD-UV380 3140 B over (upstream adds about 170 B).
     Not run: the other C-SKY boards' matrices (db25d, dm1801pro, dm32, gd88b, rt10), `reuse lint`, the
     pytest/numpy checks, the similarity scan, `SANITIZE=1`, HIL, on air.
   - GD-88 default build: flash 791,508 B (88.24%), RAM 205,792 B (95.02%), UI stack free 204 B, 0 warnings.
7. Port to other display classes. The GD-88's frames stay as frozen: compare against a baseline rendered from
   `226ad2d4`. It starts with the CI blockers (the two maximal overflows, the gallery, panels=n's shared bytes) and
   step 5b's leftovers: the French 160 px hints, the native-speaker list, the zh manual's heading counts.
   - Design pass (Nov 27): sheets, facts and options in `../rf-port-out/` (`proposal.md`, `sheets/`). The panels
     theme costs about 17 KB of flash in all (GD-88, on against off: +17,432 B; code 10.4 KB, icons 2.2 KB), about
     23.6 KB with `FONT_AA` on an Arm radio that leaves it off.
   - Decided Nov 27 (user), which radios:
     - **DM-32 (240 x 320, 220 x 310 drawable):** panels, and the default theme. Lists show 7 rows.
     - **MD-380 and DB25-D (160 x 128):** panels selectable in Settings > Display; the reference theme stays the
       default. `FONT_AA` on for the GD-88's glyphs (+6-7 KB).
     - **MD-UV380 (160 x 128):** the same panel, but its default image keeps about 2.4 KB with panels, and its
       maximal build would be about 20.7 KB over. Panels is off in its default image and in `MAXIMAL_SKIP` (the
       rule: off for flash, never `BOARD_UNSUPPORTED`, since the hardware can draw it).
     - **DM-1701 (150 x 120 drawable)** and **128 x 64 mono (MD-9600, DM-1801 Pro):** the reference theme only.
       The fit rule becomes a color drawable area at least 160 x 128 (lists need 3 rows); mono has no color for the
       panes.
   - Decided Nov 27 (user), DM-32: every GD-88 vertical metric kept. Forced by 220 px, accepted: the Connections
     Menu tile reads **"Links"** (a `{Connections|Links}` alternate; titles keep "Connections"); a tile's value (Scan
     "Off") on a third line; Quick tiles use their existing short names; the keys-locked sentence and Amateur's
     details row in 8x16 where 9x18 doesn't fit.
   - Decided Nov 27 (user), after part A's build: on a screen narrower than the GD-88, panels lists use 8x16
     (rows stay 26 px), since their 9x18 labels were cut in the middle; a label still cut goes to the user. Home's
     bar shows only "Menu" on the DM-32 (its Back key isn't a Home soft key; "* Quick" stays in the hint row). The
     focused Menu tile's label starts clear of the accent bar.
   - Decided Nov 27 (user), the last DM-32 cuts: "Display profile" has the short form "Profile" (a `{Display
     profile|Profile}` alternate; the GD-88 and titles keep the long one). A dual watch pane's line that 12x24 cuts
     falls back to 8x16 (a name still too long keeps the shared middle cut). Quick tile 5 shows only the typed digits
     of a TX frequency ("146.1"); the hint row keeps "TX 146.1____".
   - Decided Nov 27 (user), 160 x 128:
     - Strip 18 px with its marks at 0.8x and words in 8x16; margins 6 px; rows and the soft key bar 18 px; every
       list, title, bar and hint in 8x16. The hint row drops its last groups when it runs out of room.
     - Icons: a second, drawn 16 px mask set from `tools/profile_icons.py` (about 1.2 KB), the same pictures.
     - Menu and Quick keep the 3x3 keypad grid: Menu tiles show the icon and digit, Quick tiles the value; the
       title row names the focused tile (and its value).
     - Home: the mode word goes back in the strip on this class only (no mode bar; the GD-88 keeps its exception).
       The call lines take room only during a call, over the target and details rows, so Commercial and Amateur
       both fit whole.
     - A hidden or empty hint row gives its height back on this class only: 4 list rows instead of 3. The GD-88
       keeps the rule as frozen.
     - The GPS clock is off on this class (it never fits the strip, and Home has no mode bar here).
     - Keys locked: a new short form, **"Hold * to unlock"** (128 px, a `{long|short}` alternate, translated).
   - Decided Nov 27 (user), part C: the two maximal overflows (after the merge `a7ae3db2`: DM-1701 2,984 B,
     MD-UV380 4,244 B over; the branch adds 7.1 KB there, about 5 KB shared UI and 3.9 KB from the bring-up branch)
     wait: "get the code complete and we'll optimize for storage later". Until then ci.sh's maximal step fails on
     those two boards only. The five English hint short forms in `../rf-port-out/partC/hint-short-forms.md` are
     approved ("0-9 type  OK edit/save", "Hold Back to unlock", "0 Settings  Hold OK save", "OK track  Hold OK arm",
     "OK track  # TLE  Back"), with the French, Spanish and Italian proposed beside them (native review logged).
   - Decided Nov 27 (user), after the hint short forms (`be8c30db`): the GD-88's one changed frame is kept. The
     reference theme's satellite list at 320 x 240 now reads "OK track  # TLE  Back" (it dropped "# TLE" before);
     every panels GD-88 frame is unchanged. zh got short forms too (the alternate count must match): native review.
   - QA review of step 7 at `6acb11a3` ([report](reports/theme-step7-review.md)): not approved. Blocking: B1, the
     compact trunked / SFR Home overlaps (it goes through the reference Home); B2, English cuts in the middle and
     overflow at the MD-380's real 158 x 127, which no sim class covered.
   - QA fixes `edee0597..9fc4314a` (B1, B2, the fit rule from 220 x 200, the 210 x 176 class, the two decisions
     below, `ui_list_bar`, four tests; the compact trunked call keeps WAITING / ADMITTED). **Step 7 QA-approved at
     `9fc4314a`** on the one re-review ([report](reports/theme-step7-rereview.md)). Logged: at 158 x 127 the
     shared last-heard line cuts a name in the middle after a call ("Q1AB…Joe"); the MD-380's 548-word UI stack
     override may be more than panels needs (324 B free), for the storage pass.
   - **Milestone CI at `799fca27`** (`fork-notes/tools/ci-par.sh`, every board natively, 802 configurations, 51 min):
     all passed except the two deferred maximal overflows (DM-1701 3,552 B, MD-UV380 4,812 B over). Host, python
     (now with the HIL collection, the APRS self-test) and `reuse lint` pass. Step 7 is done apart from that deferred
     storage work.
   - Decided Nov 27 (user), after QA: on a narrow screen the Settings page's tiles put the icon above the label, as
     the Menu tiles do, so the label gets the tile's width on up to two lines (no new words; the GD-88 keeps its
     layout). On the MD-380 and DB25-D the original look keeps its bitmap fonts; only panels draws the `FONT_AA`
     glyphs.
   - Decided Nov 27 (user), after part B's build (`d04d9c40..e48913c5`):
     - **The focused row scrolls:** on this class a text cut at the end in the focused list row slides sideways to
       show it whole; other rows stay cut at the end. The GD-88 and DM-32 don't change.
     - **MD-380 and MD-UV380 fit at 158 x 127** (their insets): the fit rule takes that size. Lists show 3 rows there
       even with the hint row hidden. MD-380 selectable (reference default); MD-UV380 off by default and in
       `MAXIMAL_SKIP`. The MD-380's UI stack grows as needed (84 B free with panels forced on).
     - **A GD-88 with the 220 x 176 panel starts in the compact panels layout**, like the rest of the GD-88 (about
       4.4 KB on every GD-88 build).
     - **Home on this class shows no hint line**, so both layouts always fit whole ("* Quick" isn't shown there).
     - Kept as built: Settings' first level uses the Menu rule (icon and digit tiles, the title names the focused
       group); idle "needs attention" lines take Home room only when shown, like call lines; a long Quick value is
       cut in its tile and shown whole in the title row ("Tone  T100.0").
   - Part A (Nov 27, `c9ae518b`..`cc296f81`, not yet QA'd): the fit rule (color, 160 x 128, 240 px high until part B),
     DM-32 panels by default, Links, the third-line value, the 8x16 fallbacks, the title's room for "n/N" beside a
     body. GD-88 frames and profile renders 0 differ. Open for the user: the DM-32 panels gallery and `--layout` (61
     middle cuts: list labels and three Quick values), so the gallery still shows the reference theme on that class.
   - Part B (Nov 27, `7e9e2363`..HEAD, not yet QA'd): the 160 x 128 fit rule, the compact layout under 200 px high
     (strip 18, rows and bar 18, 8x16, the 16 px icon set, icon tiles, no mode bar, call lines only in a call, the hint
     row's room back, no GPS clock), "Hold * to unlock", the 160x128-panels and 220x176-panels gallery sets, DB25-D
     panels selectable with FONT_AA. Follow-up (`f8ad4b94`..`b51b466e`): no hint line on the compact Home, the 158 x 127
     fit (MD-380 selectable with a 548-word UI stack; MD-UV380 off and in `MAXIMAL_SKIP`), the sliding focused row.
   - Part C (Nov 27, `a7ae3db2`, not yet QA'd): upstream `main` merged (#957..#1185). The DB25-D's #769 sits on the
     shared superhet layer (`at1141_level0`), the GD-88 links `plugptt`. 0 frames differ against `d3c0eb7a` (2571), nor
     the GD-88's against the freeze (840). Open for the user: the maximal overflows (DM-1701 2,984 B, MD-UV380
     4,244 B over; the branch costs +7,092 B against upstream `main` there, about 5 KB of it shared UI spread over
     the menu tree, settings rows, text alternates and fit helpers, the rest `feature/gd88_bringup`'s debug link),
     with no shrink found that keeps every frame; and the English short forms for the 160 px hints
     (`../rf-port-out/partC/hint-short-forms.md`). Still logged: the native-speaker list and the zh manual's 8 pages
     with a different heading count (step 5b). Also for the native-speaker list: the zh `17-settings.md` Theme default,
     Day/Night and Large text rows no longer match the English after the Nov 27 docs pass (only its anchor was updated).
   - QA fixes (Nov 27, `edee0597`..`44f4beb3`, for the re-review): B1, B2 (new class `160x128iL2T1-panels`), the
     220 px full-layout edge, `220x176iR10-panels`, the narrow Settings tiles, the 160x128 original look's bitmap
     fonts, `ui_list_bar`, the mutation tests. GD-88 and profile frames 0 differ; DM-32 only `settings.png`.
   - **The two maximal overflows fixed** (Nov 27, `487576a5..cee9bbcd`; measurement in
     `../rf-port-out/storage/measure.md`): `LTO_3P=1` in both boards' `MAXIMAL_VARS` (3,720 / 3,692 B), language tables
     with an offset every 16 strings (448 B), Settings row descriptors at 20 bytes (400 B), and FONT_AA in the
     MD-UV380's `MAXIMAL_SKIP` (6,080 B; its default image draws no anti-aliased font). Maximal now 1,016 B free on the
     DM-1701 and 5,816 B on the MD-UV380; default images 384 to 720 B smaller (gd88b and rt10 the same). Full CI
     (`ci-par.sh`, every board, at `e5104c5e`, then the registry step again after a comment fix) passed; 2755 frames, 0
     differ.

8. Upstream preparation (Nov 27). The docs for the framework and panels are written (`1d514260`..), with the
   Discussion post (`fork-notes/discussion-ui-themes.md`) and the PR plan (`fork-notes/upstream-pr-plan.md`).
   - Decided Nov 27 (user), on what the docs pass found the plan and code disagree on:
     - **Dual watch's call history is built into panels** (shared behavior 6): a design pass with mocks for each
       display size first, for approval, then the build and one check.
     - **Toggle tiles are dropped** from the plan: on/off lists stay plain rows (the alert tone labels don't fit 2-column
       tiles even at 310 px).
     - **The gallery gets a 320x240 panels class, and its night pages draw the night palette** (they repeated the day
       frame), so the PRs can attach before/after screenshots of the GD-88 look. Done: `320x240iR10-panels` (the
       profile geometry's panel, Home keys and GPS; the 165 of its 189 frames with a frozen profile render equal it),
       and the night page also sets Day/Night (`daynight:` token). The sim's reset now ticks once at 0 first, so
       ui_app_tick's GPS check (a function static) no longer leaks from the previous gallery screen.

   - Decided Nov 28 (user), dual watch call history in panels (design pass: `../rf-port-out/dualwatch/`, option A, the list
     below the two panes, as the reference theme does):
     - **310 x 240:** 2 rows of 8x16 when idle (the state line plus 1 call while receiving). The panes move up 2 to 6 px
       for it (pane padding 2 to 0 px, the gap between panes 6 to 2 px): an accepted change to the frozen look, on dual
       watch Home only.
     - **220 x 310:** Commercial 3 rows, Amateur 2 (Amateur's pane padding 4 to 0 px). **210 x 176:** 2 rows.
     - **160 x 128 and 158 x 127:** 1 row, the newest call (pane padding 1 to 0 px).
     - **Commercial names the caller** in a receiving pane in place of "Receiving" (shared behavior 6).
     - The newest call's age shows in its pane and its list row, as in the reference theme.
     - `#` `4` listing all 4 calls with OK to call back (option C) is a future shared feature for both themes, not now.
   - Built in `533cf58f` (code and tests), `808fccd7` (docs), `5ee4d927` (gallery). The room is measured
     (`ui_dual_style_t.list_rows`: panels asks for 2 lines in the full layout, 1 in the compact one; the panes'
     padding, then their gap, then the 2 px above the lines give way), the rows drawn in the shared dual watch code
     under the style (`dual_call_listed`). Frames: the 20 non-large-text dual watch Home frames of each panels class
     (240x320i10x5, both 160x128 classes, 220x176iR10-panels, 320x240iR10-panels), their sheets and the 4 x 14 profile
     renders; no reference frame. The layout check found "Receiving on A and B" too wide for the compact line: it gives
     the heard pane's line there. Tests: uiThemeTestPanels (rows per display, the 310 x 240 offsets, Commercial's
     caller), uiPrototypeTest (every panels class with the layout check). Flash: GD-88, DM-32, DB25-D +688 B,
     MD-380 +632 B (maximal +648), DM-1701 (no panels) +16 B for the shared width check; RAM unchanged. UI stack
     unchanged (GD-88 1664 deepest, 204 free). Baseline now `../rf-base-step8`. CI: `ci-par.sh --board gd88,dm32,md380,db25d` at `5ee4d927` (360 configurations, the checks included) passed.

## Planned, not now

- A user-chosen favorite in main Menu slot 9.
- Dual watch: `#` `4` lists all recent calls, and OK calls one back (both themes).
- A PIN for Programming and settings.
- Menu scope (restricted menus) only if a purely commercial radio is supported.
