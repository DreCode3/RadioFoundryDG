#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# Renders the reference gallery and the profile look (4 variants) for pixel comparison: snapshot.py REPO OUT
import os, re, subprocess, sys, concurrent.futures as cf
repo, out = sys.argv[1], sys.argv[2]
sim = os.path.join(repo, "tests/host/uiSim")
src = open(os.path.join(repo, "tests/host/uiSim.c")).read()
table = src[src.index("gallery[] = {"):]
table = table[:table.index("\n};")]
entries = re.findall(r'\{\s*"([^"]+)",\s*"((?:[^"\\]|\\.)*)"\s*\}', table)
seen = {}
for i, (nm, sc) in enumerate(entries):
    seen[nm] = seen.get(nm, 0) + 1
    if seen[nm] > 1: entries[i] = ("%s__%d" % (nm, seen[nm]), sc)
variants = {  # prefixes from docs/dev/testing.md (profile samples): Commercial Day is the start state
    "commercial_day": "",
    "amateur_day": "ok 8 1 ok down ok long:back ",
    "commercial_night": "ok 8 1 down ok down ok long:back ",
    "amateur_night": "ok 8 1 ok down ok long:back ok 8 1 down ok down ok long:back ",
}
os.makedirs(out, exist_ok=True)
for g in range(10):        # every row of Settings (Settings > group, since the menu tree of step 3), five depths
    for k in (0, 3, 6, 9, 12):
        entries.append(("set_g%02d_d%02d" % (g, k), ("ok 8" + " down" * g + " ok" + " down" * k)))
extra_geoms = ["160x128", "240x320i10x5"]
r = subprocess.run([sim, "--gallery", os.path.join(out, "gallery")], capture_output=True, text=True,
                   env=dict(os.environ, UI_GALLERY_COLORS=""))
open(os.path.join(out, "gallery.log"), "w").write(r.stdout + r.stderr)
jobs = []
for v, pre in variants.items():
    os.makedirs(os.path.join(out, "profile", v), exist_ok=True)
    for name, script in entries:
        jobs.append((v, name, (pre + script).strip()))
def run(job):
    v, name, script = job
    p = subprocess.run([sim, "--geom", "320x240iR10", "--scale", "1", "--script", script,
                        "--out", os.path.join(out, "profile", v, name + ".png")], capture_output=True, text=True)
    return job, p.returncode
for gm in extra_geoms:
    os.makedirs(os.path.join(out, "ref", gm), exist_ok=True)
    for name, script in entries:
        if name.startswith("set_"): jobs.append(("@" + gm, name, script))
def run(job):
    v, name, script = job
    if v.startswith("@"):
        geom, path = v[1:], os.path.join(out, "ref", v[1:], name + ".png")
    else:
        geom, path = "320x240iR10", os.path.join(out, "profile", v, name + ".png")
    p = subprocess.run([sim, "--geom", geom, "--scale", "1", "--script", script, "--out", path],
                       capture_output=True, text=True)
    return job, p.returncode
fails = [j for j, rc in cf.ThreadPoolExecutor(os.cpu_count() or 4).map(run, jobs) if rc]
lay = subprocess.run([sim, "--layout"], capture_output=True, text=True)
open(os.path.join(out, "layout.log"), "w").write(lay.stdout + lay.stderr)
print("gallery rc", r.returncode, "| profile renders", len(jobs), "failed", len(fails), "| layout rc", lay.returncode)
for j in fails[:10]: print("  fail", j)
