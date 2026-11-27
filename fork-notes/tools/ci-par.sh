#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# The full tools/ci.sh run, with the boards built in parallel (fork-notes/local-setup.md, "Parallel full CI").
#   fork-notes/tools/ci-par.sh [-p LANES] [--board LIST] [--no-checks] [OUT_DIR]
# ci.sh always writes build/ci, so each lane is a worktree of HEAD (tools/worktree.sh, third_party/ read from this
# checkout) that runs `ci.sh --board <b> --steps firmware`, one board after another. The boards come from boards/
# (those with a board.mk and an installed compiler; --board picks some) and wait in one queue, heaviest (most
# configurations) first: a lane takes the next when it is free. Each lane gets JOBS = cores / lanes (JOBS sets it). Meanwhile the main checkout
# runs every other step once (registry, host, bench, python, licenses, similarity; --no-checks leaves them out): the
# host tests with Homebrew LLVM's clang on macOS (host-cc.sh, or RF_HOST_CC), whose AddressSanitizer works there.
# The Python steps use the venv $RF_VENV (default ../rf-venv beside this checkout) when it exists.
# Everything goes in one folder, $RF_WORK (default ../rf-work.noindex): Spotlight skips a folder named *.noindex, and
# one path is all the backup and antivirus exclusions need. The lanes' worktrees are $RF_WORK/par-<short HEAD>-<n>.
# OUT_DIR (default $RF_WORK/ci-<short HEAD>) gets each run's ci.sh output, the size reports, the logs of the
# configurations that failed and summary.txt. Then the worktrees and their build trees go, also on Ctrl-C.
# Exits 1 when anything failed. Builds HEAD: commit first, uncommitted changes are not in the worktrees.
set -u
LANES=4; ONLY=""; CHECKS=1; OUT=""
while [ $# -gt 0 ]; do
	case "$1" in
	-p) LANES=${2:?lanes}; shift ;;
	--board) ONLY=${2:?boards}; shift ;;
	--no-checks) CHECKS=0 ;;
	-h|--help) sed -n '4,15p' "$0"; exit 0 ;;
	-*) echo "ci-par: unknown option $1" >&2; exit 2 ;;
	*) OUT=$1 ;;
	esac
	shift
done
MAIN=$(cd "$(git rev-parse --git-common-dir)/.." && pwd) || exit 2
cd "$MAIN" || exit 2
SHA=$(git rev-parse --short HEAD)
WORK=${RF_WORK:-$MAIN/../rf-work.noindex}
mkdir -p "$WORK" && WORK=$(cd "$WORK" && pwd) || exit 2
OUT=${OUT:-$WORK/ci-$SHA}
mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd) || exit 2
rm -rf "$OUT/logs" "$OUT/.status"; mkdir -p "$OUT/logs" "$OUT/.status"
NCPU=$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
VENV=${RF_VENV:-$MAIN/../rf-venv}
if [ -x "$VENV/bin/python3" ]; then export PATH="$(cd "$VENV" && pwd)/bin:$PATH"; PYNOTE="venv $VENV"; else PYNOTE="no venv"; fi
REG_PY=$(tools/rfoundry_python.sh 2>/dev/null) || { echo "ci-par: needs Python 3.11 or later (tools/rfoundry_python.sh)" >&2; exit 2; }
# The host compiler: Homebrew LLVM's clang on macOS (Apple clang's ASan hangs at start-up there), else cc
HOST_CC=${RF_HOST_CC:-}
[ -z "$HOST_CC" ] && [ "$(uname -s)" = Darwin ] && "$MAIN/fork-notes/tools/host-cc.sh" --version >/dev/null 2>&1 &&
	HOST_CC=$MAIN/fork-notes/tools/host-cc.sh

# The boards: each boards/<b>/board.mk whose compiler is installed. Weight: its configurations, as ci.sh counts them
# (default, each build option value, each switch on by default and not required, the extras, fm-minimal, maximal)
weight() {
	local b=$1 n req kv f
	req=" $(make -s BOARD="$b" print-required 2>/dev/null) "
	n=$((3 + $($REG_PY tools/boards.py builds "$b" | wc -l) + $($REG_PY tools/boards.py builds "$b" --extra | wc -l)))
	for kv in $(make -s BOARD="$b" print-config 2>/dev/null); do
		f=${kv%%=*}
		case "$req" in *" $f "*) continue ;; esac
		[ "${kv#*=}" = y ] && n=$((n + 1))
	done
	echo "$n"
}
BOARDS=(); SKIPPED=()
for bm in boards/*/board.mk; do
	b=$(basename "$(dirname "$bm")")
	[ -n "$ONLY" ] && case ",$ONLY," in *",$b,"*) ;; *) continue ;; esac
	cc=$(make -s BOARD="$b" print-cc 2>/dev/null)
	if [ -z "$cc" ] || ! command -v "$cc" >/dev/null 2>&1; then SKIPPED+=("$b: compiler ${cc:-?} not installed"); continue; fi
	BOARDS+=("$(weight "$b") $b")
done
for b in $(echo "$ONLY" | tr ',' ' '); do [ -f "boards/$b/board.mk" ] || { echo "ci-par: unknown board $b" >&2; exit 2; }; done
[ ${#BOARDS[@]} -gt 0 ] || [ $CHECKS = 1 ] || { echo "ci-par: no board to build" >&2; exit 2; }
[ "$LANES" -gt ${#BOARDS[@]} ] && LANES=${#BOARDS[@]}
# The queue, heaviest first: each lane takes the next board when it is free, so the light boards fill in at the end
QUEUE=$OUT/.status/queue
printf '%s\n' "${BOARDS[@]+"${BOARDS[@]}"}" | sort -rn | awk 'NF == 2 { print $2 }' >"$QUEUE"
# next: the queue's first board, taken under a lock (mkdir is atomic); empty when the queue is
next() {
	local b
	until mkdir "$OUT/.status/lock" 2>/dev/null; do sleep 0.2; done
	b=$(head -1 "$QUEUE"); tail -n +2 "$QUEUE" >"$QUEUE.tmp"; mv "$QUEUE.tmp" "$QUEUE"
	rmdir "$OUT/.status/lock"
	echo "$b"
}
JOBS_LANE=${JOBS:-$(( LANES > 0 ? (NCPU / LANES > 0 ? NCPU / LANES : 1) : NCPU ))}

WTS=(); PIDS=(); MAIN_CI=0; [ -d build/ci ] && MAIN_CI=1
# killtree PID: stop it, end its children, then it
killtree() { local c; kill -STOP "$1" 2>/dev/null; for c in $(pgrep -P "$1"); do killtree "$c"; done; kill -TERM "$1" 2>/dev/null; kill -CONT "$1" 2>/dev/null; }
cleanup() {
	local p w
	for p in ${PIDS[@]+"${PIDS[@]}"}; do killtree "$p"; done
	wait 2>/dev/null
	for w in ${WTS[@]+"${WTS[@]}"}; do
		[ -d "$w" ] || continue
		rm -rf "$w/build"
		git worktree remove --force "$w" 2>/dev/null || rm -rf "$w"
	done
	git worktree prune
	if [ $CHECKS = 1 ]; then
		make -s -C tests/host clean >/dev/null 2>&1
		[ $MAIN_CI = 1 ] || rm -rf build/ci
	fi
	PIDS=(); WTS=()
}
trap 'echo; echo "ci-par: interrupted, removing the worktrees"; cleanup; exit 130' INT TERM

T0=$(date +%s)
echo "ci-par: HEAD $SHA, $LANES lanes x JOBS=$JOBS_LANE, make $( [ "${RF_GNU_MAKE:-1}" = 1 ] && command -v gmake >/dev/null && echo "GNU (gmake)" || echo system), host CC ${HOST_CC:-cc}, $PYNOTE, output $OUT"
echo "ci-par: boards (configurations): $(printf '%s\n' "${BOARDS[@]+"${BOARDS[@]}"}" | sort -rn |
	awk 'NF == 2 { printf "%s%s %s", (NR > 1 ? ", " : ""), $2, $1 }')"
for s in ${SKIPPED[@]+"${SKIPPED[@]}"}; do echo "ci-par: skip $s"; done

# The other steps, once, in this checkout. The host tests build from clean with HOST_CC (make takes binaries from
# another compiler as up to date) and are cleaned again afterwards
if [ $CHECKS = 1 ]; then
	(
		make -s -C tests/host clean >/dev/null 2>&1
		S=host
		[ -n "$HOST_CC" ] && export CC=$HOST_CC
		if [ "$(uname -s)" = Darwin ] && [ -z "$HOST_CC" ]; then
			echo "ci: skip  host tests: no Homebrew LLVM clang (fork-notes/local-setup.md, AddressSanitizer on macOS)"
			S=""
		fi
		tools/ci.sh --steps "registry${S:+,$S},bench,python,licenses,similarity"
		rc=$?
		make -s -C tests/host clean >/dev/null 2>&1
		exit $rc
	) >"$OUT/checks.log" 2>&1 &
	PIDS+=($!)
fi

# The lanes: worktrees made one at a time (git config and the worktree list are shared), then run together
for ((i = 0; i < LANES; i++)); do
	w=$WORK/par-$SHA-$i
	git worktree remove --force "$w" 2>/dev/null; rm -rf "$w"
	WTS+=("$w")
	tools/worktree.sh add "$w" --detach HEAD >"$OUT/logs/worktree-$i.log" 2>&1 || { echo "ci-par: worktree $w failed (see $OUT/logs/worktree-$i.log)"; cleanup; exit 2; }
done
# GNU make 4.4 (Homebrew's, installed as gmake) for the firmware lanes: it hands -flto=auto its job server, so a
# link-time-optimized link uses the lane's JOBS slots instead of one process per core (macOS's make 3.81 has no job
# server for it). Same firmware either way, byte for byte. RF_GNU_MAKE=0 keeps the system make.
GNUBIN=""
if [ "${RF_GNU_MAKE:-1}" = 1 ] && command -v brew >/dev/null && [ -x "$(brew --prefix make 2>/dev/null)/libexec/gnubin/make" ]; then
	GNUBIN="$(brew --prefix make)/libexec/gnubin"
fi
for ((i = 0; i < LANES; i++)); do
	(
		cd "${WTS[i]}" || exit 2
		[ -n "$GNUBIN" ] && PATH="$GNUBIN:$PATH"
		while b=$(next) && [ -n "$b" ]; do
			echo "ci-par: lane $i: $b ($(date '+%H:%M'))"
			t=$(date +%s)
			JOBS=$JOBS_LANE RFOUNDRY_THIRD_PARTY=$MAIN/third_party tools/ci.sh --board "$b" --steps firmware >"$OUT/$b.log" 2>&1
			rc=$?
			cp build/ci/size-report.md "$OUT/$b-size-report.md" 2>/dev/null
			for l in $(grep -oE 'see build/ci/[^ );]+\.log' "$OUT/$b.log" | sed 's/^see //' | sort -u); do cp "$l" "$OUT/logs/" 2>/dev/null; done
			echo "$rc $(( $(date +%s) - t ))" >"$OUT/.status/$b"
			rm -rf build/ci
		done
	) &
	PIDS+=($!)
done
rc_all=0
for p in ${PIDS[@]+"${PIDS[@]}"}; do wait "$p"; done
if [ $CHECKS = 1 ]; then
	for l in $(grep -oE 'see build/ci/[^ );]+\.log' "$OUT/checks.log" | sed 's/^see //' | sort -u); do cp "$l" "$OUT/logs/" 2>/dev/null; done
fi
PIDS=()
cleanup
trap - INT TERM

# One size report, one summary
{
	echo "# Image sizes ($SHA, $(date '+%Y-%m-%d %H:%M'), ci-par.sh)"
	echo
	echo "| Board | Config | Image bytes | Delta | Free | Used |"
	echo "|---|---|---|---|---|---|"
	for f in "$OUT"/*-size-report.md; do [ -f "$f" ] && grep -E '^\| [a-z0-9_]+ \| ' "$f" | grep -v '^| Board '; done
} >"$OUT/size-report.md"
mins() { printf '%dm%02ds' $(($1 / 60)) $(($1 % 60)); }
{
	echo "ci-par: HEAD $SHA, $LANES lanes x JOBS=$JOBS_LANE, wall time $(mins $(( $(date +%s) - T0 )))"
	if [ $CHECKS = 1 ]; then
		if tail -1 "$OUT/checks.log" | grep -q '^ci: all checks passed'; then echo "  ok    checks (registry, host, bench, python, licenses, similarity)"
		else echo "  FAIL  checks (registry, host, bench, python, licenses, similarity): see checks.log"; rc_all=1; fi
		grep -E '^ci: FAIL  ' "$OUT/checks.log" | cut -c1-200 | sed 's/^/        /'
	fi
	for wb in ${BOARDS[@]+"${BOARDS[@]}"}; do
		b=${wb#* }
		if [ ! -f "$OUT/.status/$b" ]; then echo "  FAIL  $b: not run"; rc_all=1; continue; fi
		read -r rc t <"$OUT/.status/$b"
		n=$(grep -cE "^\| $b \| " "$OUT/$b-size-report.md" 2>/dev/null)
		nf=$(grep -cE '^ci: FAIL  ' "$OUT/$b.log")
		if [ "$rc" = 0 ]; then echo "  ok    $b: ${n:-0} configurations, $(mins "$t")"
		else echo "  FAIL  $b: ${n:-0} configurations, $nf failed, $(mins "$t") (see $b.log)"; rc_all=1; fi
		grep -E '^ci: FAIL  ' "$OUT/$b.log" | cut -c1-200 | sed 's/^/        /'
	done
	for s in ${SKIPPED[@]+"${SKIPPED[@]}"}; do echo "  skip  $s"; done
	[ $rc_all = 0 ] && echo "ci-par: all checks passed" || echo "ci-par: FAILED"
} | tee "$OUT/summary.txt"
rm -rf "$OUT/.status"
exit $rc_all
