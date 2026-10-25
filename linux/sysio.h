// 与系统和其他程序打交道（Linux / Wayland）：虚拟键盘上屏、取选区、输入法、快捷键、开机自启、设置、主题。
// Wayland 不允许程序互相读写和注入按键，各项做法与理由见 docs/linux-design.md。
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace la {

// 虚拟键盘：上屏时向原程序发 Shift+Insert。首选 /dev/uinput（启动时就建好，合成器识别新设备要几百毫秒）；
// 没有权限时退到 GNOME 的 Mutter 远程桌面接口（同样是内核键码）。LA_PASTE_BACKEND=mutter 强制后者（无头测试环境用）
class VirtualKeyboard {
public:
	bool Create();
	bool Ok() const { return fd_ >= 0 || !session_.empty(); }
	const char* Backend() const { return fd_ >= 0 ? "uinput" : !session_.empty() ? "Mutter 远程桌面" : "无"; }
	void Tap(const std::vector<int>& combo);   // 依次按下、倒序抬起（linux/input-event-codes.h 的键码）
	std::string error;
private:
	void Emit(int type, int code, int value);
	void Key(int code, bool down);
	bool CreateMutter();
	int fd_ = -1;
	std::string session_;   // Mutter 远程桌面会话的对象路径
};

// 取选区：AT-SPI（无障碍接口，相当于 Windows 的 UI Automation）。拿不到就返回空，绝不读可能过期的 PRIMARY
void SelectionInit();
std::u16string SelectionGet(bool* fromAtspi);

// 输入法（fcitx5 或 IBus）：进框前问一次，框里切英文，离开时切回
void ImeQuery(std::function<void(bool active)> done);   // 中文等本地输入是否开着
void ImeSetActive(bool on);
void ImeRecover();   // 启动时：上次中途崩溃没切回的，切回去

// GNOME 自定义快捷键：Alt+L 唤起、Alt+Shift+L 使用说明、Super+Alt+L 紧急退出。返回 false 表示 Alt+L 已被别的快捷键占用
bool EnsureShortcuts(const std::string& exe);
void RemoveShortcuts();

bool AutostartOn();
void AutostartSet(bool on, const std::string& exe);

std::string FindObsidianSettings(std::string* from);
bool SystemDark();

}  // namespace la
