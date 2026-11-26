<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# QA review: theme step 3 (menu tree, settings rows, naming), 46dad875..a8591c70

Reviewer: independent QA. Reviewed in worktrees at `a8591c70` (new) and `46dad875` (old), now removed. Scratch output
is in this directory (`r/`, `snap/`, `sw/`, `py/`, the logs).

## Verdict: CHANGES REQUIRED

Two reachability regressions block approval: Data services can no longer be found with Find (finding 1), and with
`CONFIG_UI_SETTINGS=n` the Channels list and Diagnostics can't be reached at all (finding 2). Everything else is a
latent risk, a test gap, a nit or a question for the user.

## Findings

### 1. Blocking defect: Find no longer reaches Data services

At 46dad875, Data services was a row in Settings > Connections, with the synonyms "ARS registration presence server IP
gateway off-net". This step made it Menu > Connections entry 4 (`ui_menus.c` `conn_menu`) and removed the settings row
(`SA_DATA_SERVICES`). The entries of Connections are neither settings nor Find places: `ui_find.c` `places[]` has only
Connections, with the synonyms "hotspot USB network SFR tools".

Evidence, typing on Home in the GD-88 simulator (`uiSim --geom 320x240iR10 --script "<digits>"`, old and new):

| Typed | 46dad875 | a8591c70 |
|---|---|---|
| `3 2 8 2` ("data") | PC answers data, **Data services (Connections)**, Talk to TG... | PC answers data, Talk to TG... |
| `4 2 8 3` ("gate") | Tune VFO, **Data services**, ... | Tune VFO, Talk to TG ... |
| `2 7 7` ("ARS") | APRS, Send position, **Data services**, ... | APRS, Send position, Talk to TG ... |

Renders: `r/f2-rf-qa4-old-*.png` and `r/f2-rf-qa4-*.png` (`sheet-find2.png`).

Fix: add a Find place `{ TRN(SET_ROW_DATA_SERVICES, "Data services"), "ARS registration presence server IP gateway
off-net", SCR_DATA_SERVICES }`, guarded like `DATA_IDS` (the macro lives in `ui_menus.c`, so it has to move to a
header or be repeated), plus a `uiFlowTest` flow that finds it. Then check whether any other former settings row lost
its Find entry. The other rows checked here kept theirs: I compared every settings row id between the two commits,
and only `SA_DATA_SERVICES` left the settings tables.

### 2. Blocking defect (only with `CONFIG_UI_SETTINGS=n`): Channels and Diagnostics can't be reached

`SCR_CHANNELS` and `SCR_DIAG` are now opened only from `group_select` (Settings > Programming), which sits inside
`#if CONFIG_UI_SETTINGS` (`ui_menus.c`, around lines 8046 and 8067). At 46dad875 both were reachable without the
settings menu, at Hub 5 and Tools 7. With the switch off, Menu 8 shows the "not in this build" page, and Find's
"Channels" place opens `SCR_ZONES`. So the Channels list, "New from current" and Diagnostics are compiled in but
can't be reached.

The step's own HIL edits admit this: `tests/hil/test_ui.py` now skips with "built without CONFIG_UI_SETTINGS:
Channels is in Settings > Programming" and "...: Diagnostics is in Settings > Programming". Impact today is limited:
only boards without a display (rt10, gd88b) turn the switch off, but CI builds it off for every board.

Fix, for example: in a `CONFIG_UI_SETTINGS=n` build, have Menu 8 open a page with Programming's actions (Channels,
Zones, VFO, Diagnostics) instead of the "not in this build" page. Or the user accepts the loss and the docs and
`build-config.md` say so.

### 3. Latent risk: the UI task's deepest stack grew, against what the code comment says

`msgs_item` now calls `msgs_text_item`, marked `noinline` (`ui_menus.c` around line 870). The comment says the split
keeps the zone offer's frames off the deep path. In fact the message row's own path gained a frame, and that path
is the UI task's deepest chain (`ui_push > msgs_item > msgs_text_item > msg_row > ui_svc_msg_get > ...`):

| Board | 46dad875 deepest / free | a8591c70 deepest / free |
|---|---|---|
| GD-88 (`make stack-check`) | 1664 / 204 | 1676 / 192 |
| DM-1701 (`make stack-check`) | 1640 / 88 | 1656 / 72 |

The DM-1701 has 72 bytes left. Fix: leave `msgs_text_item` inlined (or `always_inline`) and keep only
`msgs_offer_item` `noinline`.

### 4. Latent risk (for the upstream PR): generic code and docs link to fork-only notes

`fork-notes/gui-plan.md` is cited in `core/ui/ui_menus.c:84` and `:7324`, `docs/ux/information-architecture.md:108`,
`docs/ux/task-flows.md:320` and `tests/host/uiFlowTest.c:121`. CLAUDE.md keeps `fork-notes/` out of upstream PRs, so
these become dangling references there. Cite `docs/ux/information-architecture.md` instead.

### 5. Test gap: mutations at new code that no test caught

Details are in the mutation table below. The gaps:

- The pick list's check mark (M2, mutation inverts it): neither uiFlowTest nor uiPrototypeTest asserts the check.
  The "Pick list: the value in use" flow checks focus only.
- The value editor's markers and accent frame (M12, M17).
- Shrinking `UI_STACK_MAX` from 9 to 7 (M13). No flow takes the deep path (Settings > Programming > Zones > Channel
  actions > Copy here > Name) and goes Back from it, so the new depth isn't held by a test. In the simulator the
  deepest frame I found was depth 7 (the name entry on that path); 9 leaves a margin of one.
- Find opening Location settings without Location underneath (M18): no Back-path flow for Location settings.
- Radio info given a digit (M9), and Radio info's DMR ID value (M15).
- The GPS clock row's visibility (M11) is caught only by uiPrototypeTest, not by uiFlowTest.

### 6. Test gap: HIL `test_theme_is_kept` is wrong on a panels build (the GD-88)

`tests/hil/test_ui.py` `test_theme_is_kept` was rewritten to `ok 8 1 ok down ok`, with the comment "Theme is
Display's first row in the reference theme". With panels active, Display's first row is Display profile. So on the
GD-88 the test sets the Amateur profile, waits for `settings.theme == 1` and times out; its `finally` puts Commercial
back.

The old key path was also wrong under panels, but this step rewrote the test without handling panels. Fix: skip when
the active theme has no Theme row, or reach the row through Find ("theme") and check `ui.focus` before OK.

### 7. Test gap: the HIL Find key paths depend on the order of results

`test_diagnostics_page` types "diag" (`3 4 2 4`) and `language_reset` types "language" in Find. They take the first
result. Find lists channels and contacts (by T9 name, or by ID prefix from 3 digits) before settings
(`ui_find.c` `ui_find_run`).

On a radio whose codeplug has a contact or channel matching 3424, `test_diagnostics_page` opens something else, and
then it `skip("not_built", "built without CONFIG_DIAG")`: a false skip. In the simulator both paths work in en, fr and
zh, on 320x240iR10, 160x120i5 and 128x64. Fix: check `ui.screen`/`ui.focus` after Find, or walk Programming's rows by
`ui.focus` label.

### 8. Nit: screen titles that aren't their row's text

The Naming rule is "a screen's title is the text of the row that opens it", in sentence case. These don't follow it:

- Messages > Zone offers (a row added by this step) opens "Zone Share Received" in Title Case
  (`r/t-2.png`).
- Location > Satellite opens "Next passes" (`r/t-3.png`).
- The channel editor's RX group list row opens "RX Group List" (`CHAN_TITLE_RX_GROUP_LIST`).

### 9. Nit: docs and comments with stale paths

- `docs/ux/dual-watch.md:216`: "Menu > Scan: `OK 6 1`" should be `OK 5 1`.
- `docs/spec/settings-store.md:169`: "Settings > Connections > Zone offers" is now Settings > Call settings > Accept
  zone shares.
- Code comments still name the old places (Settings > Sound / Keys / Key lock / Power / Advanced / My radio / Talk /
  Connections / Location):
  - `core/ui/ui_svc.h`: lines 53, 187, 191, 196, 221, 267, 399, 403, 406, 413-429, 433, 440, 445, 528, 547.
  - `core/ui/ui_screens.h`: lines 125-136 and 193.
  - `core/ui/ui_nav.h`: lines 30-50.
  - `core/ui/ui_svc_storage.c`: line 975.

### 10. Nit: hints that promise more than the rows do

Radio info's hint is "OK open  Back", but OK on Battery, DMR ID and Radio name does nothing (`group_select` has no
case for `SA_INFO_*`). Programming's rows are all actions, but its hint says "OK change  Back".

### 11. Nit: Radio info's GPS row pushes a setting out of Find

Find keeps at most 3 settings (`found < 3`). For "GPS" (`4 7 7`), Radio info's read-only GPS row now takes one of
those slots, so "Answer GPS requests" is no longer listed. Two rows named "GPS" appear, told apart only by their
chips (Radio info, Location settings).

### 12. Nit: French labels middle-elided on 160x120

Examples: "Sécurité et…rrouillage", "Moniteu… départ", "Format…essage", "Tonalités de…gnalisation"
(`sheet-lang1.png`). This belongs to the port step (CLAUDE.md: other display classes are judged after the freeze).
The panels grid elisions on the GD-88 belong to the known layout step.

## For the user (design choices the plan doesn't settle)

- **A. Talkgroup name first now applies to panels, with the default Off.** `UI_THEME_OPT_LABEL_FIRST` was removed, as
  the plan's shared behavior 4 asks. The visible effect on the GD-88's default look: Amateur Home, and Commercial Home
  during TX, RX and dual watch calls, now lead with the number ("TG 3100 · USA") where they led with the name
  ("USA · TG 3100"). See `sheet-oldnew.png` and `snap/profile/*/home_plain.png`, `tx.png`, `rx_group.png`. Should
  panels, or the GD-88, default to name first?
- **B. Decided Menu contents left out.** The Contacts split, the Messages pages, the call log's filters and scan lists
  are recorded as "not built". They are in the plan's table; build them or drop them?
- **C. The two shortened names** the implementer flagged: "DMR monitor at start" and the quick tile's "TG / Contact".
- **D. Where the theme choice goes** (flagged by the implementer). Until it's settled, a radio with the reference
  theme stored can't get back to panels.
- **E. The DMR ID and name editor is in Programming.** The plan names Radio info as read only and doesn't place the
  editor.
- **F. Programming > VFO is an action.** It switches to or from the VFO and goes Home; it isn't a page.
- **G. Language moved to the middle of Display**, following the plan's order. Reaching it takes 12 presses instead of
  5. A user who can't read English has the other-language Find synonyms; is that enough?

## Plan text

gui-plan.md only gained status notes in this range: what is implemented, what isn't built, two names for the user,
the open theme row, and notes for the layout step. No decided text was changed to match the code. Two additions are
the implementer's own choices and are recorded as such: Volume in Tones & alerts, and the VFO also in Programming.

## Checks run

| Check | Result |
|---|---|
| uiSim build at both commits (macOS) | 0 warnings |
| Full host suite in the Linux image (`make -k -C tests/host -j16`, a8591c70) | exit 0. uiFlowTest, uiFlowTestNoColor, uiFlowTestSatellite, uiThemeTest, uiThemeTestPanels, i18nTest x3 and uiPrototypeTest (32) passed. One warning in `pppTest.c:363`, a file this step didn't touch |
| GD-88 build plus `stack-check` (old and new) | ok, 0 warnings. Flash 788140 -> 790436 B (+2296), RAM 205808 -> 205832 B (+24, the 3 extra stack levels). UI stack 1664 -> 1676 deepest, 192 free |
| DM-1701 build plus `stack-check` (old and new) | ok. Flash +2844 B, RAM +8 B. UI stack 1640 -> 1656, 72 free |
| MD-9600 build (old and new) | ok. Flash +2676 B, RAM +40 B |
| DM-1701 with each switch off: UI_SETTINGS, ZONE_SHARE, ZONE_EDIT, CHANNEL_LIST, SIG_TONES, DUAL_WATCH, KEY_ASSIGN, DMR_PRIVACY, CRASH_SCREEN, VFO_FM | all built, 0 warnings. The first ZONE_SHARE attempt failed only because of a debug edit I made in the worktree for a few seconds; the rebuild was clean |
| GD-88 with UI_SETTINGS, UI_THEME_PANELS, ZONE_SHARE off | all built, 0 warnings |
| `tools/i18n.py --check`; `i18n_changes.py 46dad875` | ok (1166 strings, 4 catalogs). 12 case-only English changes with translations unchanged; the translations still read right |
| `check_generic.py`, `check_doc_links.py`, `check_keys.py` | pass |
| Pixel comparison against `rf-base-46d` (snapshot.py; full listing in `diff-list.txt`) | 839 differ, 115 missing; menu, settings and title frames expected. Non-menu frames inspected: Home and banners differ in panels Amateur, and Commercial TX/RX, because of item A. Quick tiles differ because of the "TG / Contact" rename. `home_dual_talk_back` differs in every class because the old gallery script missed the talk back row; the new frame is the correct one. Messages: "OK compose" |
| Simulator renders: Menu, Settings, pick list, value editor on 128x64, 160x120i5, 160x128, 220x176 and 320x240iR10, both themes; Radio info; Programming, Location, Connections, Scan, Tones & alerts, Call settings, Buttons and the other groups; titles of pages | see `sheet-*.png` |
| Languages es, fr and zh in Menu, Settings, Call settings and Tones & alerts, on 160x120i5 and 320x240iR10 | item 12. Italian wasn't compiled into the sim build I used, so it wasn't checked |
| Navigation depth probes (Programming > Zones > editor > Name / TX contact / share) | deepest frame found at depth 7. Old and new Find paths of the HIL tests run in the simulator |
| Random key walks with stack overflow instrumentation (a copy, never the shared checkout): 400 walks of 70 keys in each theme on 320x240iR10 | no overflow, no crash |
| uiFlowTest flow comparison, old against new (`flows-compare.txt`) | 841 -> 871 flows. No expect keys dropped from flows kept under the same name. 36 budgets went up, all explained by longer paths or counted `row:` presses. 6 flows renamed or replaced; "S hardware options open" etc. are kept under new key paths |
| uiPrototypeTest diff read | assertions moved to the new tree. The removed "talkgroup order key only on the legacy Home" test matches the plan; uiFlowTest has the key flows |
| HIL diff read (`tests/hil`) | items 6 and 7; the other key paths agree with the tree |

## Mutations (uiFlowTest and uiFlowTestNoColor; uiPrototypeTest where noted)

| # | Mutation | Result |
|---|---|---|
| M1 | pick list doesn't focus the value in use | caught (25) |
| M2 | check mark on every value but the one in use | **missed** (uiPrototypeTest also missed) |
| M3 | pick list stays open after OK | caught |
| M4 | key pick lists offer every function | caught |
| M5 | Find opens Alert tones without Tones & alerts underneath | caught |
| M6 | Settings 0 doesn't open Find | caught |
| M7 | Menu 6 and 7 swapped | caught |
| M8 | menu digit by row instead of entry | caught |
| M9 | Radio info gets a digit | **missed** |
| M10 | Messages' offer row off by one | caught |
| M11 | GPS clock row shown without GPS / hidden with it | missed by uiFlowTest, caught by uiPrototypeTest |
| M12 | value editor's right marker gone | **missed** (uiPrototypeTest also missed) |
| M13 | `UI_STACK_MAX` 9 -> 7 | **missed** |
| M14 | Connections > SFR opens the status page only | caught |
| M15 | Radio info's DMR ID always "Not set" | **missed** |
| M16 | Radio info's hint "OK change" | caught |
| M17 | value editor's accent frame not drawn | **missed** (uiPrototypeTest also missed) |
| M18 | Find opens Location settings without Location underneath | **missed** |

11 caught by uiFlowTest, 1 more by uiPrototypeTest, 6 missed by both.

## Not run

- The full `tools/ci.sh` matrix (not required for a step).
- HIL and on-air tests (no radio; read only).
- Italian in the simulator.
- A UI_SETTINGS=n simulator walk: finding 2 comes from reading the code and the HIL skips.
- The 220x176 and 240x320 classes in other languages.
- `reuse lint` and the license grep.
