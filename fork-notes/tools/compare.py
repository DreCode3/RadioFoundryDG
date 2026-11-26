#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# compare.py [--except SUBTREE]... A B: every PNG under A must exist in B with identical pixels. --except leaves out
# a subtree of A (a path relative to A, such as profile) whose pixels a step changes on purpose; everything else must
# still match, so the check stays mechanical and still fails on any other difference
import os, sys
from PIL import Image, ImageChops
args, skip = sys.argv[1:], []
while args and args[0] == "--except":
    if len(args) < 2: sys.exit("compare.py: --except needs a subtree")
    skip.append(os.path.normpath(args[1])); args = args[2:]
if len(args) != 2: sys.exit("usage: compare.py [--except SUBTREE]... A B")
a, b = args
def skipped(rel):
    return any(rel == s or rel.startswith(s + os.sep) for s in skip)
diff, missing, n, left_out = [], [], 0, 0
for root, _, files in os.walk(a):
    for f in files:
        if not f.endswith(".png"): continue
        pa = os.path.join(root, f); rel = os.path.relpath(pa, a)
        if skipped(rel): left_out += 1; continue
        pb = os.path.join(b, rel); n += 1
        if not os.path.exists(pb): missing.append(pb); continue
        ia, ib = Image.open(pa).convert("RGB"), Image.open(pb).convert("RGB")
        if ia.size != ib.size or ImageChops.difference(ia, ib).getbbox(): diff.append(rel)
print("%d compared, %d differ, %d missing" % (n, len(diff), len(missing)) +
      (", %d left out (%s)" % (left_out, ", ".join(skip)) if skip else ""))
for d in diff[:30]: print("  differs", d)
for m in missing[:10]: print("  missing", m)
sys.exit(1 if diff or missing or n == 0 else 0)
