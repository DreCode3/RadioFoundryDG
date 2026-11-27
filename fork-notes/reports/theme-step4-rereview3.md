<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Step 4 (panels layout) QA re-review 3: per-key hint row

I reviewed the range `a8d7352b..6c121484` on `feat/gd88-gui` at `6c121484`. It has four commits: `bb6adc36`,
`691ce09e`, `730666cc` and `6c121484`. I read the diff, the screens and the renders myself, and didn't read any
implementer report. The evidence is in this folder (`rf-qa8-out/`).

**Verdict: NOT APPROVED.** There is one blocking finding, B1. In every translated build, the "In zones" hint row shows a
plain English "Back", which is Back's usual job. Spanish is in the GD-88's default build. The defect is older than this
round, but it breaks the per-key rule this round delivers, on the screen it fixed. The fix is small. Everything else
the round had to close is closed, tested and sound: B1, N1, N2 (with leftovers logged), N3 and the single
`UI_THEME_SELECT` default.

## What the round had to close

| Item | State | Evidence |
|---|---|---|
| Previous B1: the "In zones" flag | Done. `chz_screen` has `SF_LIST \| SF_OK_ACTS`, and "OK in/out" shows in English. **But the row still shows a plain "Back" in every translation (new B1)** | `renders/sheet_chzones.png`, `renders/sheet_langs.png`; mutation Nd caught |
| N1: the Hardware options flag | Done. `hwopt_screen` has `SF_OK_ACTS`, and the row reads "OK next  Hold OK store". MD-9600 builds it with 0 warnings | `renders/sheet_misc.png`, `md9600.log`; Ne caught |
| N2: French "enregistrer", the discard prompt in whole words, "Sauver d'ab: maint OK" | Done. No "Sauver", "sauvé" or "sauv." is left. The prompt reads "Modifications non enregistrées" / "Abandonner les modifications de ce canal ?", and the toast is "Enregistrer d'abord : tenir OK", which fits one line. Two clipped "not saved" errors remain (N1 below) | `fr.po`, `renders/sheet_fr_savefirst.png`, `renders/sheet_fr_toasts.png` |
| N3: tests for the mutations that survived | Done. Mg, Mq, Mr, Mu, Mv, Mw, Mx, My, Mn and Mo are all caught now | `mutations.log` |
| One `UI_THEME_SELECT` default | Done. The default is defined only in `core/config.h`. `ui_theme.h` and `ui_svc.h` point to it, and `settings.c/.h` test `#if UI_THEME_SELECT`, its caller's guard. Every file that tests it includes `config.h`: `-Wundef` gives no warning for it in `ui_app.c`, `ui_screens.c`, `ui_menus.c`, `ui_svc_storage.c`, `ui_svc_stub.c` or `settings.c` | `grep`; `-Wundef` syntax runs |
| The per-key rule's expected results | Met. The discard prompt shows "Back keep editing" and hides "OK choose". Programming > Channels hides "OK details" and keeps "# go". The value editor keeps "OK save  Back cancel". Confirmations hide "OK choose  Back cancel" | `renders/sheet_discard.png`, `sheet_channels.png`, `sheet_misc.png` |

## Blocking

### B1. "In zones": a plain "Back" shows in the hint row in every translated build

- **Where:** `core/ui/ui_menus.c:4467`, `chz_screen`, with `.hint = TRX(ZONE_HINT_IN_ZONES, "OK in/out  Back")`, and
  `panels_hint_parts` / `panels_group_back` in `core/ui/themes/panels/panels.c:756`.
- **What happens:** `TRX` text is never translated, so the hint's Back group stays the English "Back".
  `panels_group_back` recognizes a Back group by the key name in use, `TR(HINT_BACK)`, which is "Atrás", "Indietro",
  "Retour" or "返回". So in a translated build the theme doesn't see "Back" as the Back key. Because it isn't Back's
  usual job by that test, it draws "Back" in the row.
  - English: the row reads "OK in/out".
  - es, it, fr and zh: the row reads "OK in/out  Back", under a bar that says "Atrás", "Indietro", "Retour" or "返回"
    (`renders/sheet_langs.png`, `renders/sheet_chzones.png`).
- **Why it blocks:**
  - The rule hides "Back's usual job" and goes "by key, never by text" (ui.md). The panels.c comment also says "the bar
    and the row are the same in every language". This screen breaks both outside English.
  - `CONFIG_LANG_ES` is on by default, so the GD-88 ships it.
  - The new test can't see it. It runs only in English, on the previous review's advice that "the English check
    holds in every language", which turns out to be wrong for the Back group.
- **It is older than this round.** At `a8d7352b` the Spanish row read "Back" alone (`renders/es_chzones_old.png`).
  `uiSim` built with `UI_LANG=es`, with `expect:hides=Back`, fails both before and after this round. The flag makes it
  more visible, but this round didn't cause it.
- **Scope:** this is the only `TRX` hint with a Back group. `HW_HINT` ("OK next  Hold OK store") has none, and every
  other hint with "Back" is `TRN` or `TR`.
- **Fix options (the implementer's choice):**
  - Make `ZONE_HINT_IN_ZONES` a translated `TRN` key. Its key words then follow the language, as every other hint
    does. That needs four catalog entries.
  - Or have the theme also recognize an untranslated hint's English key name. This needs care: it must not turn into
    comparing translated text.
  - Either way, add a check outside English, for example `uiSimFr` (or a `UI_LANG=es` run) with
    `ok 8 9 ok ok up up up up ok expect:screen=chzones expect:hides=Back`.

## Not blocking: defects, latent risks and test gaps (to log)

### N1. French: two "not saved" errors are still clipped, and the round left them out

The round rewrote the store errors next to them (`STORE_ERR_ZONE_FULL`, `_RESET_SETTINGS`, `_KEYS_NOT_SAVED`). These
two are still clipped:

- `STORE_ERR_CODEPLUG_WRITE`, "Couldn't write the codeplug: not saved": the French is "Erreur écrit codeplug".
  "écrit" is the wrong word, and "not saved" is dropped.
- `STORE_ERR_CHANNEL_READ`, "Couldn't read the channel: not saved": the French is "Erreur lect. canal". It is clipped,
  and "not saved" is dropped.

Natural forms: "Écriture du codeplug impossible : non enregistré" and "Lecture du canal impossible : non enregistré".
French isn't in the default GD-88 build.

### N2. The discard prompt uses two verbs for "discard"

- The body says "**Abandonner** les modifications de ce canal ?", which is new this round.
- The row says "Oui, **ignorer**" (`DISCARD_ROW_DISCARD`, unchanged).
- The English uses one verb ("Discard ... ?" / "Yes, discard"). "Oui, abandonner" fits.

## Nits

1. **Discard title font.** The French title "Modifications non enregistrées" is too wide for the 12x24 title, so it
   uses 8x16. That is the designed fallback and within the 8x16 rule, but it's the only title on this screen in the
   small font (`renders/fr_discard_title_zoom.png`). A whole-word shorter form, such as "Non enregistré", would keep
   12x24. Keep it, or ask a French speaker.
2. **French wording to polish:**
   - "Enregistrement occupé : réessayez" (`CHAN_TOAST_SAVE_BUSY`) reads as translated. "Enregistrement en cours :
     réessayez", already used for `TOAST_BUSY_SAVING`, says the same thing naturally.
   - "Temporaire : enregistrez-le dans qdmr" is a bare adjective. "Pour l'instant seulement : enregistrez-le dans qdmr"
     matches "pour l'instant seulement", which this round uses in `STORE_ERR_KEYS_NOT_SAVED`.
   - Untouched, but in the same family: "Enregistré sur canal" has no article, while the row now says "Enregistrer sur
     le canal". "VFO auto-enregistré" and "AES : attente" drop meaning.
3. **Value editor wording.** `HINT_OK_SAVE_BACK_CANCEL` ("OK valider") and `HINT_TYPE_OK_EDIT_SAVE_BACK`
   ("modifier/valider") use "valider" for save. "Valider" (confirm) is natural for applying a typed value, so I count
   it as correct. I note it only because the round's aim was "enregistrer everywhere".
4. **gui-plan.md doesn't record the user's per-key answers.** Layout, "Decided Nov 26", still reads "hides only a
   list's plain open and choose". It doesn't say that "OK details" and a confirmation's "Back cancel" are hidden, or
   that the rule goes key by key. ui.md has it (`6c121484`).
5. **Line length.** `ui_nav.h` now has a 121-character comment line, its only line over 120. `panels.c:738` and
   `uiThemeTest.c:1012` are also over 120 once tabs are expanded.
6. **The test's screen check.** `test_list_hint_row_keeps_what_ok_does_where_it_does_more` skips `expect:screen` for
   names with "_" (`dual_pick`, `zone_share_to`). The comment explains why. Their row checks still hold the flag
   (Mu and Mx caught).

## The flags: full sweep

There are 54 `ui_scr_def_t` with `SF_LIST` in `core/ui`, and three `hint_fn` builders. For each one I checked what
`select` does, not only the hint text.

**Flagged `SF_OK_ACTS`, and correctly:**

| Screen | Hint |
|---|---|
| compose | "OK send" |
| contacts | "OK talk" |
| zones | "OK go" |
| zone | "OK go" |
| zone_share_to | "OK send" |
| zone_offer_zone | "OK add" |
| zpick | "OK add" |
| chzones | "OK in/out" (new) |
| picker | "OK use  Hold OK set" |
| dual_pick | "OK watch" |
| channel, contactedit | "OK change  hold OK save" |
| fixed-pos | "OK change" |
| dmr-id, data-services | "OK edit/save" |
| sig-chain | "OK next" |
| hwopt | "OK next" (new) |
| zone_offer_preview | "OK or Back returns": one OK group, a sentence |

**Flagged `SF_BACK_ACTS`, and correctly:** discard ("Back keep editing"). Its OK is a plain choose, now hidden.

**Not flagged, and correct under the per-key rule:**

- **Plain open:** channels ("OK details", now unflagged as decided), find, settings ("OK open"), aprs-heard and the
  other open rows.
- **Plain choose and pick:** message, aprs-station, pick, language, zone_share, zone_offer, dual_zone, chpick,
  crash_to and crash. `crash` is "OK pick", and its rows name their action. I count it with message actions.
- **Confirmations, "OK choose  Back cancel":** confirm, zonedel and codeplug-foreign. Their Back is the usual leave,
  as decided.
- **No OK group:** numbered lists ("1-n pick"), recent, info and satellite-passes.

**The `hint_fn` builders:**

- **`group_hint`:** `ok_acts` is true for change, the number editor and "Save GPS position", and false for pick lists
  and open rows.
  - The number editor's "OK save  Back cancel" keeps both keys through `editing`, which sets both bar flags in
    `ui_draw_list`.
  - Mutations Ms, Mw and Ni catch the three branches.
- **`msgs_hint`:** "OK open" and "OK compose" are a list's usual job. `ok_acts` is left false (Mt caught).
- **`menu_hint`:** "1-n open  Back", with no OK group (`(void)ok_acts`).

No Back group in any `SF_LIST` hint does more than leave, except discard's and the editor's. Nothing is flagged
wrongly.

## `flags` widths and bits

- **`ui_scr_def_t.flags` is now `uint16_t`.** It fills the byte that was padding between `flags` and the
  2-byte-aligned `prefix`.
  - MD-9600 (BOARD_HWOPTS, panels off), `a8d7352b` against `6c121484`: every screen object is still 0x44 bytes at the
    same address (`hub_screen`, `chz_screen`, `discard_screen`, `hwopt_screen`).
  - Flash and RAM are byte-identical, 688432 B and 121408 B (`md9600.log`, `md9600-old.log`).
  - Truncating `flags` back to `uint8_t` gives 6 `-Woverflow` warnings in `uiSim`, so the zero-warnings rule guards
    the type (Nq).
- **`ui_list_t.back_acts`** (panels only) takes the padding byte after `kind`, `numbered` and `ok_acts`. The size is
  unchanged.
- **Bits:**
  - Shared: `UI_BAR_TOAST` 0x01, `LIST` 0x02, `NUMBERED` 0x04, `OK_ACTS` 0x08, `NO_HINTS` 0x10 and `BACK_ACTS` 0x20.
  - The theme's own: `PANELS_BAR_MENU` 0x100, `KEYS` 0x200 and `IDLE` 0x400.
  - They don't overlap, and 0x40 and 0x80 are free. Every holder of these flags is now `unsigned`: `bottom`,
    `reference_bottom`, `draw_hint`, `panels_bottom`, `panels_hint_parts`, `panels_hint_has_row`, `panels_home_bar`,
    `ui_draw_list`'s `bar` and the test's probe.
  - Nothing narrows them again. Np (theme bits back at 0x20), No (`BACK_ACTS` on the `NO_HINTS` bit) and Nr
    (`panels_bottom` narrowing to `uint8_t`) are all caught.
- **GD-88:**
  - The default build is 793420 B flash and 205872 B RAM, against 793340 B and 205872 B at `a8d7352b`. That is +80 B
    of flash for the per-key test and the extra flag, and the same RAM.
  - The ui task's stack is the same: deepest 1664, 204 free.
  - With panels off it is 774180 B and 205784 B, against 774172 B and 205784 B: +8 B, from the 16-bit flag loads.
- **The reference theme and the other display classes are unchanged.** cc.sh compared 1363 frames, and none differ.
  The reference theme reads only `UI_BAR_TOAST`.
- **Panels renders, old against new:** I compared the `a8d7352b` snapshot (`rf-qa7-work/cur`) with this one
  (`rf-qa8-work/cur`), 2203 frames in all.
  - 12 frames differ: `channel_discard`, `channel_zones` and `channels` in each of the four profile variants.
  - Those are exactly the three screens this round changed.

## Mutations

There were 31 mutations plus a baseline, in two detached worktrees made with `tools/worktree.sh add` (both removed).
Each one was checked against uiPrototypeTest, uiThemeTestPanels (no ASan, on the Mac), uiFlowTest, the uiSimFr check
and the build's warning count. **All 31 are caught** (`mutations.log`).

**This round's code:**

| Id | Mutation | Caught by |
|---|---|---|
| Na | discard without `SF_BACK_ACTS` | proto |
| Nb | discard flagged OK, as before | proto |
| Nc | discard flagged both | proto |
| Nd | chzones unflagged | proto |
| Ne | hwopt unflagged | proto |
| Nf | channels flagged again | proto |
| Ng | `list_draw` sets `back_acts` false | proto |
| Nh | `ui_draw_list` drops `back_acts` | proto |
| Ni | the editor no longer sets `BACK_ACTS` | proto |
| Nj | `back_acts` also sets `OK_ACTS` | proto |
| Nk | per list again (either flag keeps both keys) | proto, themeP |
| Nl | the theme ignores `BACK_ACTS` | proto, themeP |
| Nm | the theme ignores `OK_ACTS` | proto, themeP |
| Nn | the keys' flags swapped | proto, themeP |
| No | `BACK_ACTS` on the `NO_HINTS` bit | proto, themeP |
| Np | the theme's bits back at 0x20 | proto, themeP |
| Nq | `flags` back to `uint8_t` | warn, proto |
| Nr | `panels_bottom` narrows to `uint8_t` | proto, themeP |

**The previous review's survivors (N3):** Mg, Mq, Mr, Mu, Mv, Mw, Mx and My are caught by proto. Mn and Mo (the 298 px
room and the x 6 start) are caught by themeP's new wide-row check.

**Also re-checked:** Ms and Mt are caught.

## Translations this round touched

French only: 28 strings in `fr.po`. es, it and zh are untouched. `tools/i18n.py --check` passes.

- **Meaning is kept** in all 28. The only loss is "Temporaire" (nit 2).
- **Clipped forms:** none are left in what the round touched. "Erreur sauv.", "Sauv. canal indispo.", "pr sauv",
  "Mnt:" and "d'ab: maint" are gone. The two leftovers are in N1.
- **Fit on the GD-88, in all four variants:**
  - The "another number" toast goes to two lines and fits (`renders/sheet_fr_toasts.png`).
  - "Enregistrer d'abord : tenir OK" fits on one line (`renders/sheet_fr_savefirst.png`).
  - The row "Enregistrer en nouveau canal" fits (`renders/sheet_fr_toasts.png`).
  - The discard title falls back to 8x16 (nit 1).
  - The hint "OK modifier  tenir OK enregistrer" fits (`renders/sheet_chdetails.png`).
- **French spacing:** the space before ":" and "?" is used throughout the new strings.

## Checks

**Ran:**

- **`fork-notes/tools/cc.sh --except profile ../rf-base-s4 ../rf-qa8-work`:** CC OK (`cc.log`, `cc-gd88.log`).
  - uiSim: 0 warnings.
  - 1363 frames compared, 0 differ, 840 left out.
  - uiThemeTest and uiThemeTestPanels pass (ASan, Docker). uiPrototypeTest OK.
  - GD-88 (`build/t-gd88`): 0 warnings, 793420 B flash, 205872 B RAM, and every task's stack fits.
- **Full host suite in Docker, `make -k -C tests/host -j16`:** rc 0 (`hostsuite.log`).
  - The only warnings are the known `pppTest.c:363` and the jobserver note, the same as the last review.
  - The `uiSimFr` check ran.
  - I ran `make -C tests/host clean` before and after.
- **GD-88 with `CONFIG_UI_THEME_PANELS=n` (`build/t-np`):** 0 warnings, 774180 B flash, 205784 B RAM (`gd88np.log`).
- **MD-9600 (`build/t-md`):** 0 warnings, 688432 B flash, 121408 B RAM, `hwopt_screen` built (`md9600.log`). I also
  built `a8d7352b` in the worktree to compare sizes (`md9600-old.log`).
- **Checkers (`checkers.log`):**
  - `tools/i18n.py --check`: 1169 strings, 4 catalogs ok.
  - `check_generic.py`: ok.
  - `check_doc_links.py`: 220 files, 0 broken.
  - `check_keys.py`: 20 keys agree.
- **The sweep:** all 54 `SF_LIST` screens and the three `hint_fn` builders, against their `select` functions.
- **Renders** (`renders/`, `sims/` holds the simulators):
  - discard, Programming > Channels, In zones and channel details in en and fr, in all four variants.
  - In zones, discard and Channels in es, it and zh.
  - Hardware in en and fr, the value editor, and Factory reset in en and es.
  - French toasts and rows in four variants.
  - Spanish In zones at `a8d7352b`.
- **The panels snapshot compared old against new:** 2203 frames.
- **`-Wundef`** on the files that test `UI_THEME_SELECT`.
- **Mutations:** 31 plus a baseline.

**Not run:**

- The full `tools/ci.sh` matrix, HIL and on-air checks. No radio was touched.
- DM-1701 and the other boards' builds. cc.sh's reference frames cover the other display classes.
- A language crawl of every screen, old against new. I spot-checked es, it, zh and fr on the changed screens.
- French at 160x128 and 160x120 (deferred, known).

**Clean-up:**

- The worktrees `../rf-qa8-mut` and `../rf-qa8-mut2` are removed. One was stopped in the middle of a mutation, and I
  reverted it before removing it.
- `build/t-gd88`, `build/t-np` and `build/t-md` are removed, and `tests/host` is cleaned.
- The main checkout is untouched: `git status` is clean at `6c121484`.
