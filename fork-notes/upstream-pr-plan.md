<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Upstream pull request plan for the GUI work (fork-only)

How `feat/gd88-gui` splits into reviewable pull requests for `radiofoundry/radiofoundry`, after the Discussion post
([discussion-ui-themes.md](discussion-ui-themes.md)) has the maintainers' answers. Facts as of Nov 28, 2026 (branch
at `bdb27553`); the design is in [gui-plan.md](gui-plan.md).

## Rules that shape the split

From `docs/dev/contributing.md`, `.github/PULL_REQUEST_TEMPLATE.md` and CLAUDE.md:

- **One topic per PR**, squash merged: the title becomes the commit subject on `main`.
- **Branch** `<type>/<area>-<short-description>`, lower case, under about 40 characters. **Title** `<Area>: <what
  changed>`, present tense, no period (`UI: ...`, `<Model>: ...` for one radio).
- **Host tests** named in the PR for every code change; `JOBS=8 tools/ci.sh` ends with "all checks passed", skips
  listed.
- **A UI change attaches** the gallery PNGs it changed, before and after, and commits only the PNGs it meant to change
  plus each changed class's contact sheet.
- **Translations** done before merge, or `- [ ] translations: es, it` in the PR; say whether each language still fits
  where the English changed and a translation didn't.
- The template's Testing table, for every PR:

  | Check | Result |
  |---|---|
  | `JOBS=8 tools/ci.sh` ends with "all checks passed" | yes, on the re-cut branch at `<commit>`; skips listed |
  | Host unit tests added or changed | the tests named below |
  | HIL (`tests/hil`) | not run unless the owner runs it (ask; never claim) |
  | On the radio, on air | not tried (UI only), or what the owner tried |

## What must land first

- **`feature/gd88_bringup`** (`c13059ce`, the maintainer's branch) is not in upstream `main`. The fork's branch sits on
  it plus upstream `main` merges (5 commits behind `main` on Nov 28). Only PR 9 (the GD-88's own change) needs it.
  Check its state before each PR: `git ls-remote upstream feature/gd88_bringup`.
- Every other PR is **re-cut onto upstream `main`**, not opened from `feat/gd88-gui`: the branch's history carries the
  bring-up branch, the first prototype (`c08818d0`..`a948b26d`, `CONFIG_UI_PROFILES` branches later replaced) and
  fork-only files. The bring-up branch touches `core/ui` lightly (`gfx.c`/`gfx.h` banded flush, `ui.c`, two `hal`
  fields): a re-cut PR must carry none of those hunks.
- **The space PRs (1 to 3) go first**, so the DM-1701 and MD-UV380 maximal builds keep their margin when the UI PRs add
  shared code.

## How to re-cut

- **PR 4 (descriptor):** cherry-pick step 1's commits onto `main` (`61a4d75e`, `48e1e944`, `7013200b`, `26fe0875`,
  `4db45f54`, `3a70ab87`, `3b2fd11c`, `2e46cb85`, `d9dfd89b`), dropping every hunk that belongs to the prototype
  (`CONFIG_UI_PROFILES`, `gui_profiles`). Proof: `make -C tests/host ui-gallery` leaves `docs/ux/screens` unchanged.
- **PR 5 (menu tree):** cherry-pick step 3's commits (listed below) on top; strip what is under
  `CONFIG_UI_THEME_PANELS` with `unifdef -UCONFIG_UI_THEME_PANELS` on the hunks that carry it (`/usr/bin/unifdef` is on
  macOS), and keep the rest.
- **PRs 6 to 8:** from the final tree, by file area, each on top of the one before; the panels code is one directory
  (`core/ui/themes/panels/`) plus guarded blocks in shared files, which `git diff` on a `CONFIG_UI_THEME_PANELS` grep
  locates.
- After each re-cut: the full `tools/ci.sh`, the gallery, and a pixel comparison against the fork's baseline
  (`fork-notes/tools/snapshot.py`, `compare.py`; [local-setup.md](local-setup.md)) for the classes the PR must not
  change.

## The series

| # | Branch | Title | Needs |
|---|---|---|---|
| 1 | `ci/maximal-lto-3p` | `Build: the 720 KB radios' maximal builds link the third-party code with LTO` | — |
| 2 | `tools/i18n-stride-16` | `i18n: an offset every 16 strings in the language tables` | — |
| 3 | `refactor/ui-settings-row-20b` | `UI: a Settings row descriptor takes 20 bytes` | — |
| 4 | `refactor/ui-theme-descriptor` | `UI: the reference look behind a theme descriptor` | — |
| 5 | `feat/ui-menu-tree` | `UI: a 3x3 Menu and Settings grouped by topic` | 3, 4 |
| 6 | `feat/ui-narrow-text` | `UI: short forms and whole words for narrow screens` | 5 |
| 7 | `feat/ui-theme-panels` | `UI: the panels theme` | 4, 5, 6 |
| 8 | `feat/ui-panels-compact` | `UI: the panels theme's compact layout for small screens` | 7 |
| 9a | `feat/md380-panels-theme` | `MD-380: the panels theme in Settings > Display > Theme` | 8 |
| 9b | `feat/db25d-panels-theme` | `DB25-D PRO: the panels theme in Settings > Display > Theme` | 8 |
| 9c | `feat/mduv380-panels-off` | `MD-UV380: the panels theme stays out of the default image` | 8 |
| 9d | `feat/gd88-panels-theme` | `GD-88: the panels theme by default` | 8, `feature/gd88_bringup` |
| 10 | `tools/ui-live-sim` (optional) | `Tools: the UI simulator in a browser` | 7 |

The DM-32UV default goes in PR 7 (below), since the gallery's `240x320i10x5` class shows the theme its radio starts in.

### 1. Build: maximal builds with LTO for the third-party code

- **Scope:** `487576a5` (`MAXIMAL_VARS := LTO=1 LTO_3P=1` in `boards/dm1701/config.mk` and `boards/mduv380/config.mk`).
- **Why:** 3,720 / 3,692 B in those maximal builds, no change to any default image.
- **Not here:** the MD-UV380's `FONT_AA` skip (`e5104c5e`/`cee9bbcd`) moves to PR 9c. On upstream `main` that board has
  no panels theme and its maximal build fits with `FONT_AA`, so the skip's reason ("only panels would draw these") would
  be false and it would only drop `FONT_AA` from a maximal build that covers it today.
- **Tests:** none (build configuration); `tools/ci.sh --board dm1701,mduv380 --steps firmware`, `make stack-check`.
- **Gallery:** none. **Translations:** none.

### 2. i18n: an offset every 16 strings

- **Scope:** `2895ec3a` (`tools/i18n.py`, `LANG_STRIDE` 16).
- **Why:** 448 B in a build with three languages, about 150 B per language everywhere; lookup steps over up to 15
  strings instead of 7.
- **Tests:** `i18nTest`, `i18nTestNoLang`, `i18nTestDefault`, `tools/i18n_test.py`.
- **Gallery:** none. **Translations:** none (format only).

### 3. UI: a Settings row descriptor takes 20 bytes

- **Scope:** `3150f2a0` (`core/ui/ui_menus.c`: fields reordered, the unit an index into a 4-entry table, a size
  assert). Re-cut against `main`'s row table (the branch has step 3's extra rows).
- **Why:** 400 B, no visible change.
- **Tests:** `uiFlowTest`, the gallery unchanged.
- **Gallery:** none. **Translations:** none.

### 4. UI: the reference look behind a theme descriptor

- **Scope:** step 1 (above): `core/ui/ui_theme.h` (reference-only part: the descriptor, the ops, `UI_THEME_SELECT`),
  `core/ui/ui_screens.c` (palettes as const tables, the list frame `ui_draw_list` and its four list ops, the banner
  box, the op call form the stack check follows), `core/config.h` (`UI_THEME_SELECT` 0), `tests/host/uiThemeTest.c`
  (the framework tests only, built with `UI_THEME_SELECT=1`), its `tests/host/Makefile` rule and `run-ui` line,
  `docs/spec/ui.md` 9.1 to 9.5 (framework parts), `docs/dev/ui-themes.md` (framework parts).
- **Cost:** with one theme the op calls fold away; the split costs about 196 B on the DM-1701 maximal build (step 1
  measured +112 B DM-1701, −100 B MD-9600 default).
- **Tests:** `uiThemeTest` (a theme with no op draws every frame as the reference; colors; own ops called; strip
  measured where drawn; list frame and out-of-bounds geometry under ASan; theme pick). Stack check on every board.
- **Gallery:** none may change: say so, with `git status docs/ux/screens` clean after `ui-gallery`.
- **Translations:** none.

### 5. UI: a 3x3 Menu and Settings grouped by topic

- **Scope:** step 3: `7f1299eb` and `2d77b694` (simulator fixes it needs), `c1709842`, `2c8d800c`, `e1bf45bb`, `bfd0f1a6`,
  `f552d775`, `8e391d3b`, `2bdf37a6`, `c50d304a`, `61c21695`, `54699fd5`, `86698792`, `bd8dbab5`, `5d407e31`,
  `4f58a559`, `cb87a9ef`, `9243663e`, `d9d107c1`, `256697ae`, `68f72db2`, `524da753`, `02d18d40`, `63cb8a20`,
  `4dacb579`, `179779ac`, `1a8f0e51`, `b4196af9`, `5ec36c4b`, `38ce4dac` (the Chinese manual). File areas:
  `core/ui/ui_menus.c`, `ui_app.c`, `ui_find.c`, `ui_nav.h`, `core/debuglink/debuglink_view*.c` (the focused row's
  label), `tests/host/uiFlowTest.c`, `tests/hil/test_ui.py` and the HIL tests that walk menus, `docs/ux/`
  (information architecture, task flows, usability tests), `docs/user/` (every page with a menu path),
  `docs/i18n/zh/user/`, the catalogs.
- **Not in it:** the Theme row and Talkgroup name first's theme default (`9229c79f`, `9c23b09d`, `55c0de35`,
  `4e9cc7e6`, `c327d532`, `ca0b8bc0`, `e94841a2`): PR 7.
- **Issue first?** The Discussion stands for it: it changes every radio's numbered key paths.
- **Cost:** about +2.9 KB flash, +64 B RAM (step 3 measured on the GD-88).
- **Tests:** `uiFlowTest` (every task flow and press budget on four display and key map pairs, the crawler),
  the pick list's check mark, value editor and deepest Back path (`524da753`), the mutation tests (`179779ac`),
  `radioCoreTest`, `i18nTest`, HIL `test_ui.py` (owner).
- **Gallery:** most menu, settings and title frames of every class (step 3 changed 409, then 44, 151 and 20 PNGs).
  Attach before and after for Menu, Settings, Settings > Display, a settings page, Quick, Find, at 160x120i5 and
  240x320.
- **Translations:** every catalog updated on the branch (`tools/i18n.py --check`: 1173 strings, 4 catalogs ok). Open
  for native speakers: Italian "Opzioni" (Settings, kept to fit the tile), French and Chinese wording.

### 6. UI: short forms and whole words for narrow screens

- **Scope:** shared text work from steps 4, 5b and 7: `54ae6247`, `1a8b76f0`, `c030c5e4`, `691ce09e`, `1359045a`,
  `a0015e0e`, `a954d7c6`, `119f6e91`, `cacaf3cd`, `4ae847ce`, `924df11a` (a wrapped line never starts with French
  punctuation), `7e9e2363`, `42981bde` ("Hold \* to unlock"), `be8c30db` (five hint short forms), `18a55b1d`
  (`{Connections|Links}`). `6f1f2382` ("Hold OK save", 70 PNGs) and `723e5d54` (the GPS sky's empty-state line) are
  separate topics: two small PRs of their own, or named as such in this one.
- **Tests:** `uiThemeTest`'s wrap test (`test_wrap_keeps_punctuation_on_its_word`), `uiSim --ellipsis` and `--layout`
  at the 160 px classes, `i18nTest`.
- **Gallery:** the frames whose hints or labels change at 160x120i5 and 160x128 (be8c30db changed 39 PNGs).
- **Translations:** English short forms approved; French, Spanish and Italian forms written, native review open (the
  list in gui-plan.md step 5b); Chinese short forms added so the group counts match, native review open.

### 7. UI: the panels theme

- **Scope:** the theme and the shared code only it reads, in the full layout, plus the DM-32UV as its first default:
  - `core/ui/themes/panels/panels.c` and `panels_icons.h` (full layout and its 24 px icons; the compact parts go in
    PR 8), `tools/profile_icons.py`.
  - `core/ui/ui_theme.h` (panels parts: `UI_BAR_*`, options, `UI_THEME_SHARED` helpers), the guarded blocks in
    `ui_screens.c`, `ui_screens.h`, `ui_app.c`, `ui_menus.c`, `ui_dual.c`, `ui_svc_*.c`, `gfx.c`/`gfx.h`
    (`gfx_coverage_pixel`, `gfx_reserve_colors`, `gfx_text_title`).
  - The settings: `gui_flags` (offset 406) and `gui_theme` (407) in `core/settings/settings.h`,
    `docs/spec/settings-store.md`, `tests/host/settingsTest.c` offsets (`83ec470d`).
  - The switch: `config/features.mk` (`UI_THEME_PANELS`, `UI_THEME_DEFAULT`, the size rule), `core/config.h`,
    `Makefile`, `tests/hil/features.toml`.
  - The Theme row and the theme-dependent name-first default (the PR 5 exclusions), Day/Night, Display profile, GPS
    clock, the hint row's OK and Back flags (`SF_OK_ACTS`, `SF_BACK_ACTS`).
  - `boards/dm32/config.mk` (`CONFIG_UI_THEME_PANELS ?= $(CONFIG_UI_COLOR)`) and `board.mk` (`GFX_MIN_ROWS=320`),
    `cc296f81`; the simulator's `240x320i10x5` class in panels (`d04d9c40`).
  - Dual watch's calls heard in panels (step 8, shared behavior 6): `ui_dual_style_t.list_rows` and the shared rows
    (`dual_call_listed`) in `ui_dual.c`/`ui_screens.c`, Commercial naming the caller, the full layout's row counts and
    pane offsets: `533cf58f` (code and tests), `808fccd7` (docs), `5ee4d927` (gallery), with `aa84b75a` (the
    two-receivers width check built only with panels). The compact layout's one row and the short form go in PR 8.
  - The gallery: the `320x240iR10-panels` class, the GD-88 look as the radio draws it (`380e0e34`), and the panels
    classes' night pages drawing the Night palette (`cbae19f3`, the `daynight:` script token). Neither needs the
    bring-up branch: the class is a simulator geometry, not the GD-88 board.
  - Docs: `docs/spec/ui.md` section 9, `docs/ux/panels-theme.md`, `docs/dev/ui-themes.md`, `docs/build-config.md`,
    `docs/feature-matrix.md`, `docs/user/05`, `06`, `17`, `23-radios.md` (DM-32UV), `docs/ux/profile-samples/`.
  - Commits: steps 2, 4, 5 and 5b's panels parts (`6590540a`..`70a91cc3`, `94aad766`, `98f73a3c`, `22a46540`..`7ab0bf3f`,
    `42f3557a`, `adafdf7f`..`934d80c5`, `977b6312`, `02480f08`, `0b5fc3b6`, `58c03bb4`, `da089dda`, `ce4b3b3f`), the
    step 3 and 4 theme items (`d82a2621`, `ffeb20e0`, `1e2e2af6`, `730666cc`, `bda35898`, `7d366d1d`, `8b6f4c3f`,
    `057872a3`, `b9c401a2`, `4b4b665a`, `a8d7352b`, `bb6adc36`, `6c121484`), step 7 part A (`c9ae518b`, `ebc900ee`,
    `9ac6ddf1`, `9a0c68cd`, `15ef7932`, `addb9f73`, `3807f176`, `1a034b81`, `34bf700c`), `b87adff8`, and this
    session's docs commits.
- **Not in it:** `docs/ux/wireframes/gd88-home-profiles.html` (an earlier prototype, not the design), the uiLive
  simulator (PR 10), everything fork-only (below).
- **Cost:** the DM-32UV's default image +19,596 B (rebuilt at `bdb27553`, panels on against off; dual watch's list is
  about 690 B of it); every other board 0 B of panels code.
- **Tests:** `uiThemeTestPanels`, `uiThemeTestPanelsOriginal`, `uiPrototypeTest.py` (with `uiSimZh`), the `uiSimFr` checks in `run-ui`,
  `uiThemeTest`, `settingsTest` (offsets 406, 407), `radioCoreTest` (talkgroup order default), `uiFlowTest`, the
  layout check over `240x320i10x5` and `320x240iR10-panels`. `uiThemeTestPanels`' dual watch cases (rows per display,
  the 310 x 240 offsets, Commercial's caller). Stack check on the DM-32UV.
- **Gallery:** the `240x320i10x5` class (189 PNGs, the DM-32UV's look changes), the new `320x240iR10-panels` class
  (189 PNGs, the GD-88 look) and the four 310 x 240 profile samples. Attach Home, a call, dual watch with its calls
  heard, Menu, Quick, Settings, Settings > Display, a list, the keys-locked bar and a night page, before (Original, the
  `320x240` class) and after.
- **Translations:** the panels labels (`SET_VALUE_THEME_*`, `SET_LABEL_DAY_NIGHT`, `SET_LABEL_GUI_PROFILE` with
  `{Display profile|Profile}`, Quick's whole words, the month names of the GPS clock) are in every catalog; native
  review open as in PR 6.
- **Alternative:** if the maintainers want the DM-32UV default apart, it becomes PR 7b (`feat/dm32-panels-default`),
  and PR 7's gallery shows the full layout in `320x240iR10-panels` and the profile samples until then.

### 8. UI: the panels theme's compact layout for small screens

- **Scope:** step 7 part B and the QA fixes: `6f510992`, `348aa65e`, `75020b42`, `7a675e85` (`GFX_MIN_ROWS` leaves the
  compact layout out), `f8ad4b94`, `b51b466e` (the sliding focused row), `edee0597`, `8a024c6b`, `a0b4c059`, `90faf70e`,
  `af02fcd9` (`FONT_AA_THEME_ONLY`), `b9938edf`, `44f4beb3`, `9fc4314a`, the 158 x 127 fit from `aeb73816`; the 16 px
  icon set in `panels_icons.h`; the gallery classes `160x128-panels`, `160x128iL2T1-panels`, `220x176iR10-panels`; the
  compact parts of ui.md 9.6 to 9.12 and the manual. Dual watch's compact parts from step 8: one call row (the panes'
  padding to 0) and the two-receivers line's short form `{Receiving on A and B|A and B receiving}` (`bbfccbd6`,
  `817fbd5b`).
- **Cost:** the GD-88's panels cost (both layouts, 24,124 B) less the DM-32UV's (full layout, 19,596 B) is 4,528 B
  (rebuilt at `bdb27553`). That compares two boards, so it bounds the compact layout's share rather than measuring it alone. A
  compact-only board pays about 17.4 KB for the theme.
- **Tests:** `uiThemeTestPanels`' compact cases (rows with and without the hint row, Home, a call, the reference Home
  body, the grids, the sliding row, the list flags of a measured list), `uiThemeTestPanelsOriginal`, the layout check
  over the three new classes.
- **Gallery:** the three new classes (about 567 PNGs and 3 contact sheets). Attach Home, a call, Menu, Quick,
  Settings, Settings > Display and a long list at 158 x 127.
- **Translations:** `{Keys locked: hold * to unlock|Locked: hold * to unlock|Hold * to unlock}` in every catalog
  (from PR 6). New: "A and B receiving", with es, it, fr and zh short forms written, native review open.

### 9. Per radio

| PR | Scope | Tests | Gallery |
|---|---|---|---|
| 9a MD-380 | `boards/md380/config.mk`: panels on, original look by default (`CONFIG_UI_THEME_DEFAULT ?= 1`), `FONT_AA` with it and `FONT_AA_THEME_ONLY`, UI stack 548 words; `docs/user/23-radios.md`; part of `aeb73816` | MD-380 default and `no-UI_THEME_PANELS` builds, `make stack-check` (about 200 B free) | none (the class exists from PR 8) |
| 9b DB25-D PRO | `boards/db25d/config.mk` as the MD-380's, `7e6aa89e`; `23-radios.md` | DB25-D builds (C-SKY), stack check | none |
| 9c MD-UV380 | `boards/mduv380/config.mk`: panels off by default, `UI_THEME_PANELS` and `FONT_AA` in `MAXIMAL_SKIP` (`e5104c5e`/`cee9bbcd`: its default image draws no anti-aliased font; 6,080 B, without which the maximal build was 272 B short after PRs 1 to 3), the UI stack for a custom build that turns panels on; `23-radios.md`. The PR says the maximal build stops covering `FONT_AA` on this board (the MD-380, DB25-D, DM-32UV and GD-88 builds still do). If the maximal build overflows earlier in the series, this skip moves to the PR that overflows it, with that reason | MD-UV380 default and maximal | none |
| 9d GD-88 | `boards/gd88/config.mk` (`CONFIG_UI_THEME_PANELS ?= $(CONFIG_UI_COLOR)`), `23-radios.md`. Its look's gallery class (`320x240iR10-panels`) comes with PR 7; the plain `320x240` class stays its Original look | GD-88 default, maximal and switch-off builds, stack check | none |

Translations: none in 9a to 9c; 9d none.

### 10. Tools: the UI simulator in a browser (optional)

- **Scope:** `tests/host/uiLive.py`, `uiLive.html`, `uiLiveTest.py`, the testing.md paragraph. Rename its `gui:prototype`
  button and token to the theme's name first.
- **Tests:** `uiLiveTest.py`.

## Side PRs that came up

- **`test/llvm-unused-globals`:** LLVM 23 warns about 19 test counters set and never read (`-Wunused-but-set-global`;
  local-setup.md lists them). Read each in a check or drop it. Not needed for upstream's GCC CI.
- **The pixel baseline tools** (`fork-notes/tools/snapshot.py`, `compare.py`) as `tools/`, if the maintainers want a
  no-visual-change proof beyond the gallery (Discussion question).

## Fork-only material: never in an upstream PR

- The fork section of `CLAUDE.md`, `fork-notes/`, the "Fork-only" block at the end of `tools/generic_allow.txt`,
  `docs/superpowers/`, `docs/testing/gd88-gui-ci-*.md`.
- `docs/ux/wireframes/gd88-home-profiles.html` (the earlier prototype; the step 2 renders are the visual reference).

## Open items to settle before PR 7

- The gallery's light and high-contrast pages in a panels class repeat the Day frame, since panels ignores Colors (the
  night pages draw panels' Night since `cbae19f3`). Skip them in panels classes? (Discussion question 9.)
- The plain `320x240` class stays the GD-88's Original look; the GD-88's panels look is `320x240iR10-panels`.
- `uiPrototypeTest.py` and `uiLive` still say "profiles" and "prototype" in places (`320x240iR10` is the
  `gui_profiles` geometry): rename before upstream review.
