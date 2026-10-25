// Obsidian 接口替身：只实现 Latex Suite 运行时用到的部分，设置界面相关的给空壳。
import { StateField } from "@codemirror/state";
import { ViewPlugin } from "@codemirror/view";

const dummyChain = new Proxy(function () {}, { get: () => dummyChain, apply: () => dummyChain, construct: () => dummyChain });
class Dummy { constructor() { return new Proxy(this, { get: (t, p) => (p in t ? t[p] : () => dummyChain) }); } }

export class Plugin {
	constructor(app, manifest) { this.app = app; this.manifest = manifest; this._ext = []; }
	registerEditorExtension(e) { this._ext.push(e); }
	addCommand() {} addSettingTab() {} register() {} registerEvent() {} registerDomEvent() {} registerInterval() {}
	addRibbonIcon() { return dummyChain; }
	async loadData() { return globalThis.__la_settings ?? null; }
	async saveData() {}
}
export class Notice { constructor(msg) { if (msg) globalThis.__la_log?.("notice", String(msg)); } hide() {} setMessage() { return this; } }
export const Platform = { isMobile: false, isDesktop: true, isDesktopApp: true, isWin: true, isMacOS: false, isLinux: false, isIosApp: false, isAndroidApp: false, isPhone: false, isTablet: false };
export function debounce(fn, ms) { let t; const d = (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); return d; }; d.cancel = () => { clearTimeout(t); return d; }; d.run = () => fn(); return d; }
export function addIcon() {}
export function setIcon() {}
export async function loadMathJax() {}
export function renderMath() { return null; }
export async function finishRenderMath() {}
export function requireApiVersion() { return true; }
export const editorLivePreviewField = StateField.define({ create: () => true, update: (v) => v });
export const editorInfoField = StateField.define({ create: () => ({}), update: (v) => v });
export const livePreviewState = ViewPlugin.define(() => ({ mousedown: false }));
export class PluginSettingTab extends Dummy {}
export class Modal extends Dummy {}
export class Setting extends Dummy {}
export class ButtonComponent extends Dummy {}
export class ExtraButtonComponent extends Dummy {}
export class AbstractInputSuggest extends Dummy {}
export class Scope extends Dummy {}
export class MarkdownView {}
export class TFile {}
export class TFolder {}
export class TAbstractFile {}
export class Vault {}
export class Editor {}
export class EditorSelection {}
export class App {}
