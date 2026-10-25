// 对照测试（Node 版）：加载 core/js/dist/engine.js，逐键与原版标准答案比对。
// 用法：node run_node.mjs <golden.jsonl> [--data=data.json] [--limit=N] [--show=5]
import fs from "node:fs";
import vm from "node:vm";
import path from "node:path";
import readline from "node:readline";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const args = Object.fromEntries(process.argv.slice(3).map((a) => a.replace(/^--/, "").split("=")));
const goldenPath = process.argv[2];
const limit = Number(args.limit ?? Infinity);
const showN = Number(args.show ?? 5);

// 宿主环境：插件用 window.__latex_suite_require、Blob + import(url) 加载片段文本
globalThis.window = globalThis;
globalThis.Blob = class { constructor(parts) { this._s = (parts || []).map(String).join(""); } };
URL.createObjectURL = (b) => "data:text/javascript;base64," + Buffer.from(b._s, "utf8").toString("base64");
URL.revokeObjectURL = () => {};
globalThis.__la_log = (lvl, m) => { if (process.env.LA_DEBUG) process.stderr.write(`[${lvl}] ${m}
`); };
const quietConsole = { ...console, log() {}, warn() {}, debug() {}, info() {} };
globalThis.console = quietConsole;
vm.runInThisContext(fs.readFileSync(path.join(here, "../../core/js/dist/engine.js"), "utf8"), { filename: "engine.js", importModuleDynamically: vm.constants.USE_MAIN_CONTEXT_DEFAULT_LOADER });
const E = globalThis.LatexAnywhereEngine;
await E.init(args.data ? JSON.parse(fs.readFileSync(args.data, "utf8")) : null);

const KEY = { Left: "ArrowLeft", Right: "ArrowRight", Up: "ArrowUp", Down: "ArrowDown" };
const press = (tok) => {
	const m = /^([SC])-(.+)$/.exec(tok);
	const b = m ? m[2] : tok;
	return E.key(KEY[b] || b, m?.[1] === "S", m?.[1] === "C", false);
};

let total = 0, pass = 0, keys = 0;
const fails = [], times = [];
const rl = readline.createInterface({ input: fs.createReadStream(goldenPath) });
for await (const line of rl) {
	if (total >= limit) break;
	const c = JSON.parse(line);
	total++;
	E.reset("", 0, 0);
	let ok = true, at = -1, got = null;
	for (let i = 0; i < c.steps.length; i++) {
		const t0 = process.hrtime.bigint();
		const r = press(c.keys[i]);
		times.push(Number(process.hrtime.bigint() - t0) / 1e6);
		keys++;
		const want = c.steps[i];
		got = [r.doc, r.anchor, r.head, r.commit ? 1 : 0];
		if (want[0] !== got[0] || want[1] !== got[1] || want[2] !== got[2] || want[3] !== got[3]) { ok = false; at = i; break; }
	}
	if (ok) pass++;
	else fails.push({ id: c.id, keys: c.keys, at, want: c.steps[at], got });
}
times.sort((a, b) => a - b);
const pct = (p) => times[Math.min(times.length - 1, Math.floor(times.length * p))]?.toFixed(3);
process.stdout.write(`通过 ${pass}/${total}，按键 ${keys} 次；单键耗时 中位 ${pct(0.5)} ms，P99 ${pct(0.99)} ms，最大 ${times.at(-1)?.toFixed(3)} ms\n`);
for (const f of fails.slice(0, showN)) {
	const k = f.keys.map((x) => (x.length === 1 ? x : `{${x}}`)).join("");
	process.stdout.write(`✗ ${f.id} ${k}\n   第 ${f.at + 1} 键 ${JSON.stringify(f.keys[f.at])}：原版 ${JSON.stringify(f.want)}，引擎 ${JSON.stringify(f.got)}\n`);
}
if (fails.length) fs.writeFileSync(path.join(here, "fails.json"), JSON.stringify(fails, null, 1));
process.exitCode = fails.length ? 1 : 0;
