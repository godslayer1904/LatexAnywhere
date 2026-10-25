#!/bin/bash
# Linux 端到端测试：在独立的会话总线里起无头 GNOME Shell（不碰真实桌面），跑 e2e.py。
# 用法：tests/e2e-linux/run.sh build/latexanywhere   结果在 /tmp/la-e2e/out.txt
# 全程带超时：整轮最多 20 分钟（timeout 由调用方 wrap），各步骤内部也有上限。
set -u
EXE=$(readlink -f "${1:-build/latexanywhere}")
HERE=$(cd "$(dirname "$0")" && pwd)
export LA_E2E=/tmp/la-e2e LA_EXE="$EXE"
if [ -z "${LA_E2E_INNER:-}" ]; then
	export LA_E2E_INNER=1
	rm -rf "$LA_E2E"; mkdir -p "$LA_E2E"
	# 独立的运行时目录，而且要在起会话总线之前设好：总线按需拉起的服务（无障碍总线等）用的是它启动时的环境。
	# 绝不能和真实会话共用 /run/user/<uid>：测试里的无障碍总线退出时会删掉同名连接文件，真实桌面的 AT-SPI 就断了（2026-10-25 踩过）
	export XDG_RUNTIME_DIR=$LA_E2E/run
	mkdir -m 700 -p "$XDG_RUNTIME_DIR"
	exec timeout 1200 dbus-run-session -- "$0" "$EXE"
fi
export XDG_CONFIG_HOME=$LA_E2E/config XDG_STATE_HOME=$LA_E2E/state XDG_DATA_HOME=$LA_E2E/data
mkdir -p "$XDG_CONFIG_HOME" "$XDG_STATE_HOME" "$XDG_DATA_HOME"
# 与 install.sh 相同的 .desktop（文件名 = 程序标识），在 GNOME 启动前放好，它启动时扫描
mkdir -p "$XDG_DATA_HOME/applications"
printf '[Desktop Entry]
Type=Application
Name=LatexAnywhere
Exec=%s
Icon=latexanywhere
StartupWMClass=io.github.godslayer1904.LatexAnywhere
' "$EXE" >"$XDG_DATA_HOME/applications/io.github.godslayer1904.LatexAnywhere.desktop"
# 与 Windows 测试相同：放入用户的 Latex Suite 设置（个人文件，不进仓库；没有就用默认设置）
[ -f "$HOME/.la-e2e-settings.json" ] && mkdir -p "$XDG_CONFIG_HOME/LatexAnywhere" && cp "$HOME/.la-e2e-settings.json" "$XDG_CONFIG_HOME/LatexAnywhere/settings.json"
export GSETTINGS_BACKEND=memory          # 不改真实的 GNOME 设置（快捷键等）
export LA_NO_SHORTCUTS=1 LA_PASTE_BACKEND=mutter LA_TRACE=1   # 无头环境：用 Mutter 接口发键，不碰真实键盘（uinput 会进真实桌面）
export LANG=zh_CN.UTF-8 LANGUAGE=zh_CN
export GTK_A11Y=atspi NO_AT_BRIDGE=0
unset DISPLAY
# 测试专用的空密钥环（放在临时 XDG_DATA_HOME，已解锁）：否则在线账户服务要求解锁，GNOME 弹出的系统对话框会抢走键盘
eval "$(printf '' | gnome-keyring-daemon --unlock --components=secrets 2>/dev/null)" ; export GNOME_KEYRING_CONTROL
DISP=la-e2e-$$
gnome-shell --headless --wayland --wayland-display=$DISP --virtual-monitor 1600x900 --no-x11 --unsafe-mode >"$LA_E2E/shell.log" 2>&1 &
SHELL_PID=$!
for i in $(seq 1 60); do [ -S "$XDG_RUNTIME_DIR/$DISP" ] && break; sleep 0.5; done
if [ ! -S "$XDG_RUNTIME_DIR/$DISP" ]; then echo "无头 GNOME 没起来"; tail -20 "$LA_E2E/shell.log"; kill $SHELL_PID; exit 1; fi
export WAYLAND_DISPLAY=$DISP
sleep 2
python3 "$HERE/sni_watcher.py" >"$LA_E2E/sni.log" 2>&1 &
SNI_PID=$!
python3 "$HERE/target.py" >"$LA_E2E/target.log" 2>&1 &
TARGET_PID=$!
sleep 1.5
timeout 900 python3 "$HERE/e2e.py"
RC=$?
"$EXE" --quit 2>/dev/null
sleep 1
pkill -f -x "$EXE --foreground" 2>/dev/null; pkill -f "$EXE --worker" 2>/dev/null
kill $TARGET_PID $SNI_PID $SHELL_PID 2>/dev/null
wait 2>/dev/null
cp "$XDG_STATE_HOME/LatexAnywhere/log.txt" "$LA_E2E/app.log" 2>/dev/null
exit $RC
