// 公式预览：MathJax 出的 SVG 用 Direct2D 原生渲染，普通文字用 DirectWrite；按行排版、居中、超宽段落单独缩小（照原型）。
#pragma once
#include <d2d1_3.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace la {

struct PreviewItem {
	bool math = false;
	std::string svg;        // math=true：MathJax SVG（UTF-8）
	std::wstring text;      // math=false：普通文字
	bool error = false;
};

class Preview {
public:
	void SetItems(std::vector<PreviewItem> items);
	// 计算排版：maxWidth 为可用宽度（像素），fontPx 为字号（像素）。返回所需尺寸
	D2D1_SIZE_F Layout(ID2D1DeviceContext5* dc, IDWriteFactory* dw, float maxWidth, float fontPx, D2D1_COLOR_F color);
	void Draw(ID2D1DeviceContext5* dc, D2D1_POINT_2F origin);
	bool Empty() const { return items_.empty(); }
	void Reset() { items_.clear(); placed_.clear(); size_ = {0, 0}; }

private:
	struct Placed {
		size_t item;
		Microsoft::WRL::ComPtr<ID2D1SvgDocument> svg;
		Microsoft::WRL::ComPtr<IDWriteTextLayout> text;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
		float w = 0, h = 0, baseline = 0, scale = 1, x = 0, y = 0;
	};
	std::vector<PreviewItem> items_;
	std::vector<Placed> placed_;
	D2D1_SIZE_F size_{0, 0};
};

}  // namespace la
