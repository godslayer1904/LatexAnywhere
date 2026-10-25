// 公式框：光标处弹出的小窗。上半部分是预览，下半部分是编辑框（系统 RichEdit 控件，显示由引擎驱动）。
#pragma once
#include <windows.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>

#include "js_engine.h"
#include "preview.h"

namespace la {

enum class CommitMode { Latex, Keep, Unicode, MathML };

class Box {
public:
	bool Create(HINSTANCE inst, Engine* engine);
	void Toggle();               // Alt+L
	bool Visible() const { return visible_; }
	void SetDark(bool dark);
	HWND Hwnd() const { return hwnd_; }

private:
	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
	static LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	LRESULT OnMessage(UINT, WPARAM, LPARAM);
	LRESULT OnEditMessage(HWND, UINT, WPARAM, LPARAM, bool* handled);

	void Open();
	void Hide();
	void Commit(CommitMode mode);
	void Cancel();
	void Apply(const EditResult& r);
	void Restyle();
	void SyncFromControl();          // 输入法等直接改了控件内容：把差异交给引擎
	void UpdatePreview();
	void Relayout();
	void Paint();
	void CreateDeviceResources();
	void FocusBox();
	int Px(float dip) const { return (int)(dip * scale_ + 0.5f); }

	HINSTANCE inst_ = nullptr;
	HWND hwnd_ = nullptr, edit_ = nullptr;
	Engine* eng_ = nullptr;
	bool visible_ = false, applying_ = false, composing_ = false, moved_ = false, dark_ = true;
	std::u16string mirror_;          // 控件里的文字（换行为 \n）
	int anchor_ = 0, head_ = 0;
	HWND prev_ = nullptr;            // 弹框前的前台窗口
	bool prevNative_ = false;        // 弹框前是不是中文/日文输入
	POINT caret_{0, 0};
	POINT placed_{INT_MIN, INT_MIN};
	float scale_ = 1.f;
	int editH_ = 20;
	double lastToggle_ = 0;
	wchar_t pendingHigh_ = 0;

	Preview preview_;
	bool previewDirty_ = false;
	Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_;
	Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> rt_;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext5> dc_;
	Microsoft::WRL::ComPtr<IDWriteFactory> dw_;
	HFONT font_ = nullptr;
};

}  // namespace la
