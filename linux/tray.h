// 托盘图标（Linux）：自己实现 StatusNotifierItem + com.canonical.dbusmenu（GNOME 的 AppIndicator 扩展、KDE 都认这一套）。
// 不用 libayatana-appindicator-glib：它导出的是 org.gtk.Menus，Ubuntu 顶栏扩展不认，菜单打不开（实测）。
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace la {

struct TrayItem {
	std::string label;
	enum Kind { Normal, Check, Radio, Separator } kind = Normal;
	bool on = false;                     // Check / Radio 是否选中
	std::function<void()> activate;
	std::vector<TrayItem> children;      // 非空 = 子菜单
	int id = 0;                          // 内部编号，SetMenu 时分配
};

class Tray {
public:
	// iconDir 下有 iconName.svg；svg 是同一个图标的内容（另外画成位图一并提供，宿主找不到主题图标时用）
	bool Create(const std::string& iconName, const std::string& iconDir, const std::string& svg);
	void SetMenu(std::vector<TrayItem> items);
	void SetTooltip(const std::string& title);
	void Hide();

	struct Impl;
private:
	Impl* p_ = nullptr;
};

}  // namespace la
