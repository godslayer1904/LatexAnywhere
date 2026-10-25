// 与其他程序打交道：按键注入、剪贴板、光标位置、取选区、粘贴上屏、输入法、前台程序。
// 逻辑照 Python 原型（已在 Claude、Word、PowerPoint 里验收）移植。
#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

namespace la {

struct KeyEv { WORD vk; bool up; };
void SendKeys(const std::vector<KeyEv>& seq);   // 一次 SendInput 原子发出

// 剪贴板快照：保存全部可复制的格式（文字、图片、富文本……），恢复时原样放回
class ClipboardSnapshot {
public:
	bool Save();
	bool Restore() const;
private:
	struct Item { UINT fmt; std::vector<unsigned char> data; };
	std::vector<Item> items_;
	bool saved_ = false;
};
bool ClipboardSetText(const std::wstring& s);   // s 为空：清空剪贴板
std::wstring ClipboardGetText(bool* ok);

POINT CaretScreenPos();                      // 光标底部（屏幕物理像素）；拿不到就用鼠标位置
std::wstring GrabSelection();                // Ctrl+C 取选区，之后恢复剪贴板
void PasteText(const std::wstring& s, int settleMs, const std::function<void()>& after);

// 输入法
HWND FocusWindowOf(HWND fg);
bool ImeIsNative(HWND imeWnd);
void ImeSwitchToEnglish(HWND boxWnd, HWND editWnd);   // 只动公式框自己的输入法
void ImeRestoreNative(HWND prevFg);                   // 回到原窗口后切回中文/日文

std::wstring ForegroundExeLower();
RECT WorkAreaAt(POINT pt);
bool KeyIsDown(int vk);

}  // namespace la
