<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# QA re-review: theme step 3 follow-up, a8591c70..ca0b8bc0

Scope: the fixes for the first review's findings (`theme-step3-review.md`), the user's Nov 26 decisions, and the lines
these commits touched. I worked in two worktrees at `ca0b8bc0`, both removed now. Scratch output is in this
directory (`r2/`, `sw2/`, `gal/`, `py/`, `remut*.txt`).

## Verdict: APPROVED, with one latent risk to fix or accept

Both blocking findings are fixed. Data services is back in Find. Without the Settings menu, Channels, Zones, VFO and
Diagnostics are reachable again; I walked every Menu path in that simulator. The user's decisions are built as
recorded. Nothing found here is a defect.

The one latent risk worth fixing now is finding 1: `UI_STACK_MAX` 8 drops a level on one real path. The rest are test
gaps, nits and questions for the user.

## Findings

### 1. Latent risk: `UI_STACK_MAX` 8 is one level short on one real path

The path is Settings > 0 (Find) > APRS > Stations heard > a station > Message > To callsign:

- Stack: Home, Menu, Settings, Find, APRS, Stations heard, station, compose. Then the "To callsign" screen is pushed
  over the full stack.
- With overflow instrumentation in a copy of `ui_push`, the simulator printed
  `QA-OVERFLOW push 54 over 0 2 12 3 22 23 55 8`. The script was
  `ok 8 0 2 7 7 ok up up up ok ok ok ok` on 320x240iR10.
- Effect: the "To callsign" screen replaces the compose screen. OK or a typed callsign then returns to the station,
  not to the message (`got 6 (screen aprs-station)`).
- At a8591c70 (9 levels) this path fitted.
- The same task fits from Menu (Location 2, 7 frames) and from Home's digits (6 frames). Only the Settings > 0 route
  overflows, because Find stays under what it opens.

Other paths I probed stayed within 8 frames:

- Find opening Location settings, SFR, PC and network, Alert tones, Signaling tones, Language and key pick lists.
- Programming > Channels > channel > TX contact.
- The deepest path in the comment (depth 7).

Random walks found no other linear path that overflows: 300 walks per variant, panels and reference starting at
`ok 8 0`, plus the no-settings simulator. The overflows they hit come from the Quick key opened over Quick or Settings
in a loop. That is the stack's designed "replace the newest" behavior, at any size.

Fix: go back to 9 (+8 B RAM). Or have Find give up its own level when it opens a place or a setting: pop it, then
push the parents. That also takes a level off every Find route.

### 2. For the user, and a simulator fidelity gap: where the Talkgroup name first default applies

`settings_defaults` sets the bit under `CONFIG_UI_THEME_PANELS` (`core/settings/settings.c:74`).

- Factory reset goes through `settings_defaults` (`ui_svc_storage.c:1522`), so it turns the bit on.
- A stored record keeps its bit: `settingsTest.c:748` checks a stored 0. I mutated the default away and
  `settingsTestPanels` failed.
- So the build is right for "a build that compiles panels in".

The case that differs is a GD-88 with the 220x176 panel. Same firmware, but panels doesn't fit, so it runs the
reference theme and still starts with name first On.

- Literally, "a build whose default theme is panels": the build's default is panels (`UI_THEME_ID_BUILD`); this
  display falls back.
- The simulator disagrees with the firmware here. It sets the bit from `gui_profiles` (`sim/ui_sim.c:2914`), so the
  220x176 gallery and Display page show Off, while the radio starts On (`r2/th6.png`).
- `docs/user/17-settings.md:66` says "On where the firmware's default look is Panels", which reads either way.
- The user should say which is meant. Then align the simulator with `settings_defaults`, or tie the default to the
  theme that fits.

### 3. For the user: the Messages row for a zone offer no longer shows the offer's name

The stack fix (54699fd5) drops the offered zone's name from the row: the Messages row now reads "Zone offers" alone
(`r2/off2.png`). At 46dad875, Tools' row showed the name. The offer screen still shows it in its text.

The offer screen's title is now "Zone offers" for a zone and a channel alike. It was "Zone/Channel Share Received";
the body still says which it is.

The UI stack is back to the base on both boards:

| Board | Deepest / free |
|---|---|
| GD-88 | 1664 / 204 |
| DM-1701 | 1640 / 88 |

`offer_waiting` now uses `ui_svc_zone_offer_pending`.

### 4. For the user: the theme names "Original" and "Panels"

These are the visible values of the new Theme row, chosen by the implementer.

### 5. Test gaps: mutations that no test caught

| Mutation | What it does | Result |
|---|---|---|
| N4 | Removes the reapply of Colors, Highlight color and Filled status bar after a run-time theme switch | Not caught. Its effect is real: switch Panels > Original with Colors Light and Home is drawn Dark, background (0,0,0) instead of (255,255,255). Checked with clean rebuilds |
| N6 | Find no longer puts Radio info's values last | Not caught. Commit 256697ae's claim ("GPS" finds Answer GPS requests) has no flow |
| N7 | "Use" without its space | Not caught. No flow checks the action row's text ("Use TG 91") |
| M15 | Radio info's DMR ID value always "Not set" | Not caught, as in the first review |

### 6. Nit: the gallery frames left out are a real change, not nondeterminism

The frames are home_fm, home_monitor and home_temporary on 220x176 and 320x240.

- The same commit rendered twice: identical, both through `uiSim --gallery` and as single scripts.
- Against the committed PNGs they differ in one line. The committed frames show the TX frequency
  ("→147.660 MHz"); current builds show the RX frequency with its offset ("147.060 MHz +600k").
- That is upstream's "Home: FM channel shows its RX frequency (#1043)". It updated 160x128 and other classes, but the
  220x176 and 320x240 PNGs were last regenerated in 955aabfb, before it.
- Not from this step. Regenerate and commit them, rather than calling the difference noise (`sheet-gal.png`).

### 7. Nit: wrong or stale paths

- `core/ui/ui_screens.h` (auto_lock_s) and `core/ui/ui_app.c:2820` name Auto lock under "Settings > Buttons". It is
  under Security & keypad lock.
- `docs/spec/ui.md:827` still lists the DMR quick tiles as "Timeslot, Color and Talk to". The tile is now
  "TG / Contact".

### 8. Nit (test only): `ui_focus_drawn` read across tasks

`ui_focus_drawn` (`ui_screens.c`) is written by the UI task while drawing. The debug link reads it through
`ui_app_status`, apparently without a copy under a lock. A HIL poll could read half a label now and then, which only
makes a test retry. RAM cost is 36 B (DM-1701 RAM +48 B against a8591c70).

## The user's decisions: built as recorded?

| Decision | Result |
|---|---|
| Theme row first in Display, only where more than one theme fits | Yes. Shown on 320x240iR10 and hidden on 220x176 (`r2/th1.png`, `th6.png`); `set_hidden` counts themes that fit. A stored 0 shows the theme in use, from `ui_theme_cur()` (N3 caught). Picking switches at run time (N5 caught) and stores `gui_theme`. The reapply after a switch works: Colors Light survives Panels > Original (`r2/c4.png`, `h1.png`); finding 5 covers its missing test. A reference theme stored on a GD-88 can now get back to panels |
| The palette row is "Colors" | Yes (N9 caught) |
| Talkgroup name first the same in both themes; panels builds start On; a stored 0 keeps its meaning | Yes, with finding 2 for the 220x176 case |
| "Talk to" removed; "Use TG 91" / "Use Joe" | Yes on action rows and in Find (`r2/act1.png`, `act2.png`), in all 4 catalogs. The remaining "Talk to" is prose (docs/user verbs) and code comments, plus the ui.md:827 nit |
| "DMR monitor at start", "TG / Contact" | Unchanged, as accepted |

## Earlier findings: status

| # | First review | Now |
|---|---|---|
| 1 | Data services not in Find | Fixed. "data", "gate" and "ARS" list it, in the normal and the no-settings simulator (N1 caught, 15 flows) |
| 2 | UI_SETTINGS=n: Channels and Diagnostics unreachable | Fixed. Menu 8 lists Programming's pages. Every Menu path was walked in `uiSimNoSettings`, and the Makefile's run-ui checks pass. Without settings Find still doesn't list Diagnostics; neither did 46dad875 |
| 3 | Stack frame | Fixed (finding 3) |
| 4 | fork-notes references | Fixed (grep clean) |
| 5 | Test gaps | Mostly closed (mutation table) |
| 6, 7 | HIL Colors/Theme and Find order | Fixed by reading the label (`open_setting` with `ui.focus_label`, compared in every catalog). I read it and didn't run it (no radio) |
| 8 | Titles | Fixed: Zone offers, Satellite, TX contact / RX group list (`r2/v1-v3.png`) |
| 9 | Stale docs | Fixed, except finding 7 |
| 10 | Hints | Fixed: OK choose, OK save / Back cancel, Back on value-only rows (`r2/v4-v5.png`) |
| 11 | Find GPS | Code fixed; no test (N6) |

The 9243663e skim (86 files) found only comments, docs, HIL docstrings and messages, and two `tools/rfoundry`
message strings with their test. No code behavior changed. Wrong new paths: finding 7.

I didn't verify the uiFlowTest failure at 9229c79f that the next commit fixes. I didn't build that commit.

## Checks run

| Check | Result |
|---|---|
| uiSim, uiSimNoSettings, uiFlowTest x3 and radioCoreTest builds (macOS) | 0 warnings |
| uiFlowTest, uiFlowTestNoColor, uiFlowTestSatellite | passed |
| radioCoreTest | 7224 checks, passed |
| settingsTest and settingsTestPanels | passed |
| uiPrototypeTest | passed |
| uiThemeTest and uiThemeTestPanels (Linux image) | passed |
| The Makefile's uiSimNoSettings run-ui lines | pass |
| GD-88 build and stack-check | 0 warnings. Flash 790688 B (+252 vs a8591c70), RAM 205864 B (+32). UI 1664 deepest, 204 free |
| GD-88 with UI_SETTINGS and UI_THEME_PANELS off | built, 0 warnings |
| DM-1701 build and stack-check | 0 warnings. Flash 709044 B, RAM 127568 B. UI 1640 deepest, 88 free |
| MD-9600 build | ok, flash 688032 B |
| DM-1701 switch-off builds, each with 0 warnings | UI_SETTINGS=n; UI_SETTINGS=n + DIAG=n + CHANNEL_LIST=n; UI_SETTINGS=n + VFO=n; UI_SETTINGS=n + ZONE_EDIT=n + ZONE_SHARE=n; ZONE_SHARE=n; DMR_ARS=n + DMR_IP_GATEWAY=n; DMR=n |
| `i18n.py --check`, `check_doc_links`, `check_generic`, `check_keys` | pass |
| Simulator: theme row, switches and reboot in both directions; Colors and Night through switches | correct |
| Simulator: every Menu path without settings; Find for Data services; action rows; fixed titles and hints | correct |
| Depth probes with overflow instrumentation, plus 900 random walks | finding 1 |
| Gallery determinism (two full gallery runs and single renders) | finding 6 |

## Mutations

| Mutation | Result |
|---|---|
| M2 check mark inverted | caught (checked=) |
| M9 Radio info numbered | caught (nodigit=) |
| M12 editor's right marker gone | caught (editor=) |
| M13 `UI_STACK_MAX` 8 -> 7 | caught ("Back along the deepest path") |
| M15 Radio info DMR ID | **missed** |
| M17 editor frame not drawn | caught (framed=) |
| M18 Find without Location underneath | caught |
| N1 Data services place removed | caught |
| N2 Theme row always shown | caught (uiPrototypeTest) |
| N3 Theme row shows the stored 0 | caught (uiPrototypeTest) |
| N4 no reapply after a theme switch | **missed** (effect confirmed) |
| N5 no run-time switch | caught (uiPrototypeTest) |
| N6 Radio info first in Find | **missed** |
| N7 "Use" without its space | **missed** |
| N8 pick row hint says "change" | caught |
| N9 Colors row hidden in the reference theme | caught (uiPrototypeTest) |
| N10 talkgroup-first default removed | caught (settingsTestPanels) |

13 caught, 4 missed.

## Not run

- The full host suite and `tools/ci.sh`. Per Process this was a re-review; I ran the UI, settings and radio-core tests
  above.
- HIL (no radio; read only).
- Languages other than English for the new rows.
- The 9229c79f failure.
- `reuse lint`.
