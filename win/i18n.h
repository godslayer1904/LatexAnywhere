// 界面文字（10 种语言）。随系统界面语言自动选择；没有对应语言时用英文。
// 文字里 ‹…› 包住的是按键或代码，说明窗口里画成键帽样式。
#pragma once
#include <cwchar>

namespace la {

struct HelpRow { const wchar_t* keys; const wchar_t* inserts; const wchar_t* forWhat; };

struct Strings {
	const wchar_t* code;          // zh-CN、en …
	bool rtl;
	// 托盘
	const wchar_t* trayHelp;
	const wchar_t* trayAutostart;
	const wchar_t* trayQuit;
	const wchar_t* trayTip;
	const wchar_t* hotkeyBusy;
	// 使用说明
	const wchar_t* helpWindow;
	const wchar_t* title;
	const wchar_t* subtitle;
	const wchar_t* hBasic;
	const wchar_t* basic[4];
	const wchar_t* tipSelection;
	const wchar_t* hInsert;
	const wchar_t* colKey;
	const wchar_t* colInserts;
	const wchar_t* colFor;
	HelpRow rows[6];
	const wchar_t* hIme;
	const wchar_t* ime[2];
	const wchar_t* hBox;
	const wchar_t* box[3];
	const wchar_t* hSafety;
	const wchar_t* safety[3];
	// 首次启动欢迎窗口
	const wchar_t* dontShow;
	const wchar_t* startUsing;
	// 托盘「语言」菜单
	const wchar_t* trayLanguage;
	const wchar_t* followSystem;
	const wchar_t* nativeName;   // 本语言自己的名称（菜单里始终用它显示）
	// Linux 版替换的文字（%1 = 自启菜单名，%2 = 退出菜单名）
	const wchar_t* autostartLinux;
	const wchar_t* trayLinux;
	const wchar_t* helpKeyLinux;
	const wchar_t* basic0Linux;    // 基本用法第 1 条（Linux：框的位置由桌面决定，举 Linux 上的软件）
	const wchar_t* imeLinux;       // 输入法第 1 条（IBus 下用平时的切换键，不是 Shift）   // 说明窗口开头：随时按 Alt+Shift+L 再打开（Linux 没有常驻托盘可点）
};

const Strings& Lang();   // 当前语言
// 语言选择：code 为空 = 跟随系统；选择会保存到注册表
void SetLanguage(const wchar_t* code);
const wchar_t* LanguageSetting();             // 当前设置（空 = 跟随系统）
int LanguageCount();
const Strings& LanguageAt(int i);

}  // namespace la
