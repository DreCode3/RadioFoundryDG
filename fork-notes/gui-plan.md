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
- Lists of only on/off rows with short labels: 2-column toggle tiles with a state chip.
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
5. Icons and palettes. Also: the VOX and dual watch A/B marks, large on step 2's Home and about 10 px bitmaps since
   step 4; the Settings gear's dark outline at night; Chinese hint row and bar labels touching the bar's edge.
6. Freeze on the GD-88, then port to other display classes.

## Planned, not now

- A user-chosen favorite in main Menu slot 9.
- A PIN for Programming and settings.
- Menu scope (restricted menus) only if a purely commercial radio is supported.
