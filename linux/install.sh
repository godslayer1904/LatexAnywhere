#!/bin/bash
# LatexAnywhere Linux 一键安装（x86_64 / aarch64 自动识别）
#   curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
#   卸载：bash install.sh --uninstall
# 装到当前用户（~/.local），只有两处需要 sudo：缺运行库时装库、加 uinput 规则（让上屏用的虚拟键盘不需要 root）。
set -euo pipefail
REPO=godslayer1904/LatexAnywhere
BIN=$HOME/.local/bin
APPS=$HOME/.local/share/applications
ICONS=$HOME/.local/share/icons/hicolor/scalable/apps
RULE=/etc/udev/rules.d/60-latexanywhere-uinput.rules
say() { printf '\033[1;35m==>\033[0m %s\n' "$*"; }

if [ "${1:-}" = "--uninstall" ]; then
	"$BIN/latexanywhere" --quit 2>/dev/null || true
	"$BIN/latexanywhere" --uninstall-cleanup 2>/dev/null || true   # 去掉 GNOME 快捷键和自启项
	rm -f "$BIN/latexanywhere" "$APPS/latexanywhere.desktop" "$ICONS/latexanywhere.svg"
	[ -f "$RULE" ] && sudo rm -f "$RULE" && sudo udevadm control --reload || true
	say "已卸载（设置留在 ~/.config/LatexAnywhere，不要可以手动删）"
	exit 0
fi

case "$(uname -m)" in
	x86_64|amd64) ARCH=x86_64 ;;
	aarch64|arm64) ARCH=aarch64 ;;
	*) echo "不支持的架构：$(uname -m)"; exit 1 ;;
esac

# 运行库：GTK4（4.12 以上）、librsvg、AT-SPI
need=()
ldconfig -p | grep -q 'libgtk-4.so.1' || need+=(gtk4)
ldconfig -p | grep -q 'librsvg-2.so.2' || need+=(rsvg)
ldconfig -p | grep -q 'libatspi.so.0' || need+=(atspi)
if [ ${#need[@]} -gt 0 ]; then
	say "安装运行库：${need[*]}"
	if command -v apt-get >/dev/null; then sudo apt-get install -y libgtk-4-1 librsvg2-2 libatspi2.0-0
	elif command -v dnf >/dev/null; then sudo dnf install -y gtk4 librsvg2 at-spi2-core
	elif command -v pacman >/dev/null; then sudo pacman -S --needed --noconfirm gtk4 librsvg at-spi2-core
	elif command -v zypper >/dev/null; then sudo zypper install -y libgtk-4-1 librsvg-2-2 libatspi0
	else echo "请先手动安装 GTK4、librsvg、at-spi2"; exit 1; fi
fi

# 程序本体：和脚本放在一起就用本地的（解压的发行包），否则从 GitHub 最新发行版下载
mkdir -p "$BIN" "$APPS" "$ICONS"
HERE=$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || echo .)
if [ -x "$HERE/latexanywhere" ]; then
	install -m 755 "$HERE/latexanywhere" "$BIN/latexanywhere.new"
else
	TMP=$(mktemp -d)
	trap 'rm -rf "$TMP"' EXIT
	say "下载 latexanywhere-linux-$ARCH"
	curl -fL --connect-timeout 10 --retry 3 -o "$TMP/la.tar.gz" "https://github.com/$REPO/releases/latest/download/latexanywhere-linux-$ARCH.tar.gz"
	tar -xzf "$TMP/la.tar.gz" -C "$TMP"
	install -m 755 "$TMP/latexanywhere" "$BIN/latexanywhere.new"
fi
"$BIN/latexanywhere" --quit 2>/dev/null || true   # 升级：先停掉旧的
mv -f "$BIN/latexanywhere.new" "$BIN/latexanywhere"

cat >"$APPS/latexanywhere.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=LatexAnywhere
Comment=Alt+L: type LaTeX anywhere, the Obsidian Latex Suite way
Exec="$BIN/latexanywhere"
Icon=latexanywhere
Categories=Utility;Education;
EOF

# 上屏用的虚拟键盘：/dev/uinput 默认只有 root 能写。uaccess 让当前坐在电脑前的用户可写（logind 管理，切换用户即收回）
if [ ! -w /dev/uinput ] && [ ! -f "$RULE" ]; then
	say "添加 uinput 规则（需要 sudo；不加的话 GNOME 下仍可用，其他桌面需手动 Ctrl+V）"
	if echo 'KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"' | sudo tee "$RULE" >/dev/null; then
		sudo udevadm control --reload && sudo udevadm trigger --name-match=uinput || true
	fi
fi

# 开机自启 + 立即启动（首次启动会弹使用说明）
mkdir -p "$HOME/.config/autostart"
cp "$APPS/latexanywhere.desktop" "$HOME/.config/autostart/latexanywhere.desktop"
echo "X-GNOME-Autostart-enabled=true" >>"$HOME/.config/autostart/latexanywhere.desktop"
nohup "$BIN/latexanywhere" >/dev/null 2>&1 &
say "装好了：在任意输入框按 Alt+L 打公式（托盘图标在顶栏右侧）"
