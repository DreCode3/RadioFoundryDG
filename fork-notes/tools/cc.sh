#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# The per-commit check (fork-notes/gui-plan.md, Process): uiSim with no warnings, the pixel comparison against a
# baseline, uiThemeTest and uiThemeTestPanels, uiPrototypeTest and one GD-88 build with its stack check.
#   fork-notes/tools/cc.sh [--except SUBTREE]... BASELINE_DIR WORK_DIR [nofw]
# BASELINE_DIR: made once with snapshot.py (fork-notes/local-setup.md). WORK_DIR: scratch output, outside the repo.
# --except leaves a subtree of the baseline out of the pixel comparison (compare.py), for a step that changes those
# frames on purpose (profile: the panels theme's renders); every other frame must still be identical
# The GD-88 builds with csky-elf-gcc when it is installed, else in the C-SKY container (tools/docker/csky-toolchain).
# uiThemeTest is built with AddressSanitizer, whose Apple clang runtime hangs at start-up on macOS: there it builds with
# Homebrew LLVM's clang (host-cc.sh; RF_HOST_CC names another) and runs natively, else in the Linux image
# $RF_HOST_IMAGE (default local/rf-host-tests, from Dockerfile.host-tests).
set -u
EXCEPT=()
while [ "${1:-}" = --except ]; do EXCEPT+=(--except "${2:?subtree}"); shift 2; done
BASE=${1:?baseline dir}; OUT=${2:?work dir}; FW=${3:-fw}
ROOT=$(git rev-parse --show-toplevel); cd "$ROOT" || exit 2
HERE=fork-notes/tools; mkdir -p "$OUT"
NCPU=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
if command -v csky-elf-gcc >/dev/null || [ -x "$HOME/toolchains/csky-elf/bin/csky-elf-gcc" ]; then MK=(make); else MK=(tools/docker/csky-toolchain/run.sh make); fi
# The C-SKY image is amd64 only: name its platform, or Docker prints a WARNING line on arm64 hosts
export DOCKER_DEFAULT_PLATFORM=${DOCKER_DEFAULT_PLATFORM:-linux/amd64}
HOST_CC=${RF_HOST_CC:-}
[ -z "$HOST_CC" ] && [ "$(uname -s)" = Darwin ] && "$ROOT/fork-notes/tools/host-cc.sh" --version >/dev/null 2>&1 &&
	HOST_CC=$ROOT/fork-notes/tools/host-cc.sh
if [ -n "$HOST_CC" ]; then
	THEME=(env CC="$HOST_CC" sh -c)
elif [ "$(uname -s)" = Darwin ]; then
	THEME=(docker run --rm --platform linux/arm64 -v "$ROOT:$ROOT" -w "$ROOT" --user "$(id -u):$(id -g)" -e HOME=/tmp
	       "${RF_HOST_IMAGE:-local/rf-host-tests}" sh -c)
else
	THEME=(sh -c)
fi
ok=1
make -s -C tests/host uiSim >"$OUT/uisim.log" 2>&1; rc=$?; w=$(grep -ci warning "$OUT/uisim.log")
echo "uiSim rc $rc warnings $w"; [ $rc = 0 ] && [ "$w" = 0 ] || { ok=0; head -30 "$OUT/uisim.log"; }
if [ $ok = 1 ]; then
	rm -rf "$OUT/cur" && python3 $HERE/snapshot.py "$ROOT" "$OUT/cur" && python3 $HERE/compare.py ${EXCEPT[@]+"${EXCEPT[@]}"} "$BASE" "$OUT/cur" || ok=0
	# rm: a binary from the other OS or compiler would look up to date
	rm -f tests/host/uiThemeTest tests/host/uiThemeTestPanels
	"${THEME[@]}" 'make -s -C tests/host uiThemeTest uiThemeTestPanels && ./tests/host/uiThemeTest && ./tests/host/uiThemeTestPanels' \
		>"$OUT/theme.log" 2>&1; rc=$?
	rm -f tests/host/uiThemeTest tests/host/uiThemeTestPanels
	echo "uiThemeTest and uiThemeTestPanels (${HOST_CC:-${THEME[0]}}) rc $rc | $(tail -1 "$OUT/theme.log")"
	[ $rc = 0 ] || { ok=0; grep -m8 "FAIL\|rror" "$OUT/theme.log"; }
	python3 tests/host/uiPrototypeTest.py >"$OUT/proto.log" 2>&1; rc=$?
	echo "uiPrototypeTest rc $rc | $(tail -1 "$OUT/proto.log")"; [ $rc = 0 ] || ok=0
fi
if [ "$FW" != nofw ]; then
	"${MK[@]}" -j"$NCPU" BOARD=gd88 BUILD=build/t-gd88 >"$OUT/gd88.log" 2>&1; rc=$?; w=$(grep -ciw warning "$OUT/gd88.log")
	echo "gd88 rc $rc warnings $w $(grep -E '^ +(FLASH|RAM):' "$OUT/gd88.log" | awk '{printf "%s %s ", $1, $2}')"
	[ $rc = 0 ] && [ "$w" = 0 ] || { ok=0; grep -m12 -i "error\|warning" "$OUT/gd88.log" | cut -c1-220; }
	if [ $rc = 0 ]; then
		"${MK[@]}" BOARD=gd88 BUILD=build/t-gd88 stack-check >"$OUT/gd88-stack.log" 2>&1
		echo "$(grep -E '^  ui ' "$OUT/gd88-stack.log") | $(tail -1 "$OUT/gd88-stack.log")"
	fi
	rm -rf build/t-gd88
fi
[ $ok = 1 ] && echo "CC OK" || { echo "CC FAILED"; exit 1; }
