// LatexAnywhere 编辑引擎：在无网页环境里运行原版 Latex Suite 1.12.8（插件代码一字不改），
// 编辑器组装与原型公式框一致：hypermd 语言名、撤销历史、插件扩展、默认按键（去掉 Enter）、Enter 上屏 / Shift+Enter 换行 / Esc 取消。
// 宿主（C++ 里的 QuickJS，或 Node 测试）通过 globalThis.LatexAnywhereEngine 调用。
import "./shim/globals.js";
import { EditorState, Prec } from "@codemirror/state";
import { EditorView, keymap } from "@codemirror/view";
import { history, defaultKeymap, historyKeymap, insertNewline } from "@codemirror/commands";
import { StreamLanguage } from "@codemirror/language";
import { editorLivePreviewField, editorInfoField, livePreviewState } from "obsidian";
import LatexSuitePlugin from "./latex-suite/main";

const KEYCODES = { Tab: 9, Enter: 13, Backspace: 8, Delete: 46, Escape: 27, ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, Home: 36, End: 35 };

let view = null, extensions = null, committed = false, cancelled = false;

async function init(settings) {
	globalThis.__la_settings = settings ?? null;
	const app = {
		vault: { config: {}, adapter: { basePath: "" }, on: () => ({}), getAbstractFileByPath: () => null, configDir: ".obsidian" },
		workspace: { onLayoutReady: (cb) => cb(), updateOptions() {}, on: () => ({}), getActiveViewOfType: () => null },
		isVimEnabled: () => false,
	};
	const plugin = new LatexSuitePlugin(app, { id: "obsidian-latex-suite", version: "1.12.8" });
	await plugin.onload();
	const hypermd = StreamLanguage.define({ name: "hypermd", token(stream) { stream.skipToEnd(); return null; } });
	extensions = [
		hypermd,
		editorLivePreviewField, editorInfoField, livePreviewState,
		history(),
		plugin._ext,
		keymap.of([...defaultKeymap.filter((b) => b.key !== "Enter"), ...historyKeymap]),
		Prec.lowest(keymap.of([
			{ key: "Enter", run: () => { committed = true; return true; } },
			{ key: "Shift-Enter", run: (v) => insertNewline(v) },
			{ key: "Escape", run: () => { cancelled = true; return true; } },
		])),
	];
	view = new EditorView({ state: EditorState.create({ doc: "", extensions }) });
	return true;
}

function snapshot(handled) {
	const s = view.state, sel = s.selection.main;
	const r = { doc: s.doc.toString(), anchor: sel.anchor, head: sel.head, handled, commit: committed, cancel: cancelled };
	committed = cancelled = false;
	return r;
}

function reset(doc = "", anchor, head) {
	const len = doc.length;
	view.setState(EditorState.create({ doc, selection: { anchor: Math.min(anchor ?? len, len), head: Math.min(head ?? anchor ?? len, len) }, extensions }));
	return snapshot(true);
}

function insertText(text) {
	const { from, to } = view.state.selection.main;
	const defaultInsert = () => view.state.update({ changes: { from, to, insert: text }, selection: { anchor: from + text.length }, userEvent: "input.type", scrollIntoView: true });
	for (const h of view.state.facet(EditorView.inputHandler)) if (h(view, from, to, text, defaultInsert)) return;
	view.dispatch(defaultInsert());
}

function key(name, shift = false, ctrl = false, alt = false) {
	const before = view.state;
	const ev = {
		type: "keydown", key: name, code: KEYCODES[name] ? name : "",
		keyCode: KEYCODES[name] || (name.length === 1 ? name.toUpperCase().charCodeAt(0) : 0),
		shiftKey: shift, ctrlKey: ctrl, altKey: alt, metaKey: false, repeat: false, isComposing: false,
		defaultPrevented: false, preventDefault() { this.defaultPrevented = true; }, stopPropagation() {},
	};
	view._runEvent("keydown", ev);
	let handled = ev.defaultPrevented || view.state !== before;
	if (!handled && name.length === 1 && !ctrl && !alt) { insertText(name); handled = true; }
	return snapshot(handled);
}

function setSelection(anchor, head) { view.dispatch({ selection: { anchor, head: head ?? anchor } }); return snapshot(true); }

// 原生端用：插件给出的标记装饰（括号配色、光标所在括号、$ 高亮）
function marks() {
	const out = [];
	for (const p of view._plugins) {
		const deco = p.value && p.value.decorations;
		if (!deco || typeof deco.between !== "function") continue;
		deco.between(0, view.state.doc.length, (from, to, value) => {
			if (!value.point) out.push([from, to, value.spec.class || "", value.spec.attributes?.style || ""]);
		});
	}
	return out;
}

globalThis.LatexAnywhereEngine = { init, reset, key, insertText, setSelection, marks, get state() { return view.state; } };
