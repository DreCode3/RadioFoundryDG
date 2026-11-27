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
| Host compiler, make, zlib | `xcode-select --install` | Host tests and the UI simulator (Apple clang 17, GNU make 3.81) |
| Homebrew LLVM clang (AddressSanitizer tests) | `brew install llvm` (23.1.6 on this Mac) | Not on PATH: name it with `CC`. See "AddressSanitizer on macOS" below |
| Python 3 and Pillow | `python3 -m pip install --user --break-system-packages pillow` (or `brew install pillow`) | Pillow is for `fork-notes/tools/` only. Homebrew's Python refuses a plain `pip install` (PEP 668, "externally-managed-environment") |
| Python venv for CI (pytest, pyserial, numpy, reuse) | See "Python venv for CI" below | Outside the repository, at `../rf-venv` |
| Arm GNU Toolchain 13.3.rel1 (pinned in `platform/stm32f4/platform.toml`) | Download and check it as below, then `mkdir -p ~/opt && tar -xJf <archive> -C ~/opt` | The Makefile finds `~/opt/arm-gnu-toolchain-*-arm-none-eabi/bin/` by itself. If macOS blocks the binaries: `xattr -dr com.apple.quarantine ~/opt/arm-gnu-toolchain-*` (not needed when downloaded with `curl`) |
| Docker (C-SKY image, Linux host tests) | Docker Desktop, or Homebrew `docker` with `colima` (`colima start --arch aarch64`) | Either works; this Mac uses colima. Apple's `container` is untested |
| C-SKY GCC 14.2.0 (GD-88, DM-32, DB25-D...) | Native: `~/toolchains/csky-elf`, built with `tools/build-csky-toolchain.sh` plus [patches/build-csky-toolchain-macos.patch](patches/build-csky-toolchain-macos.patch) (`MAKEINFO=true` on make's command line, `--with-system-zlib --without-zstd`, GCC's prerequisites built in its tree), about 6 minutes at `-j12` | `platform/hrc7000/platform.mk` finds it first, so plain `make BOARD=gd88` builds natively: about 7 s a configuration, against about 280 s in the amd64 image under Rosetta. Its output matches the image's except the order of newlib's string functions (the image's `libc.a` lists them differently), so hashes differ while sizes match. The image (`tools/docker/csky-toolchain/run.sh`) stays the cross-check |
| Linux host-test image (Linux cross-check) | `docker build -t local/rf-host-tests -f fork-notes/tools/Dockerfile.host-tests fork-notes/tools` | See "AddressSanitizer on macOS" below. Native arm64 |
| Claude Code | Start it in the clone (CLI `claude`, or open the folder as a local session in the desktop app) | `CLAUDE.md` loads by itself |

The Arm archive is `arm-gnu-toolchain-13.3.rel1-darwin-arm64-arm-none-eabi.tar.xz` (about 123 MB). Arm publishes its
SHA-256 in a `.sha256asc` file beside it; the downloads page refuses scripted fetches (HTTP 403). The checksum in
`platform.toml` is for the Linux x86_64 archive, not this one.

```bash
B=https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-darwin-arm64-arm-none-eabi.tar.xz
curl -fLO "$B" && curl -fLO "$B.sha256asc" && shasum -a 256 -c arm-gnu-toolchain-13.3.rel1-darwin-arm64-arm-none-eabi.tar.xz.sha256asc
# expect fb6921db95d345dc7e5e487dd43b745e3a5b4d5c0c7ca4f707347148760317b4
```

The Nov 25 CI record notes two macOS problems: an AddressSanitizer stall in `gfxFbEdgeTest` and an internal compiler
error from a translated (x86) Arm compiler under LTO. Use the native arm64 Arm toolchain. Run the full `tools/ci.sh`
matrix in the Linux VM if the Mac stalls.

### AddressSanitizer on macOS

Apple clang 17's ASan runtime hangs at start-up on macOS 27 (checked on 27.3): every binary built with
`-fsanitize=address`, even an empty `main`, spins forever in `__asan::InitializeShadowMemory` before `main` runs.
UBSan alone is fine. That is the `gfxFbEdgeTest` stall above. It hits `uiThemeTest`, `uiThemeTestPanels*` and
`gfxFbEdgeTest`, which always build with ASan (so the plain `make -C tests/host` run and `tools/ci.sh`'s host step),
and `make -C tests/host SANITIZE=1` (`RFOUNDRY_CI_SANITIZE=1 tools/ci.sh`).

Homebrew LLVM's clang has a working ASan on macOS 27. Name it with `CC`; neither `tools/ci.sh` nor
`tests/host/Makefile` changes. `fork-notes/tools/host-cc.sh` runs it with one flag added (below). Clean first and
afterwards, because make takes binaries from another compiler as up to date:

```bash
HCC=$PWD/fork-notes/tools/host-cc.sh       # absolute: make -C changes directory
make -s -C tests/host clean
make -C tests/host CC=$HCC -j$(sysctl -n hw.ncpu)               # the whole suite
make -s -C tests/host clean
make -C tests/host CC=$HCC SANITIZE=1 -j$(sysctl -n hw.ncpu)    # under ASan and UBSan
make -s -C tests/host clean
CC=$HCC tools/ci.sh --steps host                                # make reads CC from the environment
make -s -C tests/host clean
```

All three pass natively (Nov 27, `b87adff8`, LLVM 23.1.6): the suite in 2 minutes at `-j16`, `SANITIZE=1` in 6.5
minutes with no sanitizer report, and the ci.sh host step with no warning. No test failed natively, so none
needed the Linux image.

The flag is `-Wno-unused-but-set-global`. LLVM 23 warns (`-Wall`) about a static global that is set and
never read, and 19 test counters in `tests/host` and `boards/*/tests` are (Nov 27: `job_runs` in
`radioCoreTest.c`, `prototype_gui` in `sim/ui_sim.c`, counters in `hrc6000DmrTest.c`, `netSvcCases.c`,
`bootGd88Test.c`, `md9600RfSeqTest.c` and others). Apple clang 17 and the Linux GCC upstream CI uses don't have the
warning, and ci.sh's host step fails on any `warning:` line. `host-cc.sh` puts the flag last, because the
Makefile's `-Wall` after it would turn the warning back on. The real fix is in the tests (read each counter in a
check, or drop it), for upstream to take.

`fork-notes/tools/cc.sh` and `ci-par.sh` use `host-cc.sh` by themselves on macOS when LLVM is installed
(`RF_HOST_CC` names another compiler; `RF_LLVM` another LLVM prefix).

**Linux cross-check.** Upstream CI runs the host tests on Linux with GCC, so a result that matters goes through the
Linux image too (`cc.sh` falls back to it when there is no LLVM). Start the VM first (`colima start`, and `colima
stop` afterwards), remove the Mac binaries, and clean afterwards:

```bash
make -s -C tests/host clean
docker run --rm -v "$PWD:$PWD" -w "$PWD" --user $(id -u):$(id -g) -e HOME=/tmp local/rf-host-tests \
	sh -c 'make -s -C tests/host uiThemeTest && ./tests/host/uiThemeTest'
make -s -C tests/host clean
```

Inside the image a whole-suite make prints "Clock skew detected" about the bind-mounted tree, which `tools/ci.sh`'s
warning check counts as a warning: run ci.sh's host step natively.

### Python venv for CI

`tools/ci.sh`'s Python step skips the HIL collection, the HIL helper and APRS tests without pytest and numpy, and the
licenses step skips `reuse lint` without reuse. They go in a venv outside the repository, with Homebrew's Python
(nothing system-wide; all four install as wheels for Python 3.15):

```bash
cd ..   # beside RadioFoundryDG
/opt/homebrew/bin/python3 -m venv rf-venv
rf-venv/bin/python3 -m pip install --upgrade pip
rf-venv/bin/python3 -m pip install pytest pyserial numpy reuse
```

`ci-par.sh` puts `../rf-venv/bin` first on PATH when it exists (`RF_VENV` names another). By hand:
`PATH=$PWD/../rf-venv/bin:$PATH tools/ci.sh --steps python,licenses`. The venv has no Pillow:
`fork-notes/tools/` keep using the user install above.

### Parallel full CI

`tools/ci.sh` always writes `build/ci`, so one checkout runs one ci.sh at a time. `fork-notes/tools/ci-par.sh`
runs the whole gate with the boards in parallel:

```bash
fork-notes/tools/ci-par.sh                      # every board, output in ../rf-work.noindex/ci-<short HEAD>
fork-notes/tools/ci-par.sh -p 6 ../rf-ci-out    # 6 lanes
fork-notes/tools/ci-par.sh --board dm1701,md380 --no-checks   # some boards, firmware only
```

- The boards are each `boards/<b>/board.mk` whose compiler is installed (C-SKY boards once
  `~/toolchains/csky-elf/bin/csky-elf-gcc` exists, natively; never in Docker). They wait in one queue, the most
  configurations first, and each lane takes the next board when it is free, so the light boards fill in at the end.
- Each lane (default 4) is a worktree of HEAD (`tools/worktree.sh add ../rf-par-<sha>-<n>`, third_party from this
  checkout) running `ci.sh --board <b> --steps firmware` for each board it takes, with `JOBS` = cores / lanes.
- Meanwhile this checkout runs the other steps once (registry, host with the LLVM `CC`, bench, python, licenses,
  similarity) with the venv. `tests/host` is cleaned before and after it, so rebuild `uiSim` afterwards.
- It builds HEAD: commit first.
- The output folder gets `summary.txt` (also printed), each board's ci.sh output (`<b>.log`), `checks.log`, the
  size reports (one per board and `size-report.md` combined) and in `logs/` the build logs of the configurations
  that failed. Then the worktrees and their build trees are removed, also on Ctrl-C. It exits 1 when anything failed.

The first run (Nov 27, `799fca27`, 4 lanes x `JOBS=4`) built all 10 boards natively, 802 configurations (Arm and
C-SKY), plus the other steps in 51 minutes: about 16 configurations a minute, against 9 for the sequential `JOBS=16`
ci.sh run earlier that day (the four Arm boards, 362 configurations, in 41 minutes). Each board took 20 to 28 minutes
(gd88b 2, rt10 6). The only failures were the known DM-1701 and MD-UV380 maximal overflows (FLASH over by 3552 bytes
on the DM-1701, 4812 on the MD-UV380). Spotlight (`mds_stores`), the backup and antivirus daemons took several cores
while the new worktrees and build trees appeared. ci-par.sh now keeps its worktrees and output in `../rf-work.noindex`, which Spotlight skips by its name; exclude that folder (and `build/` in this checkout) in the backup and antivirus apps.

## Check the setup

```bash
make -C tests/host uiSim
python3 tests/host/uiPrototypeTest.py
make -j$(sysctl -n hw.ncpu) BOARD=dm1701
make -j$(sysctl -n hw.ncpu) BOARD=gd88
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

`fork-notes/tools/cc.sh ../rf-baseline ../rf-work` runs the whole per-commit check (uiSim, pixels, `uiThemeTest`,
`uiPrototypeTest`, a GD-88 build and its stack check). `scratch.sh` sets up step 1's scratch second theme for the
stack-check proof.

The current baseline is `../rf-base-step7`, rendered from `e0639219` after step 7 (the port): the DM-32 class in panels and the compact classes included. `../rf-base-freeze` (the frozen GD-88 look) differs from it only in the port's intended classes. Every commit from `dc9e29d6` to `8a59ae5f` renders the same pixels, so any of them can serve for step 2. Keep the
output outside the repository. `snapshot.py` uses one render thread per core.

## Where the work stands

- [gui-plan.md](gui-plan.md): the approved design, the process, testing sizes, order of work.
- [reports/](reports/): the theme inventory, step 1 reports and the QA reviews.
- Steps 1 to 5 are QA-approved (step 3, the menu tree and naming, at `e94841a2`; step 4, the panels layout, at
  `7ab0bf3f`; step 5, icons and palettes, at `934d80c5`). The GD-88 look is frozen at `226ad2d4` (Nov 27), the
  baseline for the port step. On Nov 26 the menu
  tree and naming moved ahead of the panels layout; gui-plan.md's order of work has what each step starts with.
- Step 3 (menu tree) changed the menus on purpose, and `snapshot.py`'s Amateur and Night prefixes follow its paths
  (Settings is Menu 8, Display profile and Day/Night are Display's first rows): a baseline from before it
  (`46dad875`) differs in every menu, settings and title frame, and in Amateur Home (its talk line leads with the
  number by default).
