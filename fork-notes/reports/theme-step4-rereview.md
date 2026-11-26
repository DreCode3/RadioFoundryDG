# Step 4 (panels layout) QA re-review: the fix round

Range `0e944c5e..ae4b5581` on `feat/gd88-gui`, reviewed at `ae4b5581`. `0e944c5e` has the same code as the first
review's `4659ca37`; it only adds `fork-notes/`. I read the code and the screens myself and did not read any
implementer report. The evidence files are in this folder.

**Verdict: APPROVED.** No blocking findings.

- The user's seven answers are built.
- The first review's two blocking findings (B1, B2) are fixed and tested.
- Each logged defect and test gap is fixed, except the French small-display hints. Those are deferred to the port step
  (known).

What remains:

- A one-line translation defect.
- Four mutations that no test catches.
- Some nits.
- Three design questions for the user.

## The user's answers (gui-plan.md, "Decided Nov 26, after step 4's second attempt")

| Answer | Code | Renders |
|---|---|---|
| Home's strip without the mode word | `panels_draw_status(..., home)`: `t.mode = home ? NULL : ...`, width 0 | No mode in Home's strip in all four variants and in es, fr, it, zh; lists keep it (`commercial_day_on_off.png`, `amateur_night_variants.png`, `lang_*.png`) |
| Hint row off hides only the hint line | `UI_THEME_OPT_SOFT_KEYS`, `UI_BAR_NO_HINTS` in `ui_draw_bottom_key`, `ui_draw_list`, `list_bottom`, Home's footer and Quick | Bar and both labels stay on Home, the reply window, scanning, the lock, lists, the value editor, Quick, the GPS page and Radio info. Lists keep 5 rows (`hint_row_off.png`). Exception: design question 2 |
| The hint row shows only what the bar can't | `panels_hint_parts`: a list's OK and Back groups are "usual" unless `UI_BAR_EDIT` | "OK save  Back cancel" in the editor; "Back stop scan", "Back end reply", "Back stop", "Digital monitor  Back off" on Home; no "OK open" or "OK choose" in lists. See design question 1 |
| Clock rank, and Home's mode bar as the fallback | Enum order: CLOCK after UNREAD. The clock is filled before the mode-font loop. `panels_draw_status` returns whether the clock had no room; `panels_home_frame` draws it at the mode bar's right | `gps_clock.png`: the clock is in the mode bar in Home's crowd (Commercial, Amateur, D-STAR) and in the strip with room. The step 2 crowd (render 35: monitor, unread, lock, dual watch, D-STAR, ARC4) shows "21 Nov 14:08" in the strip again |
| The locked bar sentence | `panels_bar_sentence`, Hint row on or off; the pane ends at the bar | "Locked: hold * to unlock" in 12x24, Day and Night, both layouts. French is defective (D1) |
| No zone name in the Zones tile | `panels_tile_text` blanks the value by `it->icon == UI_MENU_ICON_ZONES` (structure, not text) | Zones tile empty; Scan "Off"/"Arrêt"/"No"/"关", Messages "1 new" kept |
| Right soft key stays Back | Unchanged (`ui_home_soft_key`) | "Back" on Home in every variant and language |

`ui_home_ok_menu()` (`!s->entry && !s->locked`) matches `ui_home_key` in every Home state I traced:

- **Where OK opens the menu:** the reply window, scanning, digital monitor, TX, a banner, auto-off, T3 alerting and
  dual watch. `ui_dual_home_key` doesn't take SELECT.
- **Where it doesn't:** OK tunes the typed frequency (entry), and `ui_app_key` returns before Home while locked.
- **Not drawn as Home:** the hotspot and diagnostics screens.
- **Programmed home keys:** no board lists the OK key as a home key (GD-88: `KEY_SOFT_RIGHT`; DM-32: Left and Right).
  So no assigned function can take OK away from the menu today.

## Blocking

None.

## Not blocking: defects, latent risks and test gaps (to log)

### D1. French unlock sentence is a clipped non-word, now in Home's large bar

- **Where:** `core/ui/lang/fr.po`, `LOCK_HINT_HOLD_STAR`, `"{Verrouillé: maintn *|Verrouillé: maintn *}"`, and
  `LOCK_HINT_HOLD_BACK`, `"{Verrou : mtn Retour|Verrou : mtn Retour}"`.
- **What is wrong:**
  - "maintn" and "mtn" aren't words. AGENTS.md asks for real words, and the round rewrote the other French "Maint."
    strings.
  - The sentence also drops "to unlock".
  - It predates this round (`27a8e5b6`), but this round made it the one large sentence on a locked Home
    (`locked_sentence_langs.png`).
- **How I confirmed it:** a French uiSim build with `UI_LANG=fr`, `lockkey wait:3000`, Hint row on and off.
- **Smallest fix:** `"{Verrouillé : maintenir * pour déverrouiller|Maintenir * pour déverrouiller}"`. The short form is
  30 characters, which fits the bar in 8x16. Do the same for the Back variant.
- **Spanish:** "Bloq.: mantener * para abrir" (clipped "Bloq.", and "abrir" means open) could get the same treatment.

### Test gaps: four mutations no suite catches

The suites I ran: uiPrototypeTest, uiThemeTestPanels, uiThemeTest, radioCoreTest and uiFlowTest. 33 mutations in all,
29 caught (`mutations.log`).

| Id | Mutation | Survived | What it leaves untested |
|---|---|---|---|
| M4 | `ui_draw_bottom_key`'s Hint-row-off branch disabled | proto, themeP, flow | The bar with Hint row off on screens drawn through `ui_draw_bottom`: GPS pages, text pages, volume, satellite, freq trim. The renders show it works (`hint_row_off.png`: GPS, Radio info). Test: `off + "ok 6 1 expect:shows=Back expect:layout=0"` |
| M16 | `shorter = NULL` (no "DMR monitor" before "DMon") | proto, themeP | The new Quick word. Test: `quick expect:shows=DMR_monitor` |
| M24 | Scan tile uses the rows' Off (`scan_menu_value()`) | proto, flow | `HUB_VALUE_OFF`: English can't tell the two apart. Test it in French (`Arrêt` vs `Désactivé`) or check the key |
| M27 | `panels_home_bottom` ignores `s->locked` | proto, themeP | The channel pane reaching the bar on a locked Home (Hint row on). It's a gap in the layout only. Test: compare the pane's bottom edge locked vs. unlocked |

Caught:

- The clock order (M1, M1b).
- B1's ordering (M2).
- The mode-bar fallback (M3).
- Hint row off in lists and on Home (M5-M7, M25, M26, M28).
- The list filter (M8).
- `UI_BAR_EDIT` (M9, M9b).
- The Menu label (M10, M29).
- The entry case of `ui_home_ok_menu` (M11, themeP only).
- The locked sentence and its font order (M12, M13).
- The Zones tile (M14).
- Home's mode word (M15).
- The defect tests: typed TX frequency M17, Quick at 6x12 M18 (themeP only), banner over dual watch M19b.
- The settings fixes (M20 bit mask, M21 default not saved, M22 boot call wiring via `setwrites`).
- The reference geometry's `across` (M23).

`panels_banner`'s `x0` (M19) still survives, as in the first review. The protection is `ui_overlay_top`, and M19b,
which disables it, fails `test_banner_over_dual_watch_cuts_no_line`.

## Settings

- **`settings_end_default`**
  - Now replaces `settings_end`.
  - It runs under the lock `settings_begin` took: `ui_svc_setting_default` calls begin, sets the bit, then
    end_default.
  - It copies by mask: `sv = (sv & ~changed) | (l & changed)`.
- **Races and lost bits.** I traced these cases:
  - Another task's pending bit in the same byte stays unsaved, so `dirty` stays true and it is saved
    (`test_setting_default` checks `updown_home` bit 0).
  - A pending change of the label bit itself (before == live) leaves `saved` alone, so it is still saved.
  - `changed_ms` is untouched, so an earlier pending change keeps its timer.
- **A write at boot after a failed read.** None:
  - radioCoreTest checks `st.saves == 0`, no flash units used and `!dirty` after a failed read.
  - The simulator's `setwrites=0` check holds `ui_app_init` to the default call (M22).
- **The simulator's own `ui_svc_setting_default`.** It mirrors the store's: a write that the counter doesn't count.
  That is enough for the wiring. The store's side is in radioCoreTest.
- **Guards.**
  - `settings_end_default`, `ui_svc_setting_default` and the stub are under `CONFIG_UI_THEME_PANELS`.
  - Their caller in `ui_app_init` is under `UI_THEME_SELECT`. It defaults to `CONFIG_UI_THEME_PANELS`, and only
    uiThemeTest forces it to 1, without `ui_app.c`. Equivalent today (nit N3).

## Shared code

- **Reference theme and other classes unchanged:** 1363 frames compared, 0 differ, 840 left out (`profile`) (`cc.log`).
  - `ui_theme_soft_keys()` is `false` without `UI_THEME_SELECT`, and the reference theme lacks the option, so
    `ui_theme_has(...)` is false for it.
  - uiThemeTest checks `!(ui_theme_reference.options & UI_THEME_OPT_SOFT_KEYS)`.
- **No theme test in shared code:** the shared code tests a declared option (`UI_THEME_OPT_SOFT_KEYS`), and the theme
  keeps its own flags. Nothing compares shown text:
  - The list filter goes by flags.
  - The Zones tile goes by icon id.
  - The locked state goes by `s->locked`.
- **`UI_BAR_EDIT`** is set from the focused row's `UI_ITEM_EDIT` while the list is drawn.

## Translations (es, fr, it, zh)

- **Changed strings read naturally and keep the meaning:**
  - es: "Potencia", "Escucha A/B", "Monitor DMR", "Llamadas".
  - fr: "Puissance", "Écoute A/B", "Écoute DMR", "Appels", "Maintenir OK ...", "détails", "arrêt du balayage",
    "0-9 fréquence TX", "cible par défaut".
  - it: "Ascolto A/B", "Monitor DMR", "Chiamate".
  - zh: "DMR 监听", "关".
- **Fit:** all fit their tiles and rows at 8x16 (`lang_*.png`).
- **fr "sauver":** the short forms use "sauver" where the long ones say "enregistrer" (`HINT_OK_CHANGE_HOLD_OK_SAVE`
  has both). "Sauver" is understood but less usual for saving settings (nit N5).
- **it "Opzioni" for Settings** (design question 3):
  - It fits the tile, where "Impostazioni" was cut to "Impostazio…".
  - But it means "Options", while Italian devices say "Impostazioni".
  - The Settings grid still has "Impostazioni chiamata" under the title "Opzioni".
  - The key is shared, so the title also changes on every Italian radio, reference theme included.
- **es and it "No" for the Scan tile's Off:** this matches those catalogs' rows, which already say "No".

## Nits

1. **N1.** `docs/spec/ui.md`: three edited lines run far past the file's wrap width (the strip item, the clock
   sentence, the Home item).
2. **N2.** `test_commercial_crowded_status_keeps_the_clock_and_device_icons` is back, but without step 2's `mon`, and it
   toggles the unread message instead of VOX and recording.
   - VOX and recording now drop in a crowd, so toggling them can't change the strip any more.
   - I ran step 2's exact setup and all three of its states at HEAD, and the clock shows (`gps_clock.png`).
   - Keeping step 2's setup verbatim, with a higher-ranked toggle, would hold more.
3. **N3.** The `ui_svc_setting_default` guard (`CONFIG_UI_THEME_PANELS`) is narrower than its caller's
   (`UI_THEME_SELECT`). They are the same in every real build. `#if UI_THEME_SELECT` on the declaration and the
   definition would match exactly.
4. **N4. Size with the panels switch off.**
   - GD-88 with `CONFIG_UI_THEME_PANELS=n` is 774148 B, against 774112 B in the first review: +36 B.
   - DM-1701 is 709496 B, about +68 B against the first review's figure (derived: its `e0c529a5` 709236 + 192).
   - Likely causes: `HUB_VALUE_OFF` and its ternary in the shared `hub_item`, the new keys in the catalogs, and longer
     translations.
   - The same change shows "Arrêt" in place of "Désactivé" on the reference theme's French Scan row.
5. **N5.** fr: "sauver" next to "enregistrer" (above), and "0 Paramètres" for "0 All settings".
6. **N6. es and it clipped forms, now in the hint row.** They predate this round, like the French ones it fixed:
   - es: "Mant OK guardar", "Atrás fin resp", "Bloq.".
   - it: "Tutte impost.", "fine risp.", "ferma scan.".
7. **N7. Rendering, not catalog.**
   - The 8x16 font draws "É" as "E" ("Ecoute DMR", "Ecoute A/B").
   - zh: the locked sentence's "*" is drawn in the large Latin font beside 16 px CJK, on the bar's bottom edge. This
     is the known step 5 Chinese bar position; it is no worse than on the labels.
8. **N8. Pending decision, not part of the range.** The working tree has an uncommitted gui-plan.md change that
   decides Quick tile 8 reads "TG/Contact". At `ae4b5581` the tile reads "TG" (known item).

## Design questions for the user

1. **A list's OK and Back groups are all treated as the usual ones.** In some lists OK does more than open or pick,
   and that word is no longer in the hint row:
   - "OK send": message compose and zone share "Send to"; the latter transmits.
   - "OK add" (add to zone).
   - "OK go" (zone channels, `list_hint_zone_channels.png`).
   - "OK tunes to it".
   - "OK edit/save" (channel editor rows).
   - "OK use" (talk target picker; its "Hold OK set" stays).
   - "Back keep editing" (the discard prompt, whose rows say it, `list_hint_discard.png`).

   The answer's examples are all met. If the user counts "OK send" and the like as meanings that differ from the
   usual ones, the structural fix is a flag on those screens' lists (an "OK acts" bit beside `numbered`) that keeps
   their OK and Back groups in the row. It compares no text.
2. **Text entry, emergency and the hotspot screen keep their hint line with Hint row off.** They call `ui_draw_hint`
   directly, as in every theme, because the line is the instructions (`hint_row_off.png`: "Back delete  # ABC/123/abc
   OK keep"). Does "Hint row off hides only the hint line" mean these too?
3. **it "Opzioni" for Settings** (above). Accept it, or keep "Impostazioni" with a tile-only short form.

## Checks

**Ran:**

- **`fork-notes/tools/cc.sh --except profile ../rf-base-s4 ../rf-qa6-work`:** CC OK (`cc.log`).
  - uiSim 0 warnings.
  - 1363 frames, 0 differ.
  - uiThemeTest and uiThemeTestPanels pass; uiPrototypeTest OK.
  - GD-88 0 warnings: 793244 B flash, 205872 B RAM.
- **Full host suite in Docker (`local/rf-host-tests`, `make -k -j16`):** rc 0 (`hostsuite.log`).
  - Only warning: the known `pppTest.c:363` `-Wsign-compare`.
  - I ran `make -C tests/host clean` before and after.
- **Firmware (`gd88.log`, `gd88np.log`, `dm1701.log`, `md9600.log`):** all 0 warnings.
  - GD-88: 793244 B, +780 B against the first review's 792464.
  - GD-88 with `CONFIG_UI_THEME_PANELS=n`: 774148 B.
  - DM-1701: 709496 B.
  - MD-9600: 688444 B.
  - RAM is unchanged on each.
  - Stack checks pass: GD-88 ui 1664 deepest, 204 free; DM-1701 ui 1640, 88 free (unchanged).
- **Checkers:** `tools/i18n.py --check` (1168 strings, 4 catalogs), `check_generic.py`, `check_doc_links.py` and
  `check_keys.py` all pass (`checkers.log`).
- **Mutations:** 33 in a detached worktree (`tools/worktree.sh add ../rf-qa6-mut`), 29 caught (`mutations.log`). The
  ASan suites were built without ASan on the Mac for speed; the baseline of each suite passed first.
- **Renders:** my own HEAD uiSim and UI_LANG builds.
  - 112 profile renders: 4 variants × Hint row on/off × 14 screens.
  - 112 language renders: es, fr, it, zh × Hint row on/off.
  - Crowded strips with the clock: Commercial, Amateur, D-STAR, dual watch and step 2's crowd.
  - Text entry, VFO entry, GPS page, Radio info, discard prompt and zone channels.
  - The rf-step4-out sheets (step2, before, after) side by side.
- **Clean-up:**
  - The worktree is removed, the `build/t-qa6-*` trees are removed, and `build/t-gd88` was removed by cc.sh.
  - `tests/host` is cleaned.
  - The main checkout is untouched. Its only change is someone else's uncommitted `fork-notes/gui-plan.md` edit.

**Not run:**

- Full `tools/ci.sh` matrix, HIL and on-air checks (no radio touched).
- Other boards and switch-off builds beyond GD-88's panels switch.
- I didn't build a fresh `4659ca37` baseline for sizes. The deltas use the first review's figures.
- French at 160x128 and 160x120 (deferred, known).
