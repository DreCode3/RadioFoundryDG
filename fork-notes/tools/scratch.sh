#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# scratch.sh on|off: a second theme selected at run time, as uncommitted changes in the tree, for the stack-check
# proof (build with -DUI_THEME_SELECT=1, then `make BOARD=<b> stack-check STACK_CHECK_ARGS=-v`). Never commit it.
# Written for step 1's tree; step 2's own second theme replaces it.
set -eu
ROOT=$(git rev-parse --show-toplevel); cd "$ROOT"
KEEP=${TMPDIR:-/tmp}/rf-scratch-ui_screens.c.keep
if [ "${1:-}" = on ]; then
	cp fork-notes/tools/scratch/ui_theme_scratch.c core/ui/ui_theme_scratch.c
	cp core/ui/ui_screens.c "$KEEP"
	python3 - <<'PY'
p = 'core/ui/ui_screens.c'
s = open(p).read()
old = 'void ui_strip_set(bool filled)\n{\n\tui_strip_fill = filled;\n'
assert s.count(old) == 1
s = s.replace(old, 'void ui_theme_scratch_pick(bool on);\nvoid ui_strip_set(bool filled)\n{\n\tui_strip_fill = filled;\n'
              '\tui_theme_scratch_pick(filled);\t// SCRATCH\n')
open(p, 'w').write(s)
PY
else
	rm -f core/ui/ui_theme_scratch.c
	cp "$KEEP" core/ui/ui_screens.c
fi
git status --short
