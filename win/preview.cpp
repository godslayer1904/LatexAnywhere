#include "preview.h"

#include <shlwapi.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <regex>

#include "util.h"

using Microsoft::WRL::ComPtr;

namespace la {
namespace {

// MathJax 的 ex 与字号的比例（MathJax 默认 exFactor 0.5 下、TeX 字体实际 x 高约 0.431em）
constexpr float kExPerEm = 0.431f;

bool AttrEx(const std::string& svg, const char* name, float* ex) {
	std::string key = std::string(" ") + name + "=\"";
	size_t p = svg.find(key);
	if (p == std::string::npos) return false;
	*ex = std::strtof(svg.c_str() + p + key.size(), nullptr);
	return true;
}

std::string Hex(D2D1_COLOR_F c) {
	char b[16];
	snprintf(b, sizeof b, "#%02x%02x%02x", int(c.r * 255 + .5f), int(c.g * 255 + .5f), int(c.b * 255 + .5f));
	return b;
}

// 把 MathJax 根节点的 ex 尺寸换成像素、去掉 style、把 currentColor 换成实际颜色（Direct2D 的 SVG 不认这些）
std::string PrepareSvg(const std::string& in, float wPx, float hPx, const std::string& color) {
	std::string s = in;
	size_t end = s.find('>');
	if (end == std::string::npos) return s;
	std::string head = s.substr(0, end);
	static const std::regex w(R"( width="[^"]*")"), h(R"( height="[^"]*")"), st(R"( style="[^"]*")");
	char buf[64];
	snprintf(buf, sizeof buf, " width=\"%.2f\"", wPx);
	head = std::regex_replace(head, w, buf, std::regex_constants::format_first_only);
	snprintf(buf, sizeof buf, " height=\"%.2f\"", hPx);
	head = std::regex_replace(head, h, buf, std::regex_constants::format_first_only);
	head = std::regex_replace(head, st, "", std::regex_constants::format_first_only);
	s = head + s.substr(end);
	for (size_t p = 0; (p = s.find("currentColor", p)) != std::string::npos; p += color.size()) s.replace(p, 12, color);
	return s;
}

}  // namespace

void Preview::SetItems(std::vector<PreviewItem> items) {
	items_ = std::move(items);
	placed_.clear();
}

D2D1_SIZE_F Preview::Layout(ID2D1DeviceContext5* dc, IDWriteFactory* dw, float maxWidth, float fontPx, D2D1_COLOR_F color) {
	placed_.clear();
	size_ = {0, 0};
	if (items_.empty()) return size_;
	const float exPx = fontPx * kExPerEm;
	const std::string col = Hex(color);
	ComPtr<IDWriteTextFormat> fmt;
	dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontPx * 0.85f, L"", &fmt);

	for (size_t i = 0; i < items_.size(); i++) {
		const PreviewItem& it = items_[i];
		Placed p;
		p.item = i;
		if (it.math) {
			float wex = 0, hex = 0, va = 0;
			if (!AttrEx(it.svg, "width", &wex) || !AttrEx(it.svg, "height", &hex)) continue;
			size_t vp = it.svg.find("vertical-align:");
			if (vp != std::string::npos) va = std::strtof(it.svg.c_str() + vp + 15, nullptr);   // 负数：基线以下的深度
			p.w = wex * exPx;
			p.h = hex * exPx;
			p.baseline = p.h + va * exPx;   // 从顶部到基线
			std::string svg = PrepareSvg(it.svg, p.w, p.h, col);
			ComPtr<IStream> st;
			st.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), (UINT)svg.size()));
			if (!st || FAILED(dc->CreateSvgDocument(st.Get(), D2D1::SizeF(p.w, p.h), &p.svg))) {
				Log("预览：SVG 解析失败（%zu 字节）", svg.size());
				continue;
			}
		} else {
			std::wstring t = it.text;
			ComPtr<IDWriteTextLayout> lay;
			if (FAILED(dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), fmt.Get(), 10000.f, 10000.f, &lay))) continue;
			DWRITE_TEXT_METRICS m{};
			lay->GetMetrics(&m);
			DWRITE_LINE_METRICS lm{};
			UINT32 n = 0;
			lay->GetLineMetrics(&lm, 1, &n);
			p.text = lay;
			p.w = m.widthIncludingTrailingWhitespace;
			p.h = m.height;
			p.baseline = lm.baseline;
			D2D1_COLOR_F c = it.error ? D2D1::ColorF(0.98f, 0.27f, 0.30f) : color;
			dc->CreateSolidColorBrush(c, &p.brush);
		}
		if (p.w > maxWidth && p.w > 0) p.scale = std::max(0.35f, maxWidth / p.w);   // 单独一段仍比框宽：只把这一段缩小
		placed_.push_back(std::move(p));
	}

	// 按行排：放不下就换行；每行按基线对齐、整体居中
	float y = 0, width = 0;
	size_t i = 0;
	const float gap = fontPx * 0.15f;
	while (i < placed_.size()) {
		size_t j = i;
		float lineW = 0, asc = 0, desc = 0;
		while (j < placed_.size()) {
			Placed& p = placed_[j];
			float w = p.w * p.scale;
			if (j > i && lineW + gap + w > maxWidth) break;
			lineW += (j > i ? gap : 0) + w;
			asc = std::max(asc, p.baseline * p.scale);
			desc = std::max(desc, (p.h - p.baseline) * p.scale);
			j++;
		}
		width = std::max(width, lineW);
		float x = (maxWidth - lineW) / 2;
		for (size_t k = i; k < j; k++) {
			Placed& p = placed_[k];
			p.x = x;
			p.y = y + asc - p.baseline * p.scale;
			x += p.w * p.scale + gap;
		}
		y += asc + desc + fontPx * 0.2f;
		i = j;
	}
	// 实际内容可能比可用宽度窄：整体左移，使内容贴合宽度
	float shift = (maxWidth - width) / 2;
	for (Placed& p : placed_) p.x -= shift;
	size_ = {width, std::max(0.f, y - fontPx * 0.2f)};
	return size_;
}

void Preview::Draw(ID2D1DeviceContext5* dc, D2D1_POINT_2F o) {
	D2D1_MATRIX_3X2_F old;
	dc->GetTransform(&old);
	for (Placed& p : placed_) {
		D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Scale(p.scale, p.scale) * D2D1::Matrix3x2F::Translation(o.x + p.x, o.y + p.y) * old;
		dc->SetTransform(m);
		if (p.svg) dc->DrawSvgDocument(p.svg.Get());
		else if (p.text) dc->DrawTextLayout(D2D1::Point2F(0, 0), p.text.Get(), p.brush.Get());
	}
	dc->SetTransform(old);
}

}  // namespace la
