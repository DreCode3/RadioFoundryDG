<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Step 4 (panels layout) QA re-review 2: the last small round

Range `6223fc87..a8d7352b` on `feat/gd88-gui`, reviewed at `a8d7352b` (six commits). I read the diff, the screens and
the renders myself. I didn't read any implementer report. The evidence is in this folder (`rf-qa7-out/`).

**Verdict: NOT APPROVED.** There is one blocking finding (B1): the channel editor's "In zones" list, where OK adds the
channel to a zone or removes it, isn't flagged. So its "OK in/out" hint is hidden, and the decided rule names "OK add"
as a hint to keep. The fix is one flag. Everything else this round had to build is built and tested, and the two
judgment calls are sound.

## What the round had to build

| Item | State | Evidence |
|---|---|---|
| Quick tile 8 "TG/Contact" in panels | Done. `panels_quick_word` uses `QUICK_LABEL_TG_CONTACT` as the shorter form for `QT_TALK_TO`. It fits in all four profile variants and in es, fr, it and zh | `quick_langs.png`; test `test_quick_words_fit_a_third_of_the_screen` (mutation Mi caught) |
| The hint row hides only a list's plain open and choose; a flag keeps the rest | Done, flag-based: `SF_OK_ACTS`, then `ui_list_t.ok_acts`, then `UI_BAR_OK_ACTS`. `hint_fn(bool *ok_acts)` covers Settings' per-row hints. No text is compared. **One list is missed (B1), and one more in code that the GD-88 doesn't build (N1)** | `flagged_hint_rows_old_new.png`, `flagged_hint_rows_langs.png`, `unflagged_lists.png` |
| Text entry, emergency and hotspot unchanged | Their Hint row on and off frames are the same as at `6223fc87` in en, es, fr, it and zh. The one exception is Spanish text entry, where the row now draws in full from x 6 (the second judgment call below) | `oldnew.log`; my old-versus-new script run |
| Italian "Opzioni" unchanged | Unchanged | `it.po` |
| D1: French and Spanish unlock sentences in whole words | Done. es: "Teclas bloqueadas: mantén * para desbloquear" and, short, "Desbloquear: mantén *". fr: "Verrouillé : tenir * pour déverrouiller" and "Déverrouiller : tenir *". Both now fit the large 12x24 bar font, where the old clipped forms fell back to 8x16 | `lock_bar_langs.png`, `lock_es_fr.png` |
| Tests for M4, M16, M24, M27 | Added, and each one fails without its code (Mk, Mj, Ml and Mm below) | `mutations.log` |
| ui.md line wrap | Every line the round added is 120 characters or less. The file's older long lines (20-29) are untouched | `awk` check |
| `ui_svc_setting_default` guard | The declaration, the definition, the stub and the simulator's copy are all under `UI_THEME_SELECT`, as is their caller in `ui_app_init`. `settings_end_default` is under `CONFIG_UI_THEME_PANELS \|\| UI_THEME_SELECT`, which is wider than its one caller (safe). The GD-88 builds with panels on and off with 0 warnings | `gd88.log`, `gd88np.log` |
| French "enregistrer" throughout | **Only partly done** (N2): the hints, "Oui, enregistrer" and "Rien à enregistrer ici" changed. About 15 other strings still say "Sauver"/"sauvé", some of them on the same screens | `fr.po` |

## Blocking

### B1. "In zones" (`chz_screen`): OK adds the channel to the zone or removes it, but the list isn't flagged

- **Where:** `core/ui/ui_menus.c:4465`, `chz_screen`, `.flags = SF_LIST`, hint `"OK in/out  Back"`.
  - The path is Settings > Programming > Channels > a channel > Zones.
  - It is built on the GD-88 (`CONFIG_ZONE_EDIT` defaults on with channel editing).
- **What OK does:** `chz_select` posts a `ZONE_OP_INSERT` or `ZONE_OP_REMOVE` for the zone, which writes the codeplug,
  then shows the "Added to" or "Removed from" toast. That is "OK add", which the decision names as a hint to keep, plus
  its inverse.
- **What the screen shows:** the hint row is empty and the bar reads only "OK | Back" (`unflagged_lists.png`, first
  frame). Before this round, the hint row was hidden there too. The rule decided on Nov 26 is that such a list "keeps
  that hint, marked by a flag on the list".
- **Fix:** add `| SF_OK_ACTS` to `chz_screen`'s flags.
  - Test: a `uiPrototypeTest` subTest, `("ok 8 9 ok ok up up up up ok", "OK_in/out")`. The script is the
    uiFlowTest 26 path.
  - The string is `TRX` (untranslated), so the English check holds in every language.

## Not blocking: defects, latent risks and test gaps (to log)

### N1. Hardware options (`hwopt_screen`): "OK next" steps the value, but the list isn't flagged

- **Where:** `ui_menus.c:8473`, hint `"OK next  Hold OK store"`.
- **What OK does:** `hwopt_select` steps the shown choice on OK (a change, like Settings' "OK change", which
  `group_hint` flags). With the flag missing, the row reads "Hold OK store" alone (`unflagged_lists.png`, second frame).
- **Why it doesn't block:** the screen is built only with `BOARD_HWOPTS=y` (MD-9600 today), not on the GD-88.
- **Fix:** the same one-flag fix, before the port step.

### N2. French "enregistrer" is not used throughout

- **What remains:** "Sauver"/"Sauvé"/"sauvées" remain in `ACT_ROW_SAVE_AS_CONTACT`, `ACT_ROW_SAVE_TO_CHANNEL`,
  `CHAN_ROW_SAVE_AS_NEW`, `CHAN_TOAST_SAVED_AS`, `CHAN_TOAST_NUMBER_IS_FIXED`, `DISCARD_TITLE` ("Modifs non sauvées"),
  `DISCARD_TITLE_DMR`/`_FM` ("Sauver en DMR?"), `CONTACT_TOAST_SAVED`, `BODY_SAVED_IN_CODEPLUG`, `CHIP_NOT_SAVED` and
  others.
- **Mixed on one screen:** the discard prompt now has the title "Modifs non sauvées" over the row "Oui, enregistrer".
- **A clipped non-word in the same flow:** `CHAN_TOAST_SAVE_FIRST` is still "Sauver d'ab: maint OK". It has the same
  problem as D1, and its verb doesn't match the new "Tenir OK" either.
- **Why it doesn't block:** it's translation consistency. The first review logged it as a nit (N5), and French isn't in
  the default GD-88 build (`CONFIG_LANG_FR=0`).

### N3. Test gaps: flags no test holds

These mutations survived (details in `mutations.log`):

| Id | Mutation | What it leaves untested |
|---|---|---|
| Mg | discard prompt unflagged | "Back keep editing" on the real screen. Only uiThemeTest's unit `BAR` line checks the flag's effect |
| Mq, Mr | contact editor and channel details unflagged | "OK change" in the row on those screens |
| Mu, Mv, Mx, My | dual watch pick, fixed position, share to and DMR ID unflagged | Those screens' "OK watch", "OK change", "OK send" and "OK edit/save" |
| Mw | `group_hint` leaves `ok_acts` true on rows that open a page | A Settings "OK open" row hidden from the hint row. The test hides only "OK choose" (a pick list) and a message's "OK open" |
| Mn | `room` back to `gfx_width() - 30` | The 298 px room. No test has a row of 281..298 px |
| Mo | a wide row drawn at x 15 | The x 6 start. That row's text would run past the right edge |

- **Overall:** 25 mutations plus a baseline. 15 were caught: Ma-Mf, Mh-Mm, Mp, Ms and Mt.
- **The required ones:** the four mutations the last review listed (M4 as Mk, M16 as Mj, M24 as Ml, M27 as Mm) are all
  caught now. M24 is caught by the new `uiSimFr` check only, as intended.
- **The flag's plumbing is held** by the uiPrototypeTest subtests and uiThemeTestPanels (Ma-Me, Mf, Mh, Mp, Ms, Mt).
- **Cheap additions** to `test_list_hint_row_keeps_what_ok_does_where_it_does_more`:
  - Discard: `"ok 8 9 ok ok ok 4 4 back"` and "keep_editing".
  - Channel details: `"ok 8 9 ok ok"` and "OK_change".
  - "In zones" (B1).
  - A Settings action row with `expect:hides=OK_open`.
- **The x 6 row:** a check on Spanish text entry, or on any 281..298 px row, would hold it. For example, the row's first
  ink column below x 15 on `uiSim` built with `UI_LANG=es`.

## The two judgment calls

### French "Tenir OK" (was "Maintenir OK")

- **Sound, but not the most idiomatic choice.**
  - "Maintenir" (maintenir la touche OK) is the usual French verb for holding a key, and "Appui long OK" is common
    in device UIs.
  - "Tenir OK" is understood and is a real word, so it meets the whole-words rule.
- **It's needed for the fit.** "1-9 modifier  Maintenir OK enregistrer" is 312 px, more than even the 298 px row, while
  "Tenir OK" fits in 280 px. "Appui long" is as long as "Maintenir".
- **It's consistent.** All the hints use "Tenir"/"tenir", and so do the unlock sentences.
  - Older toasts still say "maint" ("Sauver d'ab: maint OK", N2).
  - So does "Touche p/ maintenir" (pre-existing).
- **What's left is design question D2,** for a French speaker.

### A wide hint row starts at x 6 and uses 298 px

The code is `room = gfx_width() - 12`. The row draws at x 15 when it fits in `gfx_width() - 30` (280 px), else at x 6.

- **Rows that fit draw as before.** If a form fits in 280 px it also fits in 298 px, so the level loop stops at the same
  form, and the x stays 15.
  - My instrumented crawl covered 4200 frames: the gallery plus Settings depths, in four variants and five languages.
    Every row of 280 px or less is at x 15.
  - My old-versus-new comparison of the same set (4400 frames) found no frame whose only change is the row's position,
    except the wide rows below.
- **No overlap.**
  - **Bar and pane edges:** the text cell is y 186..201 and the bar starts at y 202, so they can't touch vertically.
    Horizontally, the ink runs from x 7 to at most x 300, inside the panes' x 4..305.
  - **The last row's panel:** in plain lists it reaches under the row's blank top. Its pixel map shows the panel ending at
    y 186 with one clear line before the glyphs start at y 188. That is the same gap as at x 15. The glyphs' left edge,
    x 7, lines up with the panel's visible left edge, and the focus's accent bar ends above it.
  - **Variants:** I checked this in all four (commercial and amateur, day and night) on Spanish text entry
    (`x6_text_entry_sheet.png`). I also forced a 296 px list hint on Contacts with the last row focused
    (`x6_list_sheet.png`, `x6_list_corner_zoom.png`).
- **Real cases today.** Only Spanish text entry, "Atrás borrar  # ABC/123/abc  OK listo" (296 px), in all four
  variants. It now shows all three groups.
- **One behavior to know about:** a level-0 form of 281..298 px is now shown in full at x 6, where it used to fall back
  to the shorter alternate at x 15. So a wider row wins over a shorter alternate as well as over dropping a group. That
  is consistent with "before dropping a group".
- **Test gap:** nothing tests the x 6 position or the 298 px room (mutations Mn and Mo, in N3).

## The flag on the screens: full audit

These are all 53 `SF_LIST` screens in `core/ui`.

**Flagged, and they should be:**

- compose ("OK send")
- contacts ("OK talk")
- zones and zone channels ("OK go")
- share to ("OK send")
- offer to zone and add channel ("OK add")
- picker ("OK use")
- dual watch pick ("OK watch")
- channel details, contact editor, fixed position, Settings rows ("OK change", "hold OK save")
- DMR ID and data services ("OK edit/save")
- signaling steps ("OK next")
- discard ("Back keep editing")
- zone offer preview ("OK or Back returns")

**Flagged, and arguable (design question D1):**

- Settings > Programming > Channels, "OK details". OK there opens the channel's details page, which is a plain open.
  It's useful to show because `#` tunes there, unlike the field lists, but by the letter of the rule it's an open.
- The discard prompt's "OK choose" now shows beside "Back keep editing". The flag is per list, so it can't keep one
  group and drop the other.

**Not flagged, but OK or Back does more:**

- "In zones" (B1).
- Hardware (N1).
- Factory reset, delete zone and old codeplug, "OK choose  Back cancel". Back there is the usual leave, and the rows
  say "No, keep ...", so I count these as correct. D1 asks whether "Back cancel" should stay as "Back keep editing"
  does.

**Not flagged, and correct:**

- Find ("OK open").
- Messages: `msgs_hint` leaves `ok_acts` false for "OK open" and "OK compose", which opens compose.
- Message actions ("OK choose").
- APRS station ("OK choose").
- Crash "Send to": "OK pick" saves the choice and leaves. It sends nothing.
- `pick`, `lang`, `share`, `offer`, `crash` ("OK pick").
- The numbered lists ("1-n pick", which has no OK group).
- Info ("Back").
- Satellite passes.

**`group_hint`:** `ok_acts` is true for number rows (change, and save or cancel while editing), on/off rows ("OK
change") and "Save GPS position". It is false for pick lists ("OK choose") and action rows that open a page ("OK
open"). Mutations Mc and Ms catch the first two. Mw, which leaves `ok_acts` true on rows that open a page, survives
(N3).

## Reference theme and other display classes

- **`cc.sh --except profile`:** 1363 frames, 0 differ (`cc.log`). That covers the reference gallery and the Settings
  depths at 160x128 and 240x320.
- **What the shared code adds:** a local `ok_acts`, and `(void)ok_acts` without panels.
  - `ui_list_t.ok_acts` exists only under `CONFIG_UI_THEME_PANELS`.
  - The reference theme never sees `UI_BAR_OK_ACTS`, which is defined only with panels.
- **Size with panels off:** GD-88 is 774172 B, against 774148 B at the last review: +24 B, the cost of the `hint_fn`
  out-parameter and the local in `list_draw`. It is a nit, of the same kind as the first review's N4 (the port step's
  switch-off cost).

## Translations of the new strings

- **"TG/Contacto" (es), "TG/Contatto" (it), "TG/Contact" (fr) and "TG/联系人" (zh):** natural, the usual terms in each
  language. Each fits tile 8 at 8x16 (`quick_langs.png`).
- **Spanish unlock forms:**
  - They use "mantén", the tú imperative that the Spanish catalog uses elsewhere (25 tú imperatives against 1 usted).
  - "Desbloquear: mantén *" is a label followed by an instruction, which is acceptable in a UI. The fuller "Mantén *
    para desbloquear" doesn't fit the large font (300 px against 294 px).
  - The PTT toasts, "PTT inactivo con bloqueo: mantén * para desbloquear", are natural.
- **French unlock forms:**
  - They are whole words with the French space before the colon.
  - "PTT coupé : tenir * pour déverrouiller" leaves out "while locked", but the toast appears only while the keys are
    locked, so nothing is lost.
- **Fit:** all fit their bars (`lock_bar_langs.png`).
- **Now visible because of the flag:**
  - French "OK modif" (HINT_OK_CHANGE_BACK) is a clipped noun. It is on every French Settings on/off and number row.
  - Spanish "mant OK guardar" is on channel details.
  - These are old strings, but the flag now shows them (`flagged_hint_rows_langs.png`). The Spanish one is logged in
    gui-plan.md for the port step. The French one isn't (nit N4).

## Nits

1. **N4.** fr "OK modif" (above). Add it to gui-plan.md's port-step translation list beside the es and it forms.
2. **N5.** The English `HINT_OK_CHANGE_HOLD_OK_SAVE` has a lowercase "hold" ("OK change  hold OK save"), while every
   other hint says "Hold OK ...". This is old, but it is now in the hint row of channel details and the contact editor.
3. **N6.** `ui_svc.h` now repeats `ui_theme.h`'s `#ifndef UI_THEME_SELECT` default. The two are identical today, but
   it's one more place to keep in step. Including `ui_theme.h` in `ui_svc.h`, or a comment that points at it, would
   keep one source.
4. **N7.** `settings.c` tests `UI_THEME_SELECT`, a UI macro, which only a `-D` can set there. It's harmless, but the
   settings layer now knows a UI name.
5. **N8.** `uiSimFr` adds a fourth full uiSim build to every host suite run for one script. It could reuse
   `UI_LANG=fr`'s build, or `i18nTest`'s, if build time matters.
6. **N9 (old, seen in passing).** Text entry's "12/16" counter is drawn to x 307, past the panes' right edge (305).
   This round didn't change it.

## Design questions for the user

1. **D1. Edge cases of "only what the bar can't".**
   - Should "OK details" (Programming > Channels) stay in the row? It's a plain open, but it's useful there.
   - Should the confirmations' "Back cancel" stay, as "Back keep editing" does?
   - The discard prompt now also shows its plain "OK choose", because the flag is per list.
2. **D2. French "Tenir OK".** Accept it, or ask a French speaker. "Maintenir OK" doesn't fit 280 px and "Appui long
   OK" is as long.

## Checks

**Ran:**

- **`fork-notes/tools/cc.sh --except profile ../rf-base-s4 ../rf-qa7-work`:** CC OK (`cc.log`).
  - uiSim builds with 0 warnings.
  - 1363 frames compared, 0 differ, 840 left out.
  - uiThemeTest and uiThemeTestPanels pass; uiPrototypeTest OK.
  - GD-88: 0 warnings, flash 793344 B, RAM 205872 B. Stack: ui deepest 1664, 204 free (unchanged).
- **Full host suite in Docker (`make -k -j16`):** rc 0 (`hostsuite.log`).
  - The only warning is the known `pppTest.c:363` one.
  - The new `uiSimFr` check ran.
  - I ran `make -C tests/host clean` before and after.
- **GD-88 builds:**
  - The default (`build/t-qa7-gd88`): 0 warnings, 793340 B flash, 205872 B RAM (`gd88.log`).
  - `CONFIG_UI_THEME_PANELS=n` (`build/t-qa7-np`): 0 warnings, 774172 B, 205784 B (`gd88np.log`).
  - Both trees are removed.
- **Checkers (`checkers.log`):**
  - `tools/i18n.py --check`: 1169 strings, 4 catalogs ok.
  - `check_generic.py`, `check_doc_links.py` (220 files, 0 broken) and `check_keys.py` (20 keys): ok.
- **Instrumented hint row crawl:** 4200 frames, with a print of the row's x, width and text in a scratch build
  (`crawl/crawl.json`, `tools/hintcrawl.py`).
  - Profile prefixes go by row position, so they work in every language.
  - About 22 gallery scripts per language use English `row:` names and stop early outside English.
- **Old-versus-new frames:** 4400 frames (`6223fc87` against `a8d7352b`, en, es, fr, it, zh, four variants)
  (`oldnew.log`, `oldnew/`).
  - 1300 differ only in the hint row: the flagged lists and the French save hints.
  - 112 differ elsewhere: Quick tile 8, the es and fr lock bars, and fr Quick's hint.
- **Pixel maps of the x 6 rows and the forced 296 px list hint:** (`x6_*.png`).
- **Mutations:** 25 plus a baseline, in a detached worktree (`tools/worktree.sh add ../rf-qa7-mut`), against
  uiPrototypeTest, uiThemeTestPanels (built without ASan on the Mac), uiFlowTest and the uiSimFr check
  (`mutations.log`, `tools/mutate.py`).
- **Renders I looked at:** Quick in five languages, the lock bar and locked Home in es and fr (Day and Night), the
  flagged lists' rows in five languages, the unflagged lists (In zones, Hardware, Factory reset, Delete zone, Programming
  Channels) and text entry in four variants.

**Not run:**

- The full `tools/ci.sh` matrix, HIL and on-air checks. No radio touched.
- DM-1701, MD-9600 and the other boards' builds. The round's shared change is in `core/ui`; cc.sh's reference frames
  cover the other display classes.
- uiThemeTest under ASan on the Mac (it ran under ASan in Docker, in cc.sh and the host suite).
- A random walk.
- French at 160x128 and 160x120 (deferred, known).

**Clean-up:** the worktree `../rf-qa7-mut` is removed, the `build/t-qa7-*` trees are removed and `tests/host` is
cleaned. The main checkout is untouched: `git status` is clean at `a8d7352b`.
