#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# Homebrew LLVM's clang as the host tests' CC on macOS, whose AddressSanitizer works there (Apple clang's hangs at
# start-up; fork-notes/local-setup.md, "AddressSanitizer on macOS"):
#   make -C tests/host CC=$PWD/fork-notes/tools/host-cc.sh
# It appends -Wno-unused-but-set-global: LLVM 23 warns (-Wall) about test counters that are set and never read,
# which Apple clang 17 and GCC don't, and ci.sh fails the host step on any warning. After the Makefile's flags, because
# a -Wall later on the line would turn it back on. LLVM's prefix: RF_LLVM, else Homebrew's on Apple silicon or Intel
for d in "${RF_LLVM:-}" /opt/homebrew/opt/llvm /usr/local/opt/llvm; do
	[ -n "$d" ] && [ -x "$d/bin/clang" ] && exec "$d/bin/clang" "$@" -Wno-unused-but-set-global
done
echo "host-cc.sh: no LLVM clang (brew install llvm, or set RF_LLVM)" >&2
exit 127
