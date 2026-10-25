#!/bin/bash
# Chrome 上屏复现（无头 GNOME，带 XWayland）：chrome_run.sh build/latexanywhere
set -u
EXE=$(readlink -f "${1:-build/latexanywhere}")
HERE=$(cd "$(dirname "$0")" && pwd)
export LA_E2E=/tmp/la-e2e LA_EXE="$EXE"
if [ -z "${LA_E2E_INNER:-}" ]; then
	export LA_E2E_INNER=1
	rm -rf "$LA_E2E"; mkdir -p "$LA_E2E"
	export XDG_RUNTIME_DIR=$LA_E2E/run   # 独立运行时目录，见 run.sh
	mkdir -m 700 -p "$XDG_RUNTIME_DIR"
	exec timeout 300 dbus-run-session -- "$0" "$EXE"
fi
export XDG_CONFIG_HOME=$LA_E2E/config XDG_STATE_HOME=$LA_E2E/state XDG_DATA_HOME=$LA_E2E/data
mkdir -p "$XDG_CONFIG_HOME/LatexAnywhere" "$XDG_STATE_HOME" "$XDG_DATA_HOME"
touch "$XDG_CONFIG_HOME/LatexAnywhere/hide-welcome"
export GSETTINGS_BACKEND=memory LA_NO_SHORTCUTS=1 LA_PASTE_BACKEND=mutter LA_TRACE=1 LANG=zh_CN.UTF-8
unset DISPLAY
# 测试专用的空密钥环（放在临时 XDG_DATA_HOME，已解锁）：否则在线账户服务要求解锁，GNOME 弹出的系统对话框会抢走键盘
eval "$(printf '' | gnome-keyring-daemon --unlock --components=secrets 2>/dev/null)" ; export GNOME_KEYRING_CONTROL
DISP=la-chrome-$$
gnome-shell --headless --wayland --wayland-display=$DISP --virtual-monitor 1600x900 --unsafe-mode ${X11:---no-x11} >"$LA_E2E/shell.log" 2>&1 &
SP=$!
for i in $(seq 1 60); do [ -S "$XDG_RUNTIME_DIR/$DISP" ] && break; sleep 0.5; done
export WAYLAND_DISPLAY=$DISP
sleep 3
export DISPLAY=$(grep -oE "Xwayland on :[0-9]+|display :[0-9]+" "$LA_E2E/shell.log" | grep -oE ":[0-9]+" | head -1)
"$EXE" --foreground >/dev/null 2>&1 &
sleep 3
for m in ${MODES:-wayland x11}; do timeout 60 python3 "$HERE/chrome_probe.py" $m; done
"$EXE" --quit; sleep 1; kill $SP 2>/dev/null; wait 2>/dev/null
echo "DISPLAY=$DISPLAY"
