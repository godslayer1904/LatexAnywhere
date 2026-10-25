// 界面文字（10 种语言）。随系统界面语言自动选择；没有对应语言时用英文。
// 文字里 ‹…› 包住的是按键或代码，说明窗口里画成键帽样式。
#pragma once
#include <windows.h>

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
};

const Strings& Lang();   // 当前语言

}  // namespace la
