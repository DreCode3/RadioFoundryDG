#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
#
# Sets up an Ubuntu (24.04) container or VM for RadioFoundry port work, such as the CS7000 M17 PLUS, with the clean
# room split into two Linux users:
#
#   clean  Claude Code, the RadioFoundry checkout, the toolchains, Renode. Writes code from docs/ and datasheets only.
#   dirty  Codex (run standalone, never through Claude's Codex plugin). Reads stock firmware and writes prose specs into
#          /srv/spec-outbox, which only dirty and root can read. A human reviews a spec there before copying it into
#          the clean checkout's docs/spec/: that copy is the only crossing point.
#
# The OS enforces the barrier (dirty's home and the outbox are mode 700), and clean's Claude Code settings deny reading
# them as a second guard. Run as root, once; it is safe to run again.
#
#   sudo bash setup-linux-dev.sh
#   WITH_CSKY=1 sudo -E bash setup-linux-dev.sh     also build the C-SKY toolchain (about 30-45 min) for full ci.sh
#
# Settings (environment): CLEAN_USER, DIRTY_USER, REPO_URL, UPSTREAM_URL, WITH_CSKY (0), WITH_RENODE (1), NODE_MAJOR (22)
set -euo pipefail

CLEAN_USER=${CLEAN_USER:-clean}
DIRTY_USER=${DIRTY_USER:-dirty}
REPO_URL=${REPO_URL:-https://github.com/DreCode3/RadioFoundryDG.git}
UPSTREAM_URL=${UPSTREAM_URL:-https://github.com/radiofoundry/radiofoundry}
WITH_CSKY=${WITH_CSKY:-0}
WITH_RENODE=${WITH_RENODE:-1}
NODE_MAJOR=${NODE_MAJOR:-22}
OUTBOX=/srv/spec-outbox

say() { printf '\n== %s\n' "$*"; }
warn() { printf 'WARNING: %s\n' "$*" >&2; }
as_user() { local u=$1; shift; sudo -u "$u" -H bash -lc "$*"; }

[ "$(id -u)" = 0 ] || { echo "run as root (sudo bash $0)" >&2; exit 1; }
. /etc/os-release
[ "${ID:-}" = ubuntu ] || warn "written for Ubuntu; this is ${PRETTY_NAME:-unknown}"
ARCH=$(uname -m)

say "Base packages"
export DEBIAN_FRONTEND=noninteractive
apt-get update -q
PKGS=(
	build-essential make gcc g++ clang lld llvm gdb-multiarch pkg-config zlib1g-dev
	git curl ca-certificates gnupg sudo xz-utils unzip jq ripgrep tmux
	python3 python3-venv python3-pip
	dfu-util openocd stlink-tools libusb-1.0-0
)
# tools/build-csky-toolchain.sh's prerequisites (its header lists them)
if [ "$WITH_CSKY" = 1 ]; then PKGS+=(libgmp-dev libmpfr-dev libmpc-dev libisl-dev texinfo bison flex); fi
apt-get install -y -q "${PKGS[@]}"
# GitHub's CLI is in Ubuntu's universe component, which a minimal container image may lack
apt-get install -y -q gh || warn "gh not installed: enable the universe component, or install it from cli.github.com"

say "Node.js $NODE_MAJOR (for Codex)"
if ! command -v node >/dev/null || [ "$(node -p 'process.versions.node.split(".")[0]')" -lt "$NODE_MAJOR" ]; then
	install -d -m 0755 /etc/apt/keyrings
	curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key |
		gpg --dearmor --yes -o /etc/apt/keyrings/nodesource.gpg
	echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_${NODE_MAJOR}.x nodistro main" \
		> /etc/apt/sources.list.d/nodesource.list
	apt-get update -q
	apt-get install -y -q nodejs
fi
node --version

say "Users: $CLEAN_USER (clean side) and $DIRTY_USER (dirty side)"
for u in "$CLEAN_USER" "$DIRTY_USER"; do
	id "$u" >/dev/null 2>&1 || useradd -m -s /bin/bash "$u"
	chmod 700 "/home/$u"
done
install -d -m 0700 -o "$DIRTY_USER" -g "$DIRTY_USER" "$OUTBOX"

say "Claude Code for $CLEAN_USER"
as_user "$CLEAN_USER" 'command -v claude >/dev/null || curl -fsSL https://claude.ai/install.sh | bash'
# A second guard behind the file modes: clean's Claude Code may not read the dirty side ("//" is an absolute path)
as_user "$CLEAN_USER" "mkdir -p ~/.claude && [ -f ~/.claude/settings.json ] || cat > ~/.claude/settings.json <<'EOF'
{
  \"permissions\": {
    \"deny\": [
      \"Read(//home/$DIRTY_USER/**)\",
      \"Read(/$OUTBOX/**)\"
    ]
  }
}
EOF"

say "Codex for $DIRTY_USER (user-local npm prefix, so $CLEAN_USER has no codex)"
as_user "$DIRTY_USER" 'npm config set prefix ~/.npm-global &&
	grep -q npm-global ~/.profile || echo "export PATH=\$HOME/.npm-global/bin:\$PATH" >> ~/.profile'
as_user "$DIRTY_USER" 'command -v codex >/dev/null || npm install -g @openai/codex'
as_user "$DIRTY_USER" "mkdir -p ~/firmware ~/notes && ln -sfn $OUTBOX ~/spec-outbox"

say "RadioFoundry checkout for $CLEAN_USER"
as_user "$CLEAN_USER" "[ -d ~/RadioFoundryDG/.git ] || git clone $REPO_URL ~/RadioFoundryDG
	cd ~/RadioFoundryDG
	git remote get-url upstream >/dev/null 2>&1 || git remote add upstream $UPSTREAM_URL
	git fetch -q upstream
	git submodule update --init --depth 1"

say "Arm GNU toolchain (the pinned version, into ~/opt)"
if [ "$ARCH" = x86_64 ]; then
	as_user "$CLEAN_USER" 'cd ~/RadioFoundryDG && tools/install-arm-toolchain.sh'
else
	warn "tools/install-arm-toolchain.sh pins the x86_64 archive; on $ARCH, download Arm's $ARCH-arm-none-eabi archive of" \
		"the version in platform/stm32f4/platform.toml into ~/opt by hand (check its .sha256asc)"
fi

say "Python venv (~/rf-venv): pytest, pyserial, numpy, reuse, pillow"
as_user "$CLEAN_USER" '[ -x ~/rf-venv/bin/python ] || python3 -m venv ~/rf-venv
	~/rf-venv/bin/pip install -q --upgrade pip pytest pyserial numpy reuse pillow
	grep -q rf-venv ~/.profile || echo "export PATH=\$HOME/rf-venv/bin:\$PATH" >> ~/.profile'

if [ "$WITH_CSKY" = 1 ]; then
	say "C-SKY toolchain (~/toolchains/csky-elf, where platform/hrc7000/platform.mk looks first)"
	as_user "$CLEAN_USER" '[ -x ~/toolchains/csky-elf/bin/csky-elf-gcc ] ||
		(cd ~/RadioFoundryDG && CLEAN=1 tools/build-csky-toolchain.sh)'
fi

if [ "$WITH_RENODE" = 1 ]; then
	say "Renode (STM32H743 emulation) into /opt/renode"
	if ! command -v renode >/dev/null; then
		url=$(curl -fsSL https://api.github.com/repos/renode/renode/releases/latest |
			jq -r '[.assets[].browser_download_url | select(test("linux-portable.*tar.gz$"))] |
				(map(select(test("dotnet"))) + .)[0] // empty')
		if [ -n "$url" ]; then
			tmp=$(mktemp -d)
			curl -fsSL "$url" -o "$tmp/renode.tar.gz"
			rm -rf /opt/renode && mkdir -p /opt/renode
			tar -xzf "$tmp/renode.tar.gz" -C /opt/renode --strip-components=1
			ln -sfn /opt/renode/renode /usr/local/bin/renode
			rm -rf "$tmp"
		else
			warn "no Renode portable release found; install it by hand from https://renode.io"
		fi
	fi
fi

say "Checks"
as_user "$CLEAN_USER" 'export PATH=$HOME/.local/bin:$PATH; claude --version || echo "claude: not on PATH yet (log in again)"
	cd ~/RadioFoundryDG && make -C tests/host -j"$(nproc)" uiSim >/dev/null && echo "uiSim: builds"'
as_user "$DIRTY_USER" 'export PATH=$HOME/.npm-global/bin:$PATH; codex --version || true'
command -v renode >/dev/null && echo "renode: $(command -v renode)"

cat <<EOF

Done. Next steps:
  1. Clean side:  su - $CLEAN_USER, then run 'claude' in ~/RadioFoundryDG and log in. 'gh auth login' for pushes.
  2. Dirty side:  su - $DIRTY_USER, then run 'codex' and log in. Stock firmware goes in ~/firmware, specs in
     ~/spec-outbox (= $OUTBOX). Never start Codex from a Claude session.
  3. Handoff:     as root, review a spec in $OUTBOX for code or code-shaped text, then copy it into
     /home/$CLEAN_USER/RadioFoundryDG/docs/spec/ and chown it to $CLEAN_USER.
  4. A radio over USB (DFU, ST-Link) needs passthrough on the Proxmox host first: allow the USB character devices
     (lxc.cgroup2.devices.allow: c 189:* rwm) and bind-mount /dev/bus/usb into this container.
EOF
