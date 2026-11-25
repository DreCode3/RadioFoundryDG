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
- Fork branches: `feat/gd88-gui` (display profiles, the current focus), `feat/gd88-performance`,
  `feat/gd88-test-install`, `feat/ui-home-profiles` (older).
- Before starting work, fetch `upstream main` and `upstream feature/gd88_bringup` and say how far behind the branch is.

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

Maintainer rule from the GD-88 bring-up branch (not yet in `main`'s AGENTS.md); applies to the site, the radio's
screens, error messages and `docs/user/`:

- Natural, professional English as a native technical writer would write it. "No filler" means no padding, never
  dropping words. Don't copy the clipped style of older strings.
- Full sentences with normal connectives (so, then, to, because). Don't join two clauses with a colon to save words.
- Lead with what the user cares about; put detail in a trailing phrase. Active verbs: the user gets the imperative
  ("Select Continue"), the site calls itself "this page". Avoid vague passives.
- An instruction names the action and when it ends. Keep internal terms out (loader, slot, SoC, probe).
- Say only what the user needs at that point. Don't claim a result before it happens, and don't warn against
  something the user had no reason to do. Read each string aloud.

## Fork-only material

Keep these out of upstream PRs, or rework them to upstream conventions first:

- This section of CLAUDE.md.
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
- Host tests run here (`make -C tests/host`). The Arm and C-SKY toolchains are not installed by default, so
  firmware builds and the flash matrix need them installed (`tools/build-csky-toolchain.sh` for C-SKY) or a run on
  the user's machine. Say which checks ran and which didn't.
- Use `tools/worktree.sh` for worktrees, and remove worktrees and `build/` trees when done.
