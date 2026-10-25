// 数学部分（与原型公式框同一套逻辑）：MathJax 3 出预览 SVG 与 Word 用的 MathML，unicodeit 转 Unicode。
// 无网页（liteAdaptor），由 QuickJS 运行，通过 globalThis.LatexAnywhereMath 调用。
import { mathjax } from "mathjax-full/js/mathjax.js";
import { TeX } from "mathjax-full/js/input/tex.js";
import { SVG } from "mathjax-full/js/output/svg.js";
import { liteAdaptor } from "mathjax-full/js/adaptors/liteAdaptor.js";
import { RegisterHTMLHandler } from "mathjax-full/js/handlers/html.js";
import { AllPackages } from "mathjax-full/js/input/tex/AllPackages.js";
import { STATE } from "mathjax-full/js/core/MathItem.js";
import { SerializedMmlVisitor } from "mathjax-full/js/core/MmlTree/SerializedMmlVisitor.js";
import { replace as unicodeitReplace } from "unicodeit";

const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);
const tex = new TeX({ packages: AllPackages.filter((p) => !["bussproofs", "require", "autoload"].includes(p)) });
const svg = new SVG({ fontCache: "none" });          // 字形直接写成路径，不用 <use> 引用（便于原生 SVG 渲染）
const doc = mathjax.document("", { InputJax: tex, OutputJax: svg });
const mmlVisitor = new SerializedMmlVisitor();

// 公式段落：$$…$$ 为显示公式，$…$ 为行内公式（与原型相同的正则）
const MATH_RE = /\$\$([\s\S]+?)\$\$|\$([^$\n]+?)\$/g;

// 预览切段：顶层的 + - = , 处切开（花括号里不切），没有运算符的长串每 24 个字符切一段（照原型）
function pieces(t) {
	const out = []; let cur = "", depth = 0, run = 0;
	for (let i = 0; i < t.length; i++) {
		const ch = t[i];
		if (ch === "\\") { const mm = /^\\([A-Za-z]+|.)/.exec(t.slice(i)); cur += mm[0]; i += mm[0].length - 1; run = 0; continue; }
		cur += ch;
		if (ch === "{") depth++; else if (ch === "}") depth--;
		run = /[A-Za-z0-9]/.test(ch) ? run + 1 : 0;
		if (depth === 0 && (/[+\-=,]/.test(ch) || run >= 24)) { out.push(cur); cur = ""; run = 0; }
	}
	if (cur) out.push(cur);
	return out;
}

function renderSvg(src, display) {
	const node = doc.convert(src, { display: !!display });
	const s = node.children && node.children[0];   // mjx-container 里的 <svg>
	// MathJax 出错时给的是带 data-mjx-error 的 merror；原样渲染（红字显示错误位置）
	return adaptor.outerHTML(s || node);
}

// 预览：返回 [{t:"text", s}, {t:"math", svg, src, err}]，原生端依次排版
function preview(source) {
	const items = [];
	if (!source.replace(/\$/g, "").trim()) return items;
	let pos = 0, m;
	MATH_RE.lastIndex = 0;
	while ((m = MATH_RE.exec(source))) {
		if (m.index > pos) items.push({ t: "text", s: source.slice(pos, m.index) });
		const display = m[1] !== undefined;
		const body = display ? m[1] : m[2];
		for (const p of display ? [body] : pieces(body)) {
			try { items.push({ t: "math", svg: renderSvg(p, false), src: p }); }
			catch (e) { items.push({ t: "text", s: p, err: true }); }
		}
		pos = m.index + m[0].length;
	}
	if (pos < source.length && source.slice(pos).trim()) items.push({ t: "text", s: source.slice(pos) });
	doc.clear();
	return items;
}

// Ctrl+Shift+Enter：每个公式段转 MathML（照原型：没有 $ 就把全文当一个行内公式）
function mathml(text) {
	const parts = [];
	text.replace(MATH_RE, (m0, d, i) => { parts.push({ tex: d !== undefined ? d : i, display: d !== undefined }); return m0; });
	if (!parts.length) parts.push({ tex: text, display: false });
	const out = parts.map((p) => {
		try { return mmlVisitor.visitTree(doc.convert(p.tex, { display: p.display, end: STATE.CONVERT })); }
		catch (e) { return p.tex; }
	}).join("\n");
	doc.clear();
	return out;
}

// Ctrl+Enter：转 Unicode（unicodeit 不管分式：先按括号配对把 \frac{a}{b} 换成 a/b，嵌套也行；照原型）
function toUnicode(t) {
	const group = (s, i) => {
		let depth = 0;
		for (let j = i; j < s.length; j++) {
			if (s[j] === "{") depth++;
			else if (s[j] === "}") { depth--; if (depth === 0) return [s.slice(i + 1, j), j + 1]; }
		}
		return [s.slice(i + 1), s.length];
	};
	const conv = (x) => unicodeitReplace(x).replace(/[{}]/g, "");
	const wrap = (x) => ([...conv(x)].length <= 1 ? x : "(" + x + ")");
	const fracs = (s) => {
		let out = "", i = 0;
		outer: while (i < s.length) {
			for (const cmd of ["\\dfrac", "\\tfrac", "\\frac"]) {
				if (s.startsWith(cmd, i) && s[i + cmd.length] === "{") {
					let [num, k] = group(s, i + cmd.length);
					let den = "";
					if (s[k] === "{") [den, k] = group(s, k);
					out += wrap(fracs(num)) + "/" + wrap(fracs(den));
					i = k;
					continue outer;
				}
			}
			out += s[i++];
		}
		return out;
	};
	// 数学函数名原样保留（unicodeit 会把 \ln 的 \l 当成波兰字母 ł、把 \sin 的 \s… 拆错）
	// 函数名后放一个占位符（U+E000，私用区，unicodeit 不认识也不会动），转换完再换成空格
	const OPS = /\\(arccos|arcsin|arctan|arg|cosh|cos|coth|cot|csc|deg|det|dim|exp|gcd|hom|inf|ker|lg|liminf|limsup|lim|ln|log|max|min|Pr|sec|sinh|sin|sup|tanh|tan)(?![A-Za-z])/g;
	// 上下标组 _{…} ^{…}：每个字符都有对应的上下标字符才转成上下标；否则整组写成 _(…) / ^(…)，
	// 避免 unicodeit 对没有上下标形式的字符（空格、→ 等）留下半截下划线（_{n \to \infty} → ₙ_ _→_ _∞）
	const scripts = (s) => {
		let res = "", i = 0;
		while (i < s.length) {
			const c = s[i];
			if ((c === "_" || c === "^") && s[i + 1] === "{" && s[i - 1] !== "\\") {
				let [inner, k] = group(s, i + 1);
				const conv = unicodeitReplace(scripts(inner).replace(OPS, (m, name) => name + "")).replace(//g, "").replace(/[{}\s]/g, "");
				const chars = [...conv];
				const mapped = chars.map((ch) => { const r = unicodeitReplace(c + "{" + ch + "}"); return r[0] === c ? null : r; });
				res += chars.length && mapped.every((m) => m) ? mapped.join("") : (c === "_" ? "" : "") + "(" + conv + ")";
				i = k;
				continue;
			}
			res += c;
			i++;
		}
		return res;
	};
	const out = unicodeitReplace(scripts(fracs(t)).replace(OPS, (m, name) => name + "")).replace(//g, "_").replace(//g, "^");
	// 函数名后紧跟字母数字才需要空格隔开（ln x、sin x），后面是上下标、括号就直接相连（sin²x、exp(x)）
	return out.replace(/ *(?=[A-Za-z0-9Ͱ-Ͽ])/g, " ").replace(/ */g, "").replace(/[{}]/g, "");
}

// 宿主用：记录之间用 、字段之间用  分隔（m=公式 SVG，t=文字，e=出错的文字）
function previewPacked(source) {
	return preview(source).map((it) => (it.t === "math" ? "m" + it.svg : (it.err ? "e" : "t") + it.s)).join("");
}

globalThis.LatexAnywhereMath = { preview, previewPacked, mathml, toUnicode, svg: renderSvg };
