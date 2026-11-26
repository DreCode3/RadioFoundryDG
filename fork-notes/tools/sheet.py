#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# Contact sheet of PNGs with their names: sheet.py OUT.png COLUMNS FILE.png...
import sys
from PIL import Image, ImageDraw
out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = [Image.open(f).convert("RGB") for f in files]
w, h = ims[0].size
S = Image.new("RGB", (cols*(w+8), ((len(ims)+cols-1)//cols)*(h+22)), "white")
d = ImageDraw.Draw(S)
for i, (f, im) in enumerate(zip(files, ims)):
    x, y = (i % cols)*(w+8), (i//cols)*(h+22)
    S.paste(im, (x, y+18)); d.text((x+2, y+3), f.split("/")[-1], fill="black")
S.save(out)
