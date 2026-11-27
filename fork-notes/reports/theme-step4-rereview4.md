<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Step 4 (panels layout) QA re-review 4: the In zones hint, French wording, line wraps

I reviewed `6c121484..7ab0bf3f` on `feat/gd88-gui`, checked out at `7ab0bf3f`. The range has three commits:

- `1359045a`: translate the In zones hint.
- `a0015e0e`: two French save errors and the discard answer.
- `7ab0bf3f`: wrap long lines.

I read the diffs, the code paths and the renders myself, and didn't read any implementer report. The evidence is in this
folder (`rf-qa9-out/`).

**Verdict: APPROVED.** Nothing blocks.

- B1, N1 and N2 are closed, and the wrap commit changes no code.
- The new French check fails without the fix, and every mutation I tried is caught.
- The non-blocking items below are worth logging. The most useful one is N1: on 160 px displays, the French save
  errors are shortened so that "non" is lost. That class of display is deferred to the port step, and French is off by
  default on every board.

## What the round had to close

| Item | State | Evidence |
|---|---|---|
| B1: the In zones hint was `TRX`, so es, fr, it and zh showed a plain "Back" | **Closed.** It is now `TRN(ZONE_HINT_IN_ZONES, ...)`, translated in all four catalogs. Each catalog ends it with its own `HINT_BACK` word: "Atrás", "Retour", "Indietro", "返回". On the GD-88 the row now reads "OK añadir/quitar", "OK ajouter/retirer", "OK aggiungi/togli" and "OK 加入/移出", with no Back group (the bar names Back) | `renders/sheet_chzones_langs.png`, `zh_chzones_zoom.png` |
| A non-English test that fails without the fix | **Done.** run-ui has a `uiSimFr` line with `expect:hides=Back expect:hides=retirer__Retour expect:shows=OK_ajouter/retirer`. It fails with `TRX` restored (mutation A). uiPrototypeTest's `test_hint_structure_survives_translation` now covers the key in every catalog (B, C, D, I, J) | `mutations.log` |
| No other untranslated hint with a key group the theme parses | **Confirmed.** See the sweep below. The only other `TRX` hint is `HW_HINT` ("OK next  Hold OK store"). Its only key group is OK, which the theme finds by the literal "OK" that no catalog translates, so it acts the same in every language. It has no Back group | `grep` |
| N1: French `STORE_ERR_CODEPLUG_WRITE` and `STORE_ERR_CHANNEL_READ` in whole words, keeping "not saved" | **Closed on the GD-88.** "Écriture du codeplug impossible : non enregistré" and "Lecture du canal impossible : non enregistré". Both are natural French and keep the meaning. They wrap to two lines without overflow in the panels theme and in the reference theme at 320x240, 220x176 and 240x320. On 160 px displays they don't fit (new N1) | `renders/toasts/sheet_fr_w.png`, `sheet_fr_r.png` |
| N2: French discard answer "Oui, abandonner" | **Closed.** It matches the body's verb and fits at 320x240iR10 and 160x128 | `renders/fr_discard_sheet.png` |
| Lines over 120 columns wrapped; the wrap commit changes no code | **Done for the lines the last review named** (`ui_nav.h`, `panels.c:738`, `uiThemeTest.c:1012`). The objects are byte-identical before and after. A few long lines are left in the same files (nit 3) | `wrap-objects.log` |

## Blocking

None.

## Not blocking: defects, latent risks and test gaps (to log)

### N1. French save errors on 160 px displays lose "non" (reference theme, port step)

- **What:** the reference toast is two lines at most, and `gfx_text_fit` shortens the second line in the middle.
  On 160x128 and 128x64 the write error reads "Écriture du codeplug / impossible : …n enregistré". On 160x120i5 it
  reads "impossible :… enregistré". The read error does the same. "non enregistré" (not saved) is cut to "…n
  enregistré" or "… enregistré", which reads close to "saved".
- **Before this round** the clipped forms ("Erreur écrit codeplug") fit. The English fits on every class
  (`renders/toasts/small_zoom.png`).
- **Why it doesn't block:**
  - `CONFIG_LANG_FR` defaults to n on every board, and dm1701, mduv380 and md9600 leave it out of their maximal builds.
  - gui-plan.md defers French on 160 px displays to the port step.
  - The GD-88's two panels (320x240 panels and 220x176 reference) fit.
- **Cheap fix if wanted now:** "Échec d'écriture du codeplug : non enregistré" wraps as "Échec d'écriture du" /
  "codeplug : non enregistré", 25 characters, which fits 160 px. Otherwise add both keys to gui-plan's port-step list
  beside the cut French labels.

### N2. Wrapped French text can start a line with ":" or "?"

- **Where:** `ui_wrap_line` breaks at any space. French puts a space before ":" and "?", and the catalogs use a plain
  space (fr.po has no U+00A0 or U+202F).
- **On the GD-88:** the panels theme shows the write error as "Écriture du codeplug impossible" / ": non enregistré"
  (`renders/toasts/fr_w_320x240iR10.png`). The reference theme at 240x320 does the same with the read error.
- **Elsewhere:** at 160x128 the discard prompt ends "de ce canal" / "?" (`renders/fr_discard_sheet.png`). That one is
  older than this round.
- **Meaning:** kept. It is typography only.
- **Fix:** a generic one in `ui_wrap_line`: don't break at a space that comes before ':', ';', '?' or '!'. That helps
  every French string. Rewording only moves the problem.

### N3. The In zones title is still untranslated

- **Where:** `chz_screen.prefix = TRX(ZONE_TITLE_IN_ZONES, "In zones")`. It came from upstream as `TRX` (`8cd58067`).
- **Effect:** every language shows the English title "In zones" above the translated hint and values
  (`renders/sheet_chzones_langs.png`).
- **Scope:** it's a title, not a hint, so the theme doesn't parse it and the per-key rule isn't affected. It is the
  only word-like `TRX` text on a user screen apart from the Hardware screens (`HW_TITLE`, `HW_HINT`, MD-9600 only,
  English throughout by design) and "Backlight tests" (a diagnostics action).
- **Fix:** `TRN` and four catalog entries, the same fix B1 had.

## Nits

1. **Italian at 160x120 (DM-1701).** "OK aggiungi/togli  Indietro" is shortened in the middle to "OK aggiu…i/togli".
   It fits at 160x128 and above. Italian is off by default, and small classes are deferred.
   - "OK metti/togli" would fit.
   - Elsewhere it.po says "Rimuovi" for remove ("Rimuovi dalla zona"). "aggiungi/rimuovi" would match but is
     longer. "togli" is natural, so either way works.
2. **The zh hint row sits on the bar's top edge.** It's the same on every zh hint row (Channels, channel details), so
   it's older than this round and not specific to In zones (`renders/zh_rows_zoom.png`).
3. **The wrap commit claims more than it does.** The message says lines "this branch added over 120 columns are
   wrapped". The three lines the last review named are wrapped, but some are left in the same files:
   - `panels.c:814` is 125 columns with 4-column tabs. It came from `22a46540`, in this step.
   - `uiThemeTest.c:6` and `:929` are 121 characters.
   - `uiThemeTest.c:485` and `:1129` are over 120 with tabs expanded.
   - `ui_dual.c:553` is 121 characters.
   - Branch-wide there are about 200 more (`ui_app.c:628`, `ui_menus.c:7163`, `ui_svc.h:53` and others), mostly from
     the menu-tree commits.

   No checker enforces 120, so this is cosmetic.
4. **Flash.** The default GD-88 build is 793440 B, up from 793420 B at `6c121484` (cc.sh's `build/t-gd88` gave
   793444 B). That is about +20 B for the Spanish string and its index. RAM is the same, 205872 B, and the ui stack is
   unchanged (deepest 1664, 204 free).

## Sweep: untranslated hints

- **`TRX` hints:** `grep` finds two in `core/`: `ZONE_HINT_IN_ZONES` (now `TRN`) and `HW_HINT`.
- **Literal hint text** outside `TR`/`TRN`/`TRX`: none. Every `"...  Back"` or `"OK ...  "` literal is a
  `TEXT_ALT_NEXT` line inside a `TRN` or `TR` (dual_pick, contacts, zones, channels, channel and contact edit, discard,
  add channel, picker, Home's talk-target hints).
- **The `hint_fn` builders:**
  - `msgs_hint` and `group_hint` return `TR` keys only.
  - `menu_hint` builds "1-n" from digits (no key group the theme matches by text) plus `TR(MENU_HINT_OPEN_BACK)`.
- **No literal `ui_bottom` or `.hint = "..."` calls.**
- **The theme's matching:** `panels_group_ok` tests the literal "OK", which every catalog keeps. `panels_group_back`
  tests `TR(HINT_BACK)`. Only a hint whose Back group isn't in the catalog's language could break the rule, and none
  is left.

## The wrap commit changes no behavior

- **Objects:** I built `panels.c`, `ui_dual.c`, `ui_menus.c`, `ui_app.c` and `ui_screens.c` at `a0015e0e` and at
  `7ab0bf3f`, with `-O2 -g0`, in three configurations: uiSim's (panels, English), French with panels, and panels off.
  All 15 objects are byte-identical (`wrap-objects.log`). `ui_nav.h` is comments only. Nothing in these files uses
  `__LINE__`.
- **Tests:** the Python and `uiThemeTest.c` changes only re-break lines. uiPrototypeTest and uiThemeTest pass at
  `7ab0bf3f`.
- **Renders:** the panels snapshot at `7ab0bf3f` (`rf-qa9-work/cur`) against `6c121484` (`rf-qa8-work/cur`): 2203
  frames compared, 0 differ (`snapshot-vs-qa8.log`). That is expected, because the round changed only translations.

## Mutations

I ran 10 mutations and a baseline in a detached worktree made with `tools/worktree.sh add` (removed). Each one was
checked against the build's warnings, the new `uiSimFr` line, uiPrototypeTest and `tools/i18n.py --check`. **All 10 are
caught** (`mutations.log`).

| Id | Mutation | Caught by |
|---|---|---|
| A | the hint back to `TRX` (the fix reverted) | uiSimFr line ("OK in/out  Back" shown), i18n |
| B | fr Back word "Arrière" | proto (hint structure, fr.po) |
| C | es Back word "Volver" | proto (es.po) |
| D | zh groups joined by one space | proto (zh.po) |
| E | fr entry deleted (falls back to English) | uiSimFr line, i18n |
| F | fr OK group left in English ("OK in/out  Retour") | uiSimFr line |
| G | chzones without `SF_OK_ACTS` | uiSimFr line, proto |
| H | the theme matches the English "Back" instead of `HINT_BACK` | uiSimFr line |
| I | it Back group dropped | proto (it.po) |
| J | fr Back group dropped | proto (fr.po) |

The baseline passes everything.

## Checks

**Ran:**

- **`fork-notes/tools/cc.sh --except profile ../rf-base-s4 ../rf-qa9-work`:** CC OK (`cc.log`).
  - uiSim: 0 warnings.
  - 1363 frames compared, 0 differ, 840 left out (profile).
  - uiThemeTest and uiThemeTestPanels pass (Docker). uiPrototypeTest OK.
  - GD-88 `build/t-gd88`: 0 warnings, 793444 B flash, 205872 B RAM, and every task's stack fits.
- **Full host suite in Docker:** `make -C tests/host clean`, then `make -k -C tests/host -j16`: rc 0
  (`hostsuite.log`).
  - The only warnings are the known `pppTest.c:363` and the jobserver note.
  - The new `uiSimFr` In zones line ran and passed.
  - I cleaned again afterwards.
- **GD-88, `DOCKER_DEFAULT_PLATFORM=linux/amd64 tools/docker/csky-toolchain/run.sh make -j16 BOARD=gd88`:** rc 0,
  0 warnings, 793440 B flash (88.45%), 205872 B RAM (95.06%) (`gd88.log`). This writes the default `build/gd88`, which
  already existed, and I left it.
- **Checkers (`checkers.log`):**
  - `tools/i18n.py --check`: 1170 strings, 4 catalogs ok.
  - `tools/check_keys.py`: 20 keys agree.
  - `tools/check_generic.py`: ok.
  - Also `check_doc_links.py` (0 broken) and `i18n_changes.py 6c121484` (0 changes).
- **Renders (`renders/`, simulators in `sims/`):**
  - In zones in en, es, fr, it and zh at 320x240iR10 (the given script).
  - The In zones hint row in the reference theme at 320x240, 220x176, 160x128 and 160x120i5 in es, fr, it and zh.
  - Other zh rows for comparison.
  - The French discard prompt at 320x240iR10 and 160x128.
- **French store-error toasts on all 7 classes**, in French and English on the small ones. The simulator can't raise
  these errors: `ui_svc_stub` never fails, and the only `UI_EV_SAVE_FAILED` it sends is the zone editor's own text.
  So I built a throwaway French uiSim whose `ui_toast` swaps in either key, and triggered it with `lockkey`. I also
  read both toast paths: `reference_bottom` and `panels_bottom` show one line or two (the second shortened with
  `gfx_text_fit`), with 64-byte buffers. The longest string is 50 bytes.
- **The untranslated-hint sweep** described above.
- **The wrap commit:** objects and the snapshot compared.
- **Mutations:** 10 plus a baseline.

**Not run:**

- The full `tools/ci.sh` matrix, HIL and on-air checks. No radio was touched.
- Other boards' firmware builds (MD-9600, DM-1701). The round changed only catalogs, one macro and comments, and
  cc.sh's reference frames are unchanged.
- A crawl of every screen in every language.

**Clean-up:**

- The worktree `../rf-qa9-mut` is removed.
- No `build/t-*` trees are left (cc.sh removed `build/t-gd88`).
- `tests/host` is cleaned.
- The main checkout is untouched: `git status` is clean at `7ab0bf3f`.

**APPROVED**
