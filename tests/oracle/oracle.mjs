// 原版 Latex Suite 宿主：在无界面的 DOM 里跑 CodeMirror 6，加载 Obsidian 里装的 Latex Suite main.js（原样，不改一字），
// 通过 stdin/stdout 一行一个 JSON 接收按键、返回文档和光标。
// 协议：
//   {"op":"reset","doc":"...","anchor":0,"head":0}
//   {"op":"key","key":"a"|"Tab"|"Enter"|"Backspace"|"ArrowLeft"...,"shift":false,"ctrl":false,"alt":false}
//   {"op":"setsel","anchor":0,"head":0}
// 回复：{"ok":true,"doc":"...","anchor":n,"head":n,"handled":bool,"math":{"in":bool,"display":bool,"src":"..."}}
//   handled=false 表示这个键插件和编辑器都没处理（文字里的 Tab/Enter 等），由外面原样交给软件。
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import readline from "node:readline";
import { fileURLToPath } from "node:url";
import { JSDOM } from "jsdom";

const here = path.dirname(fileURLToPath(import.meta.url));
const pluginDir = path.join(here, "plugin");
const dataArg = process.argv.find((a) => a.startsWith("--data="));
const dataPath = dataArg ? dataArg.slice(7) : null;

// ---------- 无界面 DOM ----------
const dom = new JSDOM("<!doctype html><html><body></body></html>", { pretendToBeVisual: true });
const w = dom.window;
for (const k of ["window", "document", "navigator", "Node", "HTMLElement", "Element", "Text", "MutationObserver",
	"KeyboardEvent", "Event", "InputEvent", "getComputedStyle", "requestAnimationFrame", "cancelAnimationFrame", "DOMParser"]) {
	try { globalThis[k] = k === "window" ? w : w[k]; } catch { }
}
w.document.createRange = () => {
	const r = new w.Range();
	r.getClientRects = () => [];
	r.getBoundingClientRect = () => ({ left: 0, right: 0, top: 0, bottom: 0, width: 0, height: 0 });
	return r;
};
w.Element.prototype.getClientRects = function () { return []; };
w.Element.prototype.scrollIntoView = function () { };
// 插件用 Blob + URL.createObjectURL + import() 加载 snippet 文本；node 不支持 blob: 地址，换成等价的 data: 地址
globalThis.Blob = class { constructor(parts, opts) { this._s = (parts || []).map(String).join(""); this.type = opts?.type || ""; } };
URL.createObjectURL = (b) => "data:text/javascript;base64," + Buffer.from(b._s, "utf8").toString("base64");
URL.revokeObjectURL = () => { };
// Obsidian 给 DOM 加的辅助函数（createEl/createDiv/createSpan 及元素上的同名方法）
function applyElOpts(el, o) {
	if (typeof o === "string") o = { cls: o };
	if (!o) return el;
	if (o.cls) el.className = Array.isArray(o.cls) ? o.cls.join(" ") : o.cls;
	if (o.text !== undefined) el.textContent = String(o.text);
	if (o.attr) for (const [k, v] of Object.entries(o.attr)) el.setAttribute(k, v);
	if (o.title) el.title = o.title;
	if (o.parent) o.parent.appendChild(el);
	return el;
}
globalThis.createEl = (tag, o, cb) => { const el = applyElOpts(w.document.createElement(tag), o); cb?.(el); return el; };
globalThis.createDiv = (o, cb) => globalThis.createEl("div", o, cb);
globalThis.createSpan = (o, cb) => globalThis.createEl("span", o, cb);
globalThis.createFragment = (cb) => { const f = w.document.createDocumentFragment(); cb?.(f); return f; };
for (const P of [w.Element.prototype, w.DocumentFragment.prototype]) {
	P.createEl = function (tag, o, cb) { const el = globalThis.createEl(tag, o, cb); this.appendChild(el); return el; };
	P.createDiv = function (o, cb) { return this.createEl("div", o, cb); };
	P.createSpan = function (o, cb) { return this.createEl("span", o, cb); };
	P.empty = function () { while (this.firstChild) this.removeChild(this.firstChild); };
}
Object.assign(w.Element.prototype, {
	addClass(...c) { this.classList.add(...c.flat()); }, removeClass(...c) { this.classList.remove(...c.flat()); },
	toggleClass(c, v) { this.classList.toggle(c, v); }, hasClass(c) { return this.classList.contains(c); },
	setText(t) { this.textContent = t; }, setAttr(k, v) { this.setAttribute(k, v); }, setCssStyles(s) { Object.assign(this.style, s); },
	show() { this.style.display = ""; }, hide() { this.style.display = "none"; },
});
// Obsidian 给 String/Array 加的扩展方法（插件里用了 .contains 等）
if (!String.prototype.contains) Object.defineProperty(String.prototype, "contains", { value: function (s) { return this.includes(s); } });
if (!Array.prototype.contains) Object.defineProperty(Array.prototype, "contains", { value: function (s) { return this.includes(s); } });
if (!Array.prototype.first) Object.defineProperty(Array.prototype, "first", { value: function () { return this[0]; } });
if (!Array.prototype.last) Object.defineProperty(Array.prototype, "last", { value: function () { return this[this.length - 1]; } });

const cmState = await import("@codemirror/state");
const cmView = await import("@codemirror/view");
const cmLanguage = await import("@codemirror/language");
const cmCommands = await import("@codemirror/commands");
const modules = {
	"@codemirror/state": cmState,
	"@codemirror/view": cmView,
	"@codemirror/language": cmLanguage,
	"@codemirror/commands": cmCommands,
	"@codemirror/autocomplete": await import("@codemirror/autocomplete"),
	"@codemirror/lint": await import("@codemirror/lint"),
	"@codemirror/search": await import("@codemirror/search"),
	"@lezer/common": await import("@lezer/common"),
	"@lezer/highlight": await import("@lezer/highlight"),
	"@lezer/lr": await import("@lezer/lr"),
	fs, path, os,
};

// ---------- Obsidian 接口替身（只实现插件实际用到的，未知的给空壳） ----------
class Dummy { constructor() { return new Proxy(this, { get: (t, p) => (p in t ? t[p] : () => dummyChain) }); } }
const dummyChain = new Proxy(function () { }, { get: () => dummyChain, apply: () => dummyChain });
class Plugin {
	constructor(app, manifest) { this.app = app; this.manifest = manifest; this._ext = []; }
	registerEditorExtension(e) { this._ext.push(e); }
	addCommand() { } addSettingTab() { } register() { } registerEvent() { } addRibbonIcon() { return dummyChain; }
	registerDomEvent() { } registerInterval() { }
	async loadData() { return dataPath ? JSON.parse(fs.readFileSync(dataPath, "utf8")) : null; }
	async saveData() { }
}
const obsidian = {
	Plugin,
	PluginSettingTab: class extends Dummy { },
	Notice: class { constructor(msg) { if (msg) process.stderr.write("[Notice] " + String(msg) + "\n"); } hide() { } setMessage() { } },
	Platform: { isMobile: false, isDesktop: true, isDesktopApp: true, isWin: true, isMacOS: false, isLinux: false, isIosApp: false, isAndroidApp: false },
	debounce: (fn, ms) => { let t; const d = (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; d.cancel = () => clearTimeout(t); d.run = () => fn(); return d; },
	addIcon() { }, setIcon() { },
	loadMathJax: async () => { },
	renderMath: () => w.document.createElement("span"),
	finishRenderMath: async () => { },
	editorLivePreviewField: cmState.StateField.define({ create: () => true, update: (v) => v }),
	livePreviewState: cmView.ViewPlugin.define(() => ({ mousedown: false })),
	editorInfoField: cmState.StateField.define({ create: () => ({}), update: (v) => v }),
	requireApiVersion: () => true,
	Modal: class extends Dummy { }, Setting: class extends Dummy { }, ButtonComponent: class extends Dummy { },
	ExtraButtonComponent: class extends Dummy { }, AbstractInputSuggest: class extends Dummy { }, Scope: class extends Dummy { },
	MarkdownView: class { }, TFile: class { }, TFolder: class { }, TAbstractFile: class { }, Vault: class { },
	Editor: class { }, EditorSelection: class { },
};
modules.obsidian = new Proxy(obsidian, { get: (t, p) => (p in t ? t[p] : class extends Dummy { }) });

// ---------- 加载原版插件 ----------
const code = fs.readFileSync(path.join(pluginDir, "main.js"), "utf8");
const mod = { exports: {} };
new Function("require", "module", "exports", "window", "document", code)(
	(name) => { if (name in modules) return modules[name]; throw new Error("插件 require 了未提供的模块 " + name); },
	mod, mod.exports, w, w.document);
const PluginClass = mod.exports.default || mod.exports;
const app = {
	vault: { config: {}, adapter: { basePath: path.dirname(path.dirname(path.dirname(pluginDir))) }, on: () => ({}), getAbstractFileByPath: () => null, configDir: ".obsidian" },
	workspace: { onLayoutReady: (cb) => cb(), updateOptions() { }, on: () => ({}), getActiveViewOfType: () => null },
	isVimEnabled: () => false,
};
const manifest = JSON.parse(fs.readFileSync(path.join(pluginDir, "manifest.json"), "utf8"));
const plugin = new PluginClass(app, manifest);
await plugin.onload();

// Obsidian 的编辑器：语言名 hypermd（插件靠它判断是否用自带的 markdown+公式解析器）、默认快捷键、撤销历史
const hypermd = cmLanguage.StreamLanguage.define({ name: "hypermd", token(stream) { stream.skipToEnd(); return null; } });
const parent = w.document.createElement("div");
w.document.body.appendChild(parent);
const extensions = [
	hypermd,
	obsidian.editorLivePreviewField, obsidian.editorInfoField, obsidian.livePreviewState,   // Obsidian 编辑器自带的状态
	cmCommands.history(),
	plugin._ext,
	cmView.keymap.of([...cmCommands.defaultKeymap.filter((b) => b.key !== "Enter"), ...cmCommands.historyKeymap]),
	cmState.Prec.lowest(cmView.keymap.of([
		{ key: "Enter", run: () => { committed = true; return true; } },
		{ key: "Shift-Enter", run: (v) => cmCommands.insertNewline(v) },
		{ key: "Escape", run: () => { cancelled = true; return true; } },
	])),
];
let committed = false, cancelled = false;
const view = new cmView.EditorView({ parent, state: cmState.EditorState.create({ doc: "", extensions }) });

// ---------- 按键 ----------
const CODES = { Tab: "Tab", Enter: "Enter", Backspace: "Backspace", Delete: "Delete", Escape: "Escape",
	ArrowLeft: "ArrowLeft", ArrowRight: "ArrowRight", ArrowUp: "ArrowUp", ArrowDown: "ArrowDown", Home: "Home", End: "End" };
const KEYCODES = { Tab: 9, Enter: 13, Backspace: 8, Delete: 46, Escape: 27, ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, Home: 36, End: 35 };

function pressKey(key, mods = {}) {
	const before = view.state;
	const ev = new w.KeyboardEvent("keydown", {
		key, code: CODES[key] || "", keyCode: KEYCODES[key] || (key.length === 1 ? key.toUpperCase().charCodeAt(0) : 0),
		shiftKey: !!mods.shift, ctrlKey: !!mods.ctrl, altKey: !!mods.alt, bubbles: true, cancelable: true,
	});
	view.contentDOM.dispatchEvent(ev);
	let handled = ev.defaultPrevented || view.state !== before;
	// 没人处理的可打印字符：按浏览器默认行为插入（经 inputHandler，和真实打字同一条路）
	if (!handled && key.length === 1 && !mods.ctrl && !mods.alt) {
		insertText(key);
		handled = true;
	}
	return handled;
}

function insertText(text) {
	const { from, to } = view.state.selection.main;
	const handlers = view.state.facet(cmView.EditorView.inputHandler);
	const defaultInsert = () => view.state.update({ changes: { from, to, insert: text }, selection: { anchor: from + text.length }, userEvent: "input.type", scrollIntoView: true });
	for (const h of handlers) if (h(view, from, to, text, defaultInsert)) return;
	view.dispatch(defaultInsert());
}

// 预览用：光标所在公式（简单扫描 $，只用于头顶预览显示）
function mathAt(doc, pos) {
	const re = /\$\$([\s\S]*?)(\$\$|$)|\$([^$\n]*?)(\$|$)/g;
	let m;
	while ((m = re.exec(doc))) {
		if (m[0] === "") { re.lastIndex++; continue; }
		const disp = m[1] !== undefined;
		const start = m.index + (disp ? 2 : 1);
		const end = start + (disp ? m[1] : m[3]).length;
		if (pos >= start && pos <= end) return { in: true, display: disp, src: disp ? m[1] : m[3] };
		if (m.index > pos) break;
	}
	return { in: false, display: false, src: "" };
}

function snapshot(handled) {
	const s = view.state, sel = s.selection.main, doc = s.doc.toString();
	const r = { ok: true, doc, anchor: sel.anchor, head: sel.head, handled, commit: committed, cancel: cancelled, math: mathAt(doc, sel.head) };
	committed = cancelled = false;
	return r;
}

function handle(msg) {
	switch (msg.op) {
		case "reset": {
			const len = (msg.doc || "").length;
			view.setState(cmState.EditorState.create({ doc: msg.doc || "", selection: { anchor: Math.min(msg.anchor ?? len, len), head: Math.min(msg.head ?? len, len) }, extensions }));
			return snapshot(true);
		}
		case "setsel":
			view.dispatch({ selection: { anchor: msg.anchor, head: msg.head } });
			return snapshot(true);
		case "key":
			return snapshot(pressKey(msg.key, msg));
		case "ping":
			return { ok: true, version: manifest.version };
	}
	return { ok: false, error: "未知 op" };
}

function findPluginDir() {
	const reg = path.join(process.env.APPDATA || "", "obsidian", "obsidian.json");
	const vaults = Object.values(JSON.parse(fs.readFileSync(reg, "utf8")).vaults || {});
	for (const v of vaults) {
		const p = path.join(v.path, ".obsidian", "plugins", "obsidian-latex-suite");
		if (fs.existsSync(path.join(p, "main.js"))) return p;
	}
	throw new Error("找不到 Obsidian 里的 Latex Suite");
}

const reply = (line) => {
	let out;
	try { out = handle(JSON.parse(line)); } catch (e) { out = { ok: false, error: String(e && e.stack || e) }; }
	return JSON.stringify(out) + "\n";
};
const pipeIdx = process.argv.indexOf("--pipe");
if (pipeIdx > 0) {
	// 给 AHK 用：命名管道，一行请求一行回复
	const net = await import("node:net");
	const name = "\\\\.\\pipe\\" + (process.argv[pipeIdx + 1] || "latex-suite-host");
	net.createServer((sock) => {
		sock.setEncoding("utf8");
		let buf = "";
		sock.on("data", (d) => {
			buf += d;
			let i;
			while ((i = buf.indexOf("\n")) >= 0) {
				const line = buf.slice(0, i).replace(/\r$/, ""); buf = buf.slice(i + 1);
				if (line) sock.write(reply(line));
			}
		});
		sock.on("error", () => { });
	}).listen(name);
	// 父进程（LatexAnywhere）退出就跟着退出
	const parentPid = Number(process.argv[process.argv.indexOf("--parent") + 1] || 0);
	if (parentPid) setInterval(() => { try { process.kill(parentPid, 0); } catch { process.exit(0); } }, 2000);
} else {
	const rl = readline.createInterface({ input: process.stdin });
	process.stdout.write(JSON.stringify({ ok: true, ready: true, version: manifest.version }) + "\n");
	rl.on("line", (line) => process.stdout.write(reply(line)));
}
