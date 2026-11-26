#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# compare.py A B: every PNG under A must exist in B with identical pixels
import os, sys
from PIL import Image, ImageChops
a, b = sys.argv[1], sys.argv[2]
diff, missing, n = [], [], 0
for root, _, files in os.walk(a):
    for f in files:
        if not f.endswith(".png"): continue
        pa = os.path.join(root, f); pb = os.path.join(b, os.path.relpath(pa, a)); n += 1
        if not os.path.exists(pb): missing.append(pb); continue
        ia, ib = Image.open(pa).convert("RGB"), Image.open(pb).convert("RGB")
        if ia.size != ib.size or ImageChops.difference(ia, ib).getbbox(): diff.append(os.path.relpath(pa, a))
print("%d compared, %d differ, %d missing" % (n, len(diff), len(missing)))
for d in diff[:30]: print("  differs", d)
for m in missing[:10]: print("  missing", m)
sys.exit(1 if diff or missing else 0)
