// 替换 latex-suite/settings/settings_tab.ts：设置界面在原生版里不存在；isIMESupported 照抄原实现。
import { Platform } from "obsidian";
export class LatexSuiteSettingTab { constructor() {} display() {} hide() {} }
export function isIMESupported() { return Platform.isMobileApp; }
