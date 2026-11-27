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
| Python 3 and Pillow | `python3 -m pip install --user --break-system-packages pillow` (or `brew install pillow`) | Pillow is for `fork-notes/tools/` only. Homebrew's Python refuses a plain `pip install` (PEP 668, "externally-managed-environment") |
| Arm GNU Toolchain 13.3.rel1 (pinned in `platform/stm32f4/platform.toml`) | Download and check it as below, then `mkdir -p ~/opt && tar -xJf <archive> -C ~/opt` | The Makefile finds `~/opt/arm-gnu-toolchain-*-arm-none-eabi/bin/` by itself. If macOS blocks the binaries: `xattr -dr com.apple.quarantine ~/opt/arm-gnu-toolchain-*` (not needed when downloaded with `curl`) |
| Docker (C-SKY image, Linux host tests) | Docker Desktop, or Homebrew `docker` with `colima` (`colima start --arch aarch64`) | Either works; this Mac uses colima. Apple's `container` is untested |
| C-SKY GCC 14.2.0 (GD-88) | `tools/docker/csky-toolchain/run.sh make -j$(sysctl -n hw.ncpu) BOARD=gd88` | The image is named in `platform/hrc7000/platform.toml` and pulled on first use. It is amd64 only, so it runs emulated on Apple silicon (a full build takes about 2 minutes) and Docker prints a platform `WARNING` line unless `DOCKER_DEFAULT_PLATFORM=linux/amd64` is set. Pass `-j` yourself: `run.sh`'s `JOBS` default uses `nproc`, which macOS lacks. A native build (`tools/build-csky-toolchain.sh`) is tested on Linux only |
| Linux host-test image (AddressSanitizer tests) | `docker build -t local/rf-host-tests -f fork-notes/tools/Dockerfile.host-tests fork-notes/tools` | See "AddressSanitizer on macOS" below. Native arm64 |
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
UBSan alone is fine. That is the `gfxFbEdgeTest` stall above. It hits:

- `uiThemeTest` and `gfxFbEdgeTest`, which always build with ASan. The plain `make -C tests/host` run reaches them
  and hangs, and so does `tools/ci.sh`'s host step.
- `make -C tests/host SANITIZE=1` (`RFOUNDRY_CI_SANITIZE=1 tools/ci.sh`).

Run them in the Linux image. `fork-notes/tools/cc.sh` does this for `uiThemeTest` by itself. By hand, remove the Mac
binary first, because make would take a binary from the other OS as up to date, and clean afterwards:

```bash
rm -f tests/host/uiThemeTest
docker run --rm -v "$PWD:$PWD" -w "$PWD" --user $(id -u):$(id -g) -e HOME=/tmp local/rf-host-tests \
	sh -c 'make -s -C tests/host uiThemeTest && ./tests/host/uiThemeTest'
rm -f tests/host/uiThemeTest
```

## Check the setup

```bash
make -C tests/host uiSim
python3 tests/host/uiPrototypeTest.py
make -j$(sysctl -n hw.ncpu) BOARD=dm1701
tools/docker/csky-toolchain/run.sh make -j$(sysctl -n hw.ncpu) BOARD=gd88
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

Every commit from `dc9e29d6` to `8a59ae5f` renders the same pixels, so any of them can serve for step 2. Keep the
output outside the repository. `snapshot.py` uses one render thread per core.

## Where the work stands

- [gui-plan.md](gui-plan.md): the approved design, the process, testing sizes, order of work.
- [reports/](reports/): the theme inventory, step 1 reports and the QA reviews.
- Steps 1 to 4 are QA-approved (step 3, the menu tree and naming, at `e94841a2`; step 4, the panels layout, at
  `7ab0bf3f`). On Nov 26 the menu
  tree and naming moved ahead of the panels layout; gui-plan.md's order of work has what each step starts with.
- Step 3 (menu tree) changed the menus on purpose, and `snapshot.py`'s Amateur and Night prefixes follow its paths
  (Settings is Menu 8, Display profile and Day/Night are Display's first rows): a baseline from before it
  (`46dad875`) differs in every menu, settings and title frame, and in Amateur Home (its talk line leads with the
  number by default).
