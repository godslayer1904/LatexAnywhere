// 公式框（Linux / GTK4）：编辑区交给原版 Latex Suite 引擎，上方实时预览，Enter 等上屏。
// 与 Windows 版 win/box.cpp 一一对应；Wayland 的差异：框的位置由桌面决定、上屏用虚拟键盘发 Shift+Insert。
#pragma once
#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "js_engine.h"
#include "sysio.h"

namespace la {

enum class CommitMode { Latex, Keep, Unicode, MathML };

class Box {
public:
	bool Create(GtkApplication* app, Engine* engine, VirtualKeyboard* kb);
	void Toggle();
	void SetDark(bool dark);
	bool Visible() const { return visible_; }

private:
	void Open();
	void Hide();
	void Commit(CommitMode mode);
	void Cancel();
	void Apply(const EditResult& r);
	void Restyle();
	void SyncFromBuffer();
	void UpdatePreview();
	void Relayout();
	void Key(const char* name, bool shift, bool ctrl);
	void Typed(const std::string& utf8, bool shift);
	void UpdateImCursor();
	void ShowPreedit();
	void FinishPaste(std::u16string text, bool restoreIme);
	// 剪贴板快照：弹框拿到焦点时读下全部格式，上屏粘贴后原样放回（同 Windows 版 ClipboardSnapshot）
	void SnapshotClipboard();
	void RestoreClipboard();
	struct ClipItem { std::string mime; GBytes* data; };
	std::vector<ClipItem> clip_;
	unsigned clipGen_ = 0;
	int clipPending_ = 0;
	bool clipEmpty_ = false;
	GObject* curClip_ = nullptr;   // 当前持有的剪贴板内容（LaClip）
	bool keepFormula_ = false;

	// UTF-16 下标（引擎）与字符下标（GtkTextBuffer）互换
	int Cp(int u16) const;
	int U16(int cp) const;
	GtkTextIter Iter(int u16);

	static gboolean OnKey(GtkEventControllerKey*, guint keyval, guint keycode, GdkModifierType state, Box* self);
	static void OnCommit(GtkIMContext*, const char* str, Box* self);
	static void OnPreeditChanged(GtkIMContext*, Box* self);
	static void OnMarkSet(GtkTextBuffer*, const GtkTextIter*, GtkTextMark* mark, Box* self);
	static void OnChanged(GtkTextBuffer*, Box* self);
	static void DrawPreview(GtkDrawingArea*, cairo_t* cr, int w, int h, gpointer self);

	Engine* eng_ = nullptr;
	VirtualKeyboard* kb_ = nullptr;
	GtkWidget* win_ = nullptr;
	GtkWidget* previewScroll_ = nullptr;
	GtkWidget* preview_ = nullptr;
	GtkWidget* sep_ = nullptr;
	GtkWidget* editScroll_ = nullptr;
	GtkWidget* text_ = nullptr;
	GtkTextBuffer* buf_ = nullptr;
	GtkIMContext* im_ = nullptr;
	GtkCssProvider* css_ = nullptr;

	std::u16string mirror_;
	int anchor_ = 0, head_ = 0;
	bool applying_ = false;
	bool visible_ = false;
	bool dark_ = true;
	bool prevIme_ = false;
	bool targetTerminal_ = false;   // 弹框时的目标是终端
	bool wantPrimary_ = false;      // 目标不经无障碍接口可见：框拿到焦点后读主选区
	std::u16string lastPrimary_;    // 上次从主选区带进来的内容
	void ReadPrimary();
	bool lastShift_ = false;
	std::string preedit_;
	double lastToggle_ = -1e9;
	unsigned syncId_ = 0;

	struct Item;   // 预览的一段：公式（SVG）或文字
	std::vector<Item*> items_;
	int pvW_ = 0, pvH_ = 0;
	void ClearItems();
};

}  // namespace la
