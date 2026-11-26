<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Working locally on the Mac (fork-only)

The work moved from a cloud session to the user's Apple silicon Mac on Nov 26, 2026. Everything is on GitHub in
`feat/gd88-gui`; nothing lives only in the cloud container.

## One-time setup

```bash
git clone https://github.com/DreCode3/RadioFoundryDG.git
cd RadioFoundryDG
git switch feat/gd88-gui
git submodule update --init --depth 1
git remote add upstream https://github.com/radiofoundry/radiofoundry
```

| Need | How | Notes |
|---|---|---|
| Host compiler, make, zlib | `xcode-select --install` | Host tests and the UI simulator |
| Python 3 and Pillow | `python3 -m pip install pillow` | Pillow is for `fork-notes/tools/` only |
| Arm GNU Toolchain 13.3.rel1 (pinned in `platform/stm32f4/platform.toml`) | Download the macOS arm64 `arm-none-eabi` build of 13.3.rel1 from Arm's GNU Toolchain downloads page, check its SHA-256 against Arm's, unpack into `~/opt/` | The Makefile finds `~/opt/arm-gnu-toolchain-*-arm-none-eabi/bin/` by itself. If macOS blocks the binaries: `xattr -dr com.apple.quarantine ~/opt/arm-gnu-toolchain-*` |
| C-SKY GCC 14.2.0 (GD-88) | Docker Desktop or Apple's `container`, then `tools/docker/csky-toolchain/run.sh make BOARD=gd88` | The image is named in `platform/hrc7000/platform.toml`. A native build (`tools/build-csky-toolchain.sh`) is tested on Linux only |
| Claude Code | Start it in the clone (CLI `claude`, or open the folder as a local session in the desktop app) | `CLAUDE.md` loads by itself |

The Nov 25 CI record notes two macOS problems: an AddressSanitizer stall in `gfxFbEdgeTest` and an internal compiler
error from a translated (x86) Arm compiler under LTO. Use the native arm64 Arm toolchain. Run the full `tools/ci.sh`
matrix in the Linux VM if the Mac stalls.

## Check the setup

```bash
make -C tests/host uiSim
python3 tests/host/uiPrototypeTest.py
make BOARD=dm1701
tools/docker/csky-toolchain/run.sh make BOARD=gd88
```

## The pixel baseline for refactors

The no-visual-change steps compare screens against a baseline (`fork-notes/tools/`). Render it locally once, from a
commit with the look the step must keep, then compare after every commit:

```bash
make -C tests/host uiSim
python3 fork-notes/tools/snapshot.py "$PWD" ../rf-baseline          # once, before the step
python3 fork-notes/tools/snapshot.py "$PWD" ../rf-current           # after a change
python3 fork-notes/tools/compare.py ../rf-baseline ../rf-current     # must report 0 differ, 0 missing
```

Every commit from `dc9e29d6` to `8a59ae5f` renders the same pixels, so any of them can serve for step 2. Keep the
output outside the repository. `snapshot.py` uses one render thread per core.

## Where the work stands

- [gui-plan.md](gui-plan.md): the approved design, the process, testing sizes, order of work.
- [reports/](reports/): the theme inventory, step 1 reports and the QA reviews.
- Step 1 is QA-approved. Step 2 (selection and the second theme) had just started; its hand-off is in
  `reports/theme-step2-handoff.md`.
