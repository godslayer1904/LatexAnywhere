// 无网页的 @codemirror/view 替身：只实现 Latex Suite 1.12.8、@codemirror/language、@codemirror/commands 实际用到的部分。
// 文档与选区仍由真正的 @codemirror/state 管理；这里负责插件生命周期、事件分发顺序、按键匹配和逻辑上的光标移动。
// 按键匹配（normalizeKeyName / modifiers / buildKeymap / runHandlers）与事件分发顺序照搬 @codemirror/view 6.43
// （MIT, Copyright (C) 2018-2021 by Marijn Haverbeke <marijnh@gmail.com> and others），保证与原版行为一致。
import {
	Facet, Prec, StateEffect, EditorSelection, RangeValue, RangeSet, Transaction, findClusterBreak, MapMode,
	codePointAt, codePointSize, CharCategory, ChangeSet,
} from "@codemirror/state";

// ---------------- 插件 ----------------
const pluginFacet = Facet.define();

export class ViewPlugin {
	constructor(create, spec) {
		this.create = create;
		this.spec = spec || {};
		this.extension = [pluginFacet.of(this), this.spec.provide ? this.spec.provide(this) : []];
	}
	static define(create, spec) { return new ViewPlugin(create, spec); }
	static fromClass(cls, spec) { return new ViewPlugin((view) => new cls(view), spec); }
	of() { return this; }
}

export function logException(state, exception, context) {
	const handler = state.facet(exceptionSink);
	if (handler.length) handler[0](exception);
	else globalThis.__la_log?.("error", String((context ? context + ": " : "") + (exception && exception.stack || exception)));
}
const exceptionSink = Facet.define();

export class ViewUpdate {
	constructor(view, state, transactions) {
		this.view = view;
		this.state = state;
		this.transactions = transactions;
		this.startState = view._startState;
		this.changes = transactions.reduce((c, tr) => c.compose(tr.changes), ChangeSet.empty(this.startState.doc.length));
		this.flags = 0;
	}
	get viewportChanged() { return false; }
	get viewportMoved() { return false; }
	get heightChanged() { return false; }
	get geometryChanged() { return this.docChanged; }
	get focusChanged() { return false; }
	get docChanged() { return !this.changes.empty; }
	get selectionSet() { return this.transactions.some((tr) => tr.selection); }
	get empty() { return this.transactions.length === 0; }
}

// ---------------- 装饰：照搬 @codemirror/view 6.43（side 常量决定标签/装饰随编辑移动的方式，必须一致） ----------------
const noAttrs = Object.create(null);
function attrsEq(a, b, ignore) {
	if (a == b) return true;
	if (!a) a = noAttrs;
	if (!b) b = noAttrs;
	const keysA = Object.keys(a), keysB = Object.keys(b);
	if (keysA.length != keysB.length) return false;
	for (const key of keysA) if (key != ignore && (keysB.indexOf(key) == -1 || a[key] !== b[key])) return false;
	return true;
}
function combineAttrs(source, target) {
	for (const name in source) {
		if (name == "class" && target.class) target.class += " " + source.class;
		else if (name == "style" && target.style) target.style += ";" + source.style;
		else target[name] = source[name];
	}
	return target;
}
export class WidgetType {
	eq(widget) { return false; }
	updateDOM(dom, view, from) { return false; }
	compare(other) { return this == other || this.constructor == other.constructor && this.eq(other); }
	get estimatedHeight() { return -1; }
	get lineBreaks() { return 0; }
	ignoreEvent(event) { return true; }
	coordsAt(dom, pos, side) { return null; }
	get isHidden() { return false; }
	get editable() { return false; }
	destroy(dom) {}
}
const BlockType = { Text: 0, WidgetBefore: 1, WidgetAfter: 2, WidgetRange: 3 };
export class Decoration extends RangeValue {
	constructor(startSide, endSide, widget, spec) {
		super();
		this.startSide = startSide;
		this.endSide = endSide;
		this.widget = widget;
		this.spec = spec;
	}
	get heightRelevant() { return false; }
	static mark(spec) { return new MarkDecoration(spec); }
	static widget(spec) {
		let side = Math.max(-1e4, Math.min(10000, spec.side || 0)), block = !!spec.block;
		side += (block && !spec.inlineOrder) ? (side > 0 ? 300000000 : -4e8) : (side > 0 ? 100000000 : -1e8);
		return new PointDecoration(spec, side, side, block, spec.widget || null, false);
	}
	static replace(spec) {
		let block = !!spec.block, startSide, endSide;
		if (spec.isBlockGap) { startSide = -5e8; endSide = 400000000; }
		else {
			const { start, end } = getInclusive(spec, block);
			startSide = (start ? (block ? -3e8 : -1) : 500000000) - 1;
			endSide = (end ? (block ? 200000000 : 1) : -6e8) + 1;
		}
		return new PointDecoration(spec, startSide, endSide, block, spec.widget || null, true);
	}
	static line(spec) { return new LineDecoration(spec); }
	static set(of, sort = false) { return RangeSet.of(of, sort); }
	hasHeight() { return this.widget ? this.widget.estimatedHeight > -1 : false; }
}
Decoration.none = RangeSet.empty;
class MarkDecoration extends Decoration {
	constructor(spec) {
		const { start, end } = getInclusive(spec);
		super(start ? -1 : 500000000, end ? 1 : -6e8, null, spec);
		this.tagName = spec.tagName || "span";
		this.attrs = spec.class && spec.attributes ? combineAttrs(spec.attributes, { class: spec.class })
			: spec.class ? { class: spec.class } : spec.attributes || noAttrs;
	}
	eq(other) { return this == other || other instanceof MarkDecoration && this.tagName == other.tagName && attrsEq(this.attrs, other.attrs); }
	range(from, to = from) {
		if (from >= to) throw new RangeError("Mark decorations may not be empty");
		return super.range(from, to);
	}
}
MarkDecoration.prototype.point = false;
class LineDecoration extends Decoration {
	constructor(spec) { super(-2e8, -2e8, null, spec); }
	eq(other) { return other instanceof LineDecoration && this.spec.class == other.spec.class && attrsEq(this.spec.attributes, other.spec.attributes); }
	range(from, to = from) {
		if (to != from) throw new RangeError("Line decoration ranges must be zero-length");
		return super.range(from, to);
	}
}
LineDecoration.prototype.mapMode = MapMode.TrackBefore;
LineDecoration.prototype.point = true;
class PointDecoration extends Decoration {
	constructor(spec, startSide, endSide, block, widget, isReplace) {
		super(startSide, endSide, widget, spec);
		this.block = block;
		this.isReplace = isReplace;
		this.mapMode = !block ? MapMode.TrackDel : startSide <= 0 ? MapMode.TrackBefore : MapMode.TrackAfter;
	}
	get type() { return this.startSide != this.endSide ? BlockType.WidgetRange : this.startSide <= 0 ? BlockType.WidgetBefore : BlockType.WidgetAfter; }
	get heightRelevant() { return this.block || !!this.widget && (this.widget.estimatedHeight >= 5 || this.widget.lineBreaks > 0); }
	eq(other) { return other instanceof PointDecoration && widgetsEq(this.widget, other.widget) && this.block == other.block && this.startSide == other.startSide && this.endSide == other.endSide; }
	range(from, to = from) {
		if (this.isReplace && (from > to || (from == to && this.startSide > 0 && this.endSide <= 0))) throw new RangeError("Invalid range for replacement decoration");
		if (!this.isReplace && to != from) throw new RangeError("Widget decorations can only have zero-length ranges");
		return super.range(from, to);
	}
}
PointDecoration.prototype.point = true;
function getInclusive(spec, block = false) {
	let { inclusiveStart: start, inclusiveEnd: end } = spec;
	if (start == null) start = spec.inclusive;
	if (end == null) end = spec.inclusive;
	return { start: start ?? block, end: end ?? block };
}
function widgetsEq(a, b) { return a == b || !!(a && b && a.compare(b)); }
export class GutterMarker extends RangeValue { eq(o) { return this === o; } }
export const gutter = () => [];
export const Direction = { LTR: 0, RTL: 1 };
export const showTooltip = Facet.define();
export const tooltips = () => [];
export const showPanel = Facet.define();
export const hoverTooltip = () => [];
export const drawSelection = () => [];
export const highlightActiveLine = () => [];
export const dropCursor = () => [];
export const rectangularSelection = () => [];
export const crosshairCursor = () => [];
export const highlightSpecialChars = () => [];
export const lineNumbers = () => [];
export const placeholder = () => [];
export const getTooltip = () => null;
export const getPanel = () => null;

// ---------------- 按键匹配：照搬 @codemirror/view ----------------
const currentPlatform = "win";
function normalizeKeyName(name, platform) {
	const parts = name.split(/-(?!$)/);
	let result = parts[parts.length - 1];
	if (result == "Space") result = " ";
	let alt, ctrl, shift, meta;
	for (let i = 0; i < parts.length - 1; ++i) {
		const mod = parts[i];
		if (/^(cmd|meta|m)$/i.test(mod)) meta = true;
		else if (/^a(lt)?$/i.test(mod)) alt = true;
		else if (/^(c|ctrl|control)$/i.test(mod)) ctrl = true;
		else if (/^s(hift)?$/i.test(mod)) shift = true;
		else if (/^mod$/i.test(mod)) { if (platform == "mac") meta = true; else ctrl = true; }
		else throw new Error("Unrecognized modifier name: " + mod);
	}
	if (alt) result = "Alt-" + result;
	if (ctrl) result = "Ctrl-" + result;
	if (meta) result = "Meta-" + result;
	if (shift) result = "Shift-" + result;
	return result;
}
function modifiers(name, event, shift) {
	if (event.altKey) name = "Alt-" + name;
	if (event.ctrlKey) name = "Ctrl-" + name;
	if (event.metaKey) name = "Meta-" + name;
	if (shift !== false && event.shiftKey) name = "Shift-" + name;
	return name;
}
const Keymaps = new WeakMap();
function getKeymap(state) {
	const bindings = state.facet(keymap);
	let map = Keymaps.get(bindings);
	if (!map) Keymaps.set(bindings, map = buildKeymap(bindings.reduce((a, b) => a.concat(b), [])));
	return map;
}
export function runScopeHandlers(view, event, scope) { return runHandlers(getKeymap(view.state), event, view, scope); }
let currentKeyEvent = null;
function buildKeymap(bindings, platform = currentPlatform) {
	const bound = Object.create(null);
	const add = (scope, key, command, preventDefault, stopPropagation) => {
		const scopeObj = bound[scope] || (bound[scope] = Object.create(null));
		const full = key.split(/ (?!$)/).map((k) => normalizeKeyName(k, platform)).join(" ");
		const binding = scopeObj[full] || (scopeObj[full] = { preventDefault: false, stopPropagation: false, run: scopeObj._any?.run?.slice() || [] });
		if (command) binding.run.push(command);
		if (preventDefault) binding.preventDefault = true;
		if (stopPropagation) binding.stopPropagation = true;
	};
	for (const b of bindings) {
		const scopes = b.scope ? b.scope.split(" ") : ["editor"];
		if (b.any)
			for (const scope of scopes) {
				const scopeObj = bound[scope] || (bound[scope] = Object.create(null));
				if (!scopeObj._any) scopeObj._any = { preventDefault: false, stopPropagation: false, run: [] };
				const { any } = b;
				for (const key in scopeObj) scopeObj[key].run.push((view) => any(view, currentKeyEvent));
			}
		const name = b[platform] || b.key;
		if (!name) continue;
		for (const scope of scopes) {
			add(scope, name, b.run, b.preventDefault, b.stopPropagation);
			if (b.shift) add(scope, "Shift-" + name, b.shift, b.preventDefault, b.stopPropagation);
		}
	}
	return bound;
}
// 键码表（只用于 Ctrl/Alt 组合键回退到基础键名，与 w3c-keyname 一致的子集）
const base = {}, shiftTable = {};
for (let i = 0; i < 10; i++) base[48 + i] = String(i);
for (let i = 65; i <= 90; i++) { base[i] = String.fromCharCode(i + 32); shiftTable[i] = String.fromCharCode(i); }
function runHandlers(map, event, view, scope) {
	currentKeyEvent = event;
	const name = event.key;
	const charCode = codePointAt(name, 0), isChar = codePointSize(charCode) == name.length && name != " ";
	let handled = false, prevented = false;
	const ran = new Set();
	const runFor = (binding) => {
		if (binding) {
			for (const cmd of binding.run)
				if (!ran.has(cmd)) {
					ran.add(cmd);
					if (cmd(view)) return true;
				}
			if (binding.preventDefault) prevented = true;
		}
		return false;
	};
	const scopeObj = map[scope];
	let baseName, shiftName;
	if (scopeObj) {
		if (runFor(scopeObj[modifiers(name, event, !isChar)])) handled = true;
		else if (isChar && (event.altKey || event.metaKey || event.ctrlKey) && !(event.ctrlKey && event.altKey) &&
			(baseName = base[event.keyCode]) && baseName != name) {
			if (runFor(scopeObj[modifiers(baseName, event, true)])) handled = true;
			else if (event.shiftKey && (shiftName = shiftTable[event.keyCode]) != name && shiftName != baseName &&
				runFor(scopeObj[modifiers(shiftName, event, false)])) handled = true;
		} else if (isChar && event.shiftKey && runFor(scopeObj[modifiers(name, event, true)])) handled = true;
		if (!handled && runFor(scopeObj._any)) handled = true;
	}
	if (prevented) handled = true;
	currentKeyEvent = null;
	return handled;
}

// ---------------- 编辑器视图 ----------------
const handleKeyEvents = Prec.default(ViewPlugin.define(() => ({}), { eventHandlers: {
	keydown(event, view) { return runHandlers(getKeymap(view.state), event, view, "editor"); },
} }));
export const keymap = Facet.define({ enables: handleKeyEvents });

export class EditorView {
	constructor(config = {}) {
		this._plugins = [];
		this._startState = config.state;
		this.state = config.state;
		this.composing = false;
		this.compositionStarted = false;
		this.hasFocus = true;
		this.inView = true;
		this.updateState = 0;
		this._dispatchFn = config.dispatch || null;
		this.dispatch = this.dispatch.bind(this);   // 同官方：命令常把 dispatch 解构出来单独调用
		this.dom = fakeElement();
		this.contentDOM = fakeElement();
		this.scrollDOM = fakeElement();
		this.root = { getSelection: () => null, activeElement: null };
		this._initPlugins();
	}
	get viewport() { return { from: 0, to: this.state.doc.length }; }
	get visibleRanges() { return [{ from: 0, to: this.state.doc.length }]; }
	get lineWrapping() { return true; }
	get textDirection() { return Direction.LTR; }
	textDirectionAt() { return Direction.LTR; }
	get defaultCharacterWidth() { return 8; }
	get defaultLineHeight() { return 16; }
	get documentTop() { return 0; }
	get documentPadding() { return { top: 0, bottom: 0 }; }
	get contentHeight() { return 0; }
	get scaleX() { return 1; }
	get scaleY() { return 1; }
	focus() {}
	requestMeasure() {}
	coordsAtPos() { return null; }
	coordsForChar() { return null; }
	posAtCoords() { return null; }
	posAtDOM() { return 0; }
	domAtPos() { return { node: this.contentDOM, offset: 0 }; }
	elementAtHeight() { return this.lineBlockAt(0); }
	lineBlockAtHeight() { return this.lineBlockAt(0); }
	lineBlockAt(pos) { const l = this.state.doc.lineAt(pos); return { from: l.from, to: l.to, length: l.length, top: 0, bottom: 0, height: 0, type: 0 }; }
	get viewportLineBlocks() { const out = []; for (let i = 1; i <= this.state.doc.lines; i++) out.push(this.lineBlockAt(this.state.doc.line(i).from)); return out; }
	bidiSpans(line) { return [{ from: 0, to: line.length, level: 0, dir: Direction.LTR }]; }
	destroy() { for (const p of this._plugins) p.value?.destroy?.(); this._plugins = []; }

	// 与官方一致：插件按需创建——构造期间用 view.plugin() 取别的插件会当场建出来
	plugin(plugin) {
		const p = this._plugins.find((x) => x.spec === plugin);
		return p ? this._ensure(p) : null;
	}
	_ensure(p) {
		if (p.value === undefined) {
			p.value = null;
			try { p.value = p.spec.create(this); } catch (e) { logException(this.state, e, "CodeMirror plugin crashed"); }
		}
		return p.value;
	}
	_initPlugins() {
		this._plugins = this.state.facet(pluginFacet).map((spec) => ({ spec, value: undefined }));
		for (const p of this._plugins) this._ensure(p);
		this._handlers = null;
	}
	_syncPlugins() {
		const specs = this.state.facet(pluginFacet);
		if (specs.length === this._plugins.length && specs.every((s, i) => this._plugins[i].spec === s)) return;
		const old = this._plugins;
		this._plugins = specs.map((spec) => old.find((p) => p.spec === spec) || { spec, value: undefined });
		for (const p of old) if (!this._plugins.includes(p)) p.value?.destroy?.();
		for (const p of this._plugins) this._ensure(p);
		this._handlers = null;
	}

	dispatch(...input) {
		const trs = input.length == 1 && input[0] instanceof Transaction ? input
			: input.length == 1 && Array.isArray(input[0]) ? input[0]
			: [this.state.update(...input)];
		if (this._dispatchFn) { for (const tr of trs) this._dispatchFn(tr, this); return; }
		this.update(trs);
	}

	// 同官方 EditorView.update：事务为空不更新插件；更新监听器在状态复位为空闲之后调用（监听器里可以再派发事务）
	update(transactions) {
		if (this.updateState !== 0) throw new Error("Calls to EditorView.update are not allowed while an update is in progress");
		let state = this.state;
		for (const tr of transactions) {
			if (tr.startState != state) throw new RangeError("Trying to update state with a transaction that doesn't start from the previous state.");
			state = tr.state;
		}
		this._startState = this.state;
		const update = new ViewUpdate(this, state, transactions);
		try {
			this.updateState = 2;
			this.state = state;
			if (!update.empty) {
				this._syncPlugins();
				for (const p of this._plugins) {
					if (p.value && p.value.update) {
						try { p.value.update(update); } catch (e) { logException(state, e, "CodeMirror plugin crashed"); }
					}
				}
			}
		} finally {
			this.updateState = 0;
		}
		if (!update.empty)
			for (const listener of this.state.facet(EditorView.updateListener)) {
				try { listener(update); } catch (e) { logException(this.state, e, "update listener"); }
			}
	}

	setState(newState) {
		this.destroy();
		this._startState = newState;
		this.state = newState;
		this._initPlugins();
	}

	// 事件分发：插件按优先级顺序，任何一个处理器返回 true 或 preventDefault 即停止（同官方 InputState.runHandlers）
	_eventHandlers(type) {
		if (!this._handlers) {
			const handlers = Object.create(null);
			for (const p of this._plugins) {
				const eh = p.spec.spec.eventHandlers;
				if (eh) for (const t in eh) if (eh[t]) {
					const f = eh[t], value = p.value;
					(handlers[t] || (handlers[t] = [])).push((view, event) => {
						try { return f.call(value, event, view); } catch (e) { logException(view.state, e); }
					});
				}
			}
			this._handlers = handlers;
		}
		return this._handlers[type] || [];
	}
	_runEvent(type, event) {
		for (const h of this._eventHandlers(type)) {
			if (event.defaultPrevented) break;
			if (h(this, event)) { event.preventDefault(); break; }
		}
		return event.defaultPrevented;
	}

	// ---------- 逻辑光标移动（无排版信息：按字符、按行） ----------
	moveByChar(start, forward, by) {
		const line = this.state.doc.lineAt(start.head);
		let pos = start.head;
		const step = () => {
			if (forward ? pos == line.to : pos == line.from) return false;
			const off = pos - line.from;
			pos = line.from + findClusterBreak(line.text, off, forward);
			return true;
		};
		if (forward ? pos == line.to : pos == line.from) {
			if (forward ? line.number == this.state.doc.lines : line.number == 1) return EditorSelection.cursor(pos);
			return EditorSelection.cursor(forward ? pos + 1 : pos - 1, forward ? -1 : 1);
		}
		let check = by ? by(line.text.slice(pos - line.from, findClusterBreak(line.text, pos - line.from, forward) + 0)) : null;
		step();
		if (check) {
			for (;;) {
				const save = pos;
				const nextOff = pos - line.from;
				if (forward ? pos == line.to : pos == line.from) break;
				const next = forward ? line.text.slice(nextOff, findClusterBreak(line.text, nextOff, true)) : line.text.slice(findClusterBreak(line.text, nextOff, false), nextOff);
				if (!check(next)) { pos = save; break; }
				step();
			}
		}
		return EditorSelection.cursor(pos, forward ? -1 : 1);
	}
	moveByGroup(start, forward) {
		return this.moveByChar(start, forward, (initial) => {
			const cat = this.state.charCategorizer(start.head);
			let c = cat(initial);
			return (next) => {
				const nc = cat(next);
				if (c == CharCategory.Space && nc != CharCategory.Space) c = nc;
				return c == nc;
			};
		});
	}
	moveToLineBoundary(start, forward) {
		const line = this.state.doc.lineAt(start.head);
		return EditorSelection.cursor(forward ? line.to : line.from, forward ? -1 : 1);
	}
	moveVertically(start, forward, distance) {
		const doc = this.state.doc, line = doc.lineAt(start.head);
		const goal = start.goalColumn ?? (start.head - line.from);
		if (forward ? line.number == doc.lines : line.number == 1) return EditorSelection.cursor(forward ? doc.length : 0);
		const target = doc.line(line.number + (forward ? 1 : -1));
		return EditorSelection.cursor(target.from + Math.min(goal, target.length), 0, undefined, goal);
	}
	visualLineSide(line, end) { return EditorSelection.cursor(end ? line.to : line.from, end ? -1 : 1); }

	static domEventHandlers(handlers) { return ViewPlugin.define(() => ({}), { eventHandlers: handlers }); }
	static domEventObservers() { return []; }
	static theme() { return []; }
	static baseTheme() { return []; }
	static scrollIntoView(pos) { return scrollEffect.of(pos); }
	static findFromDOM() { return null; }
}
const scrollEffect = StateEffect.define();
EditorView.updateListener = Facet.define();
EditorView.inputHandler = Facet.define();
EditorView.decorations = Facet.define();
EditorView.outerDecorations = Facet.define();
EditorView.atomicRanges = Facet.define();
EditorView.bidiIsolatedRanges = Facet.define();
EditorView.scrollMargins = Facet.define();
EditorView.editable = Facet.define({ combine: (v) => (v.length ? v[0] : true) });
EditorView.contentAttributes = Facet.define();
EditorView.editorAttributes = Facet.define();
EditorView.exceptionSink = exceptionSink;
EditorView.perLineTextDirection = Facet.define({ combine: (v) => v.some((x) => x) });
EditorView.clickAddsSelectionRange = Facet.define();
EditorView.dragMovesSelection = Facet.define();
EditorView.mouseSelectionStyle = Facet.define();
EditorView.clipboardInputFilter = Facet.define();
EditorView.clipboardOutputFilter = Facet.define();
EditorView.cspNonce = Facet.define({ combine: (v) => (v.length ? v[0] : "") });
EditorView.darkTheme = Facet.define({ combine: (v) => v.indexOf(true) > -1 });
EditorView.styleModule = Facet.define();
EditorView.announce = StateEffect.define();
EditorView.lineWrapping = [];


function fakeElement() {
	const el = {
		style: {}, classList: { add() {}, remove() {}, contains: () => false, toggle() {} },
		getElementsByClassName: () => [], querySelector: () => null, querySelectorAll: () => [],
		addEventListener() {}, removeEventListener() {}, appendChild() {}, insertBefore() {}, remove() {},
		getBoundingClientRect: () => ({ left: 0, right: 0, top: 0, bottom: 0, width: 0, height: 0 }),
		parentElement: null, nextSibling: null, ownerDocument: null, focus() {}, blur() {},
	};
	return el;
}

export { MapMode };
