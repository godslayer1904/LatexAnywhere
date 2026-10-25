// Obsidian Latex Suite 输入窗：真 CodeMirror 6 + 原版 Latex Suite（运行时从 Obsidian 插件目录读取 main.js / data.json / styles.css，不改一字）
// 与 AHK 的约定（window.chrome.webview）：
//   收：{type:"open", doc, anchor, head}  打开/重置编辑内容
//       {type:"focus"}
//   发：{type:"ready", version} / {type:"commit", text} / {type:"cancel"} / {type:"size", h} / {type:"error", msg}
import * as cmState from "@codemirror/state";
import * as cmView from "@codemirror/view";
import * as cmLanguage from "@codemirror/language";
import * as cmCommands from "@codemirror/commands";
import * as cmAutocomplete from "@codemirror/autocomplete";
import * as cmLint from "@codemirror/lint";
import * as cmSearch from "@codemirror/search";
import * as lezerCommon from "@lezer/common";
import * as lezerHighlight from "@lezer/highlight";
import * as lezerLr from "@lezer/lr";

const PLUGIN = "https://latexsuite.plugin/";
const post = (m) => window.chrome?.webview?.postMessage(m);
const fail = (msg) => { post({ type: "error", msg: String(msg) }); document.getElementById("err").textContent = String(msg); };

// ---------- Obsidian 给 DOM / 内置类型加的扩展 ----------
function applyElOpts(el, o) {
	if (typeof o === "string") o = { cls: o };
	if (!o) return el;
	if (o.cls) el.className = Array.isArray(o.cls) ? o.cls.join(" ") : o.cls;
	if (o.text !== undefined) el.textContent = String(o.text);
	if (o.attr) for (const [k, v] of Object.entries(o.attr)) el.setAttribute(k, v);
	if (o.parent) o.parent.appendChild(el);
	return el;
}
window.createEl = (tag, o, cb) => { const el = applyElOpts(document.createElement(tag), o); cb?.(el); return el; };
window.createDiv = (o, cb) => window.createEl("div", o, cb);
window.createSpan = (o, cb) => window.createEl("span", o, cb);
window.createFragment = (cb) => { const f = document.createDocumentFragment(); cb?.(f); return f; };
for (const P of [Element.prototype, DocumentFragment.prototype]) {
	P.createEl = function (tag, o, cb) { const el = window.createEl(tag, o, cb); this.appendChild(el); return el; };
	P.createDiv = function (o, cb) { return this.createEl("div", o, cb); };
	P.createSpan = function (o, cb) { return this.createEl("span", o, cb); };
	P.empty = function () { while (this.firstChild) this.removeChild(this.firstChild); };
}
Object.assign(Element.prototype, {
	addClass(...c) { this.classList.add(...c.flat()); }, removeClass(...c) { this.classList.remove(...c.flat()); },
	toggleClass(c, v) { this.classList.toggle(c, v); }, hasClass(c) { return this.classList.contains(c); },
	setText(t) { this.textContent = t; }, setAttr(k, v) { this.setAttribute(k, v); }, setCssStyles(s) { Object.assign(this.style, s); },
	show() { this.style.display = ""; }, hide() { this.style.display = "none"; },
});
const defOnce = (o, k, v) => { if (!o[k]) Object.defineProperty(o, k, { value: v }); };
defOnce(String.prototype, "contains", function (s) { return this.includes(s); });
defOnce(Array.prototype, "contains", function (s) { return this.includes(s); });
defOnce(Array.prototype, "first", function () { return this[0]; });
defOnce(Array.prototype, "last", function () { return this[this.length - 1]; });

// ---------- Obsidian 接口替身 ----------
const dummyChain = new Proxy(function () { }, { get: () => dummyChain, apply: () => dummyChain });
class Dummy { constructor() { return new Proxy(this, { get: (t, p) => (p in t ? t[p] : () => dummyChain) }); } }
let pluginData = {};
class Plugin {
	constructor(app, manifest) { this.app = app; this.manifest = manifest; this._ext = []; }
	registerEditorExtension(e) { this._ext.push(e); }
	addCommand() { } addSettingTab() { } register() { } registerEvent() { } addRibbonIcon() { return dummyChain; }
	registerDomEvent() { } registerInterval() { }
	async loadData() { return pluginData; }
	async saveData() { }
}
// 公式渲染：用 Obsidian 安装包里自带的 MathJax（和 Obsidian 同一份）
function renderMath(src, display) {
	const el = document.createElement(display ? "div" : "span");
	try { el.appendChild(window.MathJax.tex2chtml(src, { display: !!display })); }
	catch (e) { el.textContent = src; }
	return el;
}
const obsidian = {
	Plugin,
	PluginSettingTab: class extends Dummy { },
	Notice: class { constructor(msg) { if (msg) console.warn("[Notice]", msg); } hide() { } setMessage() { } },
	Platform: { isMobile: false, isDesktop: true, isDesktopApp: true, isWin: true, isMacOS: false, isLinux: false, isIosApp: false, isAndroidApp: false },
	debounce: (fn, ms) => { let t; const d = (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; d.cancel = () => clearTimeout(t); d.run = () => fn(); return d; },
	addIcon() { }, setIcon() { },
	loadMathJax: async () => { },
	renderMath,
	finishRenderMath: async () => { try { window.MathJax.startup.document.clear(); window.MathJax.startup.document.updateDocument(); } catch { } },
	editorLivePreviewField: cmState.StateField.define({ create: () => true, update: (v) => v }),
	livePreviewState: cmView.ViewPlugin.define(() => ({ mousedown: false })),
	editorInfoField: cmState.StateField.define({ create: () => ({}), update: (v) => v }),
	requireApiVersion: () => true,
	Modal: class extends Dummy { }, Setting: class extends Dummy { }, ButtonComponent: class extends Dummy { },
	ExtraButtonComponent: class extends Dummy { }, AbstractInputSuggest: class extends Dummy { }, Scope: class extends Dummy { },
	MarkdownView: class { }, TFile: class { }, TFolder: class { }, TAbstractFile: class { }, Vault: class { },
	Editor: class { }, EditorSelection: class { },
};
const modules = {
	"@codemirror/state": cmState, "@codemirror/view": cmView, "@codemirror/language": cmLanguage,
	"@codemirror/commands": cmCommands, "@codemirror/autocomplete": cmAutocomplete, "@codemirror/lint": cmLint,
	"@codemirror/search": cmSearch, "@lezer/common": lezerCommon, "@lezer/highlight": lezerHighlight, "@lezer/lr": lezerLr,
	obsidian: new Proxy(obsidian, { get: (t, p) => (p in t ? t[p] : class extends Dummy { }) }),
	fs: {}, path: { join: (...a) => a.join("/"), basename: (p) => p.split(/[\\/]/).pop() }, os: { platform: () => "win32" },
};

const MD_TAGS = Object.fromEntries(["formatting", "formatting-math", "formatting-math-begin", "formatting-math-end", "keyword", "math", "math-block"].map((n) => [n, lezerHighlight.Tag.define()]));
// ---------- 和 Obsidian 一样的 markdown 着色：公式区域标成 math，$ 标成 formatting-math ----------
const hypermd = cmLanguage.StreamLanguage.define({
	name: "hypermd",
	startState: () => ({ math: null }),
	token(stream, st) {
		if (st.math) {
			if (stream.match(st.math)) { st.math = null; return "formatting formatting-math formatting-math-end keyword math"; }
			if (stream.match(/^\\./)) return "math";
			stream.next();
			while (!stream.eol() && stream.peek() !== "$" && stream.peek() !== "\\") stream.next();
			return "math";
		}
		if (stream.match("$$")) { st.math = "$$"; return "formatting formatting-math formatting-math-begin keyword math math-block"; }
		if (stream.match(/^\$(?=[^\s$])/) || stream.match(/^\$(?=\$)/)) { st.math = "$"; return "formatting formatting-math formatting-math-begin keyword math"; }
		stream.next();
		while (!stream.eol() && stream.peek() !== "$") stream.next();
		return null;
	},
	copyState: (s) => ({ math: s.math }),
	tokenTable: MD_TAGS,
});
// 把 token 名对应成 Obsidian 样式表认的类名（cm-math、cm-formatting-math……），颜色就直接来自 app.css
const obsidianClasses = lezerHighlight.tagHighlighter(Object.entries(MD_TAGS).map(([name, tag]) => ({ tag, class: "cm-" + name })));
// StreamLanguage 默认把未知 token 名映射成 class "cm-xxx"，和 Obsidian 的 CSS 类名一致
const mathHighlight = cmView.EditorView.baseTheme({});

let view, plugin;
async function boot() {
	const [code, data, css, manifest] = await Promise.all([
		fetch(PLUGIN + "main.js").then((r) => r.text()),
		fetch(PLUGIN + "data.json").then((r) => r.json()).catch(() => ({})),   // 没有 data.json：用 Latex Suite 默认设置
		fetch(PLUGIN + "styles.css").then((r) => r.text()).catch(() => ""),
		fetch(PLUGIN + "manifest.json").then((r) => r.json()).catch(() => ({ id: "obsidian-latex-suite" })),
	]);
	pluginData = data;
	const st = document.createElement("style"); st.textContent = css; document.head.appendChild(st);
	const mod = { exports: {} };
	new Function("require", "module", "exports", code)((n) => { if (n in modules) return modules[n]; throw new Error("插件 require 了未提供的模块 " + n); }, mod, mod.exports);
	const PluginClass = mod.exports.default || mod.exports;
	const app = {
		vault: { config: {}, adapter: {}, on: () => ({}), getAbstractFileByPath: () => null, configDir: ".obsidian" },
		workspace: { onLayoutReady: (cb) => cb(), updateOptions() { }, on: () => ({}), getActiveViewOfType: () => null },
		isVimEnabled: () => false,
	};
	plugin = new PluginClass(app, manifest);
	await plugin.onload();

	// 输入法自己的键：Enter 上屏（插件没处理时）、Shift+Enter 换行、Esc 取消；优先级最低，插件的矩阵 Enter 等先跑
	const imeKeys = cmState.Prec.lowest(cmView.keymap.of([
		{ key: "Enter", run: () => { if (window.__live) return false; post({ type: "commit", text: view.state.doc.toString() }); return true; } },
		{ key: "Shift-Enter", run: (v) => (window.__live ? false : cmCommands.insertNewline(v)) },
		{ key: "Escape", run: () => { if (window.__live) return false; post({ type: "cancel" }); return true; } },
	]));
	// 原地模式：插件没接手的 Enter 交还给外面的软件（聊天框里照常发送），不让编辑器自己插换行
	const livePass = cmState.Prec.high(cmView.keymap.of([
		{ key: "Enter", run: () => { if (!window.__live) return false; window.__passed = true; return true; } },
		{ key: "Shift-Enter", run: () => { if (!window.__live) return false; window.__passed = true; return true; } },
	]));
	const extensions = [
		hypermd,
		cmLanguage.syntaxHighlighting(obsidianClasses),
		obsidian.editorLivePreviewField, obsidian.editorInfoField, obsidian.livePreviewState,
		cmCommands.history(),
		cmView.drawSelection(),
		cmView.EditorView.lineWrapping,
		cmView.tooltips({ position: "absolute" }),
		plugin._ext,
		cmView.keymap.of([...cmCommands.defaultKeymap, ...cmCommands.historyKeymap]),
		imeKeys,
		livePass,
		cmView.EditorView.updateListener.of((u) => { if (u.geometryChanged || u.docChanged) reportSize(); }),
	];
	window.__extensions = extensions;
	view = new cmView.EditorView({ parent: document.getElementById("editor"), state: cmState.EditorState.create({ doc: "", extensions }) });
	view.focus();
	new MutationObserver(reportSize).observe(document.body, { childList: true, subtree: true, attributes: true });
	post({ type: "ready", version: manifest.version });
}

function reportSize() {
	requestAnimationFrame(() => {
		const ed = document.getElementById("editor").getBoundingClientRect();
		let top = ed.top, bottom = ed.bottom;
		for (const t of document.querySelectorAll(".cm-tooltip")) { const r = t.getBoundingClientRect(); top = Math.min(top, r.top); bottom = Math.max(bottom, r.bottom); }
		post({ type: "size", h: Math.ceil(ed.height), tipAbove: Math.max(0, Math.ceil(ed.top - top)), tipBelow: Math.max(0, Math.ceil(bottom - ed.bottom)) });
	});
}

window.chrome?.webview?.addEventListener("message", (e) => {
	const m = e.data;
	if (!view) return;
	if (m.type === "open") {
		window.__live = false;
		const len = (m.doc || "").length;
		view.setState(cmState.EditorState.create({ doc: m.doc || "", selection: { anchor: Math.min(m.anchor ?? len, len), head: Math.min(m.head ?? len, len) }, extensions: window.__extensions }));
		view.focus();
		reportSize();
	} else if (m.type === "focus") view.focus();
});

// 测试接口：把按键当真实 keydown 派发进编辑器（不需要窗口焦点），没人处理的可打印字符按原生方式插入
window.__reset = (doc = "", a = 0, h = a) => { view.setState(cmState.EditorState.create({ doc, selection: { anchor: a, head: h }, extensions: window.__extensions })); return "ok"; };
// 原地输入用：一个键交给原版插件，返回文档、光标、是否被处理、光标所在公式
function mathAt(doc, pos) {
	const re = /\$\$([\s\S]*?)(\$\$|$)|\$([^$\n]*?)(\$|$)/g;
	let m;
	while ((m = re.exec(doc))) {
		if (m[0] === "") { re.lastIndex++; continue; }
		const disp = m[1] !== undefined, start = m.index + (disp ? 2 : 1), src = disp ? m[1] : m[3];
		if (pos >= start && pos <= start + src.length) return { in: true, display: disp, src, start };
		if (m.index > pos) break;
	}
	return { in: false, display: false, src: "" };
}
window.__key = (o) => {
	window.__live = true, window.__passed = false;
	const before = view.state;
	const ev = new KeyboardEvent("keydown", { key: o.key, shiftKey: !!o.shift, ctrlKey: !!o.ctrl, altKey: !!o.alt, bubbles: true, cancelable: true });
	view.contentDOM.dispatchEvent(ev);
	let handled = (ev.defaultPrevented || view.state !== before) && !window.__passed;
	if (!handled && !window.__passed && o.key.length === 1 && !o.ctrl && !o.alt) {
		const { from, to } = view.state.selection.main;
		const ins = () => view.state.update({ changes: { from, to, insert: o.key }, selection: { anchor: from + 1 }, userEvent: "input.type" });
		if (!view.state.facet(cmView.EditorView.inputHandler).some((h) => h(view, from, to, o.key, ins))) view.dispatch(ins());
		handled = true;
	}
	const s = view.state.selection.main, doc = view.state.doc.toString();
	return JSON.stringify({ doc, anchor: s.anchor, head: s.head, handled, math: mathAt(doc, s.head) });
};
window.__keys = (list) => {
	for (const k of list) {
		const o = typeof k === "string" ? { key: k } : k;
		const before = view.state;
		const ev = new KeyboardEvent("keydown", { key: o.key, shiftKey: !!o.shift, ctrlKey: !!o.ctrl, bubbles: true, cancelable: true });
		view.contentDOM.dispatchEvent(ev);
		if (!ev.defaultPrevented && view.state === before && o.key.length === 1) {
			const { from, to } = view.state.selection.main;
			const ins = () => view.state.update({ changes: { from, to, insert: o.key }, selection: { anchor: from + 1 }, userEvent: "input.type" });
			if (!view.state.facet(cmView.EditorView.inputHandler).some((h) => h(view, from, to, o.key, ins))) view.dispatch(ins());
		}
	}
	const s = view.state.selection.main;
	return JSON.stringify({ doc: view.state.doc.toString(), anchor: s.anchor, head: s.head, tooltip: !!document.querySelector(".cm-tooltip mjx-container, .cm-tooltip .MathJax, .cm-tooltip") });
};

boot().catch((e) => fail(e && e.stack || e));
