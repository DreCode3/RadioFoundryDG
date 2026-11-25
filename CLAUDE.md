@AGENTS.md

<!-- Fork-only section. Upstream's CLAUDE.md is the single line above: drop everything below from upstream PRs. -->

# Working in this fork (DreCode3/RadioFoundryDG)

AGENTS.md (imported above) is upstream's rule set and wins over anything here. This section adds the upstream
contributor and reviewer requirements AGENTS.md only points to, and the fork's own context. The goal is work that
can go upstream as pull requests, so write every change as if it were one.

## Repositories and branches

| Remote | Repository | Use |
|---|---|---|
| `origin` | `DreCode3/RadioFoundryDG` | This fork. All pushes go here |
| `upstream` | `radiofoundry/radiofoundry` | Read only. Add with `git remote add upstream https://github.com/radiofoundry/radiofoundry` |

- `main` mirrors `upstream/main`. Never commit fork work to it; update it only by fast-forward from upstream.
- The GD-88 work sits on upstream's `feature/gd88_bringup` (a maintainer's bring-up branch, not yet in `main`).
  Our GD-88 branches contain it plus upstream `main` merges. An upstream PR from them can only go in after, or on
  top of, that branch: check its state (`git ls-remote upstream feature/gd88_bringup`) before planning one.
- Fork branches:

  | Branch | State |
  |---|---|
  | `feat/gd88-gui` | The working branch. Commit and push here |
  | `feat/gd88-performance` | Parked; may be revisited. Don't merge it into the GUI branch |
  | `feat/gd88-test-install` | Contained in `feat/gd88-gui` |
  | `feat/ui-home-profiles` | Retired. Don't build on it |

- Before starting work, fetch `upstream main` and `upstream feature/gd88_bringup` and say how far behind the branch is.

## Current plan

1. **Finalize the GUI on the GD-88** (310 x 240 drawable, `320x240iR10` in the simulator): visual elements first,
   then QA. Changes stay generic code in `core/ui`, enabled by board capability, but are judged on the GD-88 only.
2. **Freeze** the GD-88 version when the user says the layout and quality are ready.
3. **Port** to other radios and display classes, then fix the CI blockers this leaves (maximal builds that overflow
   on small panels, flash cost with `CONFIG_UI_PROFILES` off, the settings offset test, the gallery).

Until step 3, don't spend effort on other display classes beyond keeping them building and unchanged.

The approved design (theme framework boundary, menu tree, settings groups, naming, layout, Home, order of work) is in
[fork-notes/gui-plan.md](fork-notes/gui-plan.md). Read it before any GUI work and keep it current when the user
decides something. The upstream Discussions draft is [fork-notes/discussion-ui-themes.md](fork-notes/discussion-ui-themes.md).

## Upstream pull request requirements

From [docs/dev/contributing.md](docs/dev/contributing.md) and [.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md):

- **Branch name** `<type>/<area>-<short-description>`: lower case, hyphens, under about 40 characters, no names or
  dates. Types: `feat`, `fix` (`fix/123-...` with an issue), `radio`, `test`, `docs`, `refactor`, `tools`, `ci`.
- **Title** `<Area>: <what changed>`, present tense, no trailing period (`UI: ...`, `GD-88: ...` for one radio's
  change). PRs are squash merged, so the title becomes the commit subject on `main`.
- **One topic per PR.** Split unrelated changes. A large feature goes up as a series of reviewable PRs.
- **Host tests** in `tests/host/` for every code change, named in the PR. A bug fix includes the test that fails
  without it. Code the host can't reach (register writes, wiring) says so, tests the logic around it, and adds an
  opt-in HIL test when one can be scripted. Docs-only PRs are the only exception.
- `JOBS=8 tools/ci.sh` ends with "all checks passed"; list any skips in the PR.
- The template filled in: What and why (link the issue), Radios, the Testing table (ci.sh, host tests, HIL, on air),
  the checklist. A UI change attaches before and after gallery PNGs.
- **Issue first** for a new transport, API, mode, file format or a second way to do an existing job.
- Translations: done before merge, or listed as `- [ ] translations: es, it` in the PR. Say whether each language
  still fits where the English changed and a translation didn't.

## What reviewers check

From [.github/copilot-instructions.md](.github/copilot-instructions.md). Check these before calling work done:

- Tests cover the branches that are easy to skip: error paths, both modes (DMR and FM), empty and full lists, each
  feature switch off, and keys or screens that exist on one radio or one display class only.
- The PR claims no coverage the diff doesn't show.
- No code, names or comments from another firmware. The UI is designed in `docs/ux/` from first principles, not from
  any other firmware's menus, screens or key maps (stock firmware included).
- A parser of outside input (USB, air, network) has a fuzz loop (`tests/host/fuzz.h`), run under `SANITIZE=1`.

## Rules that bite in UI work

- **Flash.** `ci.sh` builds every board's maximal configuration. A feature that cannot fit a board is not
  `BOARD_UNSUPPORTED` unless the hardware can't run it (give the reason, one switch a line in `config.mk`). With the
  switch off, the build must cost nothing: guard definitions with the same condition as their callers.
- **Settings** (`docs/spec/settings-store.md` 3). Fields are only appended. A reserved byte may take a meaning only
  when 0 means "as before". Update `tests/host/settingsTest.c` offset checks and the spec field table together.
- **New setting**: in the user manual's settings reference with its default and status. **New switch**: in
  `docs/build-config.md`, `config/features.mk`, `core/config.h`, `docs/feature-matrix.md` and
  `tests/hil/features.toml`.
- **Gallery**: `make -C tests/host ui-gallery`; commit only the PNGs meant to change plus the contact sheet of each
  changed class. A new screen goes in `gallery[]` in `tests/host/uiSim.c` and gets a `uiFlowTest` flow checking
  `screen=<name>`.
- **Debug link**: anything a HIL test must see is a view key in `core/debuglink/debuglink_view*.c`.
- **HIL skips** use `rfoundry_hil.skip(kind, reason)` with a kind (`not_built`, `no_hw`, `state`, `opt_in`,
  `bench`, `other_board`), never `pytest.skip`.
- Other checkers ci.sh runs: `tools/check_generic.py`, `tools/check_doc_links.py`, `tools/check_keys.py`,
  `tools/i18n.py`, `tools/license_grep.sh`, `reuse lint`, `tools/bringup.py check`.

## Text a user reads

AGENTS.md on this branch has the maintainers' rule for text a user reads (from `feature/gd88_bringup`, not yet in
upstream `main`). It applies to every on-screen label the GUI adds. Short labels on small screens still follow it:
pick a shorter word, don't drop words.

## Fork-only material

Keep these out of upstream PRs, or rework them to upstream conventions first:

- This section of CLAUDE.md.
- `fork-notes/` (plans and drafts for the fork).
- `docs/superpowers/` (planning notes from earlier tool-assisted work).
- One-off CI run write-ups such as `docs/testing/gd88-gui-ci-*.md`. Upstream records bring-up runs with
  `tools/bringup.py run` into `docs/testing/results/<id>/` and puts CI results in the PR's Testing table.

## Authorship

Commits go in under the user's git identity. Contributions are MIT, credited to "The RadioFoundry Authors"
([license of contributions](docs/dev/contributing.md#license-of-contributions)); new files use the standard SPDX and
copyright header, not a personal one.

## This cloud environment

- No radio, debug link or SDR bench is attached. HIL and on-air checks are the user's; ask for results, never claim
  them.
- Host tests run here (`make -C tests/host`). The container is rebuilt between sessions, so the toolchains may be
  missing: `tools/install-arm-toolchain.sh` (Arm, minutes) and `tools/build-csky-toolchain.sh` (C-SKY, about
  45 minutes), plus `git submodule update --init --depth 1`. The C-SKY build first needs
  `apt-get install -y build-essential libgmp-dev libmpfr-dev libmpc-dev libisl-dev texinfo bison flex`; without
  texinfo it fails at `doc/bfd.info`. Say which checks ran and which didn't.
- GD-88 budget (default build, Nov 2026): flash 87.9% (about 107 KB free), RAM 95.0% (about 10.5 KB free). RAM is
  the tight one: prefer `const` tables and drawing on the fly over buffers.
- The UI preview loop: `make -C tests/host uiSim`, then `tests/host/uiSim --geom 320x240iR10 --scale 2 --script
  "<keys>" --out <png>` and show the PNG. `python3 tests/host/uiPrototypeTest.py` checks the profiles.
- Use `tools/worktree.sh` for worktrees, and remove worktrees and `build/` trees when done.
