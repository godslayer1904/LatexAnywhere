// 打包引擎：原版 Latex Suite 1.12.8 + @codemirror/state 等，@codemirror/view 与 obsidian 换成无网页替身。
// 产物 dist/engine.js（单文件 IIFE，供 QuickJS 与 Node 测试加载）。
import esbuild from "esbuild";
import inlineImportPlugin from "esbuild-plugin-inline-import";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const minify = process.argv.includes("--minify");

// 只有设置界面（片段编辑器、文件监视）才用到的包：换成空壳，运行逻辑不经过它们
const STUBS = /^(@codemirror\/(autocomplete|lint|search|lang-javascript)|@popperjs\/core|fs|path|os|electron)$/;
const stubPlugin = {
	name: "stub",
	setup(b) {
		b.onResolve({ filter: STUBS }, (a) => ({ path: a.path, namespace: "stub" }));
		b.onResolve({ filter: /settings[\/]settings_tab$/ }, () => ({ path: path.join(here, "shim", "settings_tab.js") }));
		b.onLoad({ filter: /.*/, namespace: "stub" }, () => ({
			contents: "const d = new Proxy(function () {}, { get: (t, p) => (p === '__esModule' ? true : d), apply: () => d, construct: () => d }); module.exports = d;",
			loader: "js",
		}));
	},
};

await esbuild.build({
	entryPoints: [path.join(here, "engine.js")],
	bundle: true,
	format: "iife",
	target: "es2022",
	platform: "neutral",
	mainFields: ["module", "main"],
	minify,
	legalComments: "eof",
	// 与原版一致：原版 tsconfig 目标 ES6，TypeScript 因此用赋值语义处理类字段
	tsconfigRaw: { compilerOptions: { useDefineForClassFields: false } },
	outfile: path.join(here, "dist", "engine.js"),
	alias: {
		"@codemirror/view": path.join(here, "shim", "view.js"),
		"obsidian": path.join(here, "shim", "obsidian.js"),
		"src": path.join(here, "latex-suite"),
	},
	plugins: [inlineImportPlugin(), stubPlugin],
	logLevel: "warning",
});
console.log("dist/engine.js 已生成");
