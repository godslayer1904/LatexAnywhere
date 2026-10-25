#!/bin/bash
# 10 种语言的说明窗口截图（无头 GNOME）：help_shots.sh build/latexanywhere  → /tmp/la-e2e/help_<语言>.png
set -u
EXE=$(readlink -f "${1:-build/latexanywhere}")
export LA_E2E=/tmp/la-e2e
if [ -z "${LA_E2E_INNER:-}" ]; then
	export LA_E2E_INNER=1; rm -rf "$LA_E2E"; mkdir -p "$LA_E2E"
	export XDG_RUNTIME_DIR=$LA_E2E/run; mkdir -m 700 -p "$XDG_RUNTIME_DIR"   # 独立运行时目录，见 run.sh
	exec timeout 300 dbus-run-session -- "$0" "$EXE"
fi
export XDG_CONFIG_HOME=$LA_E2E/config XDG_STATE_HOME=$LA_E2E/state XDG_DATA_HOME=$LA_E2E/data GSETTINGS_BACKEND=memory
mkdir -p "$XDG_CONFIG_HOME" "$XDG_STATE_HOME" "$XDG_DATA_HOME"
unset DISPLAY
eval "$(printf '' | gnome-keyring-daemon --unlock --components=secrets 2>/dev/null)"; export GNOME_KEYRING_CONTROL
gnome-shell --headless --wayland --wayland-display=la-h --virtual-monitor 1280x900 --no-x11 --unsafe-mode >"$LA_E2E/shell.log" 2>&1 &
SP=$!
sleep 5; export WAYLAND_DISPLAY=la-h
for l in zh-CN zh-TW ja en ko vi ru ar es pt-BR; do
	LA_LANG=$l "$EXE" --help >/dev/null 2>&1 &
	P=$!; sleep 2.5
	gdbus call --session --dest org.gnome.Shell.Screenshot --object-path /org/gnome/Shell/Screenshot --method org.gnome.Shell.Screenshot.Screenshot false false "$LA_E2E/help_$l.png" >/dev/null
	kill $P; wait $P 2>/dev/null
done
kill $SP; wait 2>/dev/null; ls "$LA_E2E"/help_*.png | wc -l
