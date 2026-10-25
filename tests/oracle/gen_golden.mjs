// 生成对照测试的标准答案：把按键序列交给原版 Latex Suite 1.12.8（oracle.mjs），逐键记录文档与光标。
// 用法：node gen_golden.mjs <输出.jsonl> [--data=data.json] [--fuzz=3000] [--seed=1]
//   不给 --data 用插件默认设置（可公开）；给了就用该设置文件里的片段（个人配置，别提交）。
import { spawn } from "node:child_process";
import readline from "node:readline";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import RandExp from "randexp";

const here = path.dirname(fileURLToPath(import.meta.url));
const args = Object.fromEntries(process.argv.slice(3).map((a) => a.replace(/^--/, "").split("=")));
const outPath = process.argv[2];
const fuzzCount = Number(args.fuzz ?? 3000);
let seed = Number(args.seed ?? 1);

// 固定种子的随机数，保证每次生成同样的用例
function rng() {
	seed |= 0; seed = (seed + 0x6d2b79f5) | 0;
	let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
	t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
	return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}
RandExp.prototype.randInt = (a, b) => a + Math.floor(rng() * (1 + b - a));

// ---------- 读片段 ----------
const evalJs = (src) => new Function("return " + src.replace(/^\s*export\s+default\s+/, ""))();
let snippets, variables;
if (args.data) {
	const d = JSON.parse(fs.readFileSync(args.data, "utf8"));
	snippets = evalJs(d.snippets);
	variables = d.snippetVariables ? evalJs(d.snippetVariables) : evalJs(fs.readFileSync(path.join(here, "../../core/js/latex-suite/default_snippet_variables.js"), "utf8"));
} else {
	snippets = evalJs(fs.readFileSync(path.join(here, "../../core/js/latex-suite/default_snippets.js"), "utf8"));
	variables = evalJs(fs.readFileSync(path.join(here, "../../core/js/latex-suite/default_snippet_variables.js"), "utf8"));
}

function triggerSample(s) {
	const opts = s.options || "";
	if (s.trigger instanceof RegExp || opts.includes("r")) {
		let src = s.trigger instanceof RegExp ? s.trigger.source : String(s.trigger);
		for (const [k, v] of Object.entries(variables)) src = src.split(k).join(v);
		try {
			const re = new RandExp(new RegExp(src));
			re.max = 3;
			const out = re.gen();
			return /[\n\r\t]/.test(out) ? null : out;
		} catch { return null; }
	}
	return String(s.trigger);
}

const prefixFor = (opts) => (opts.includes("M") ? ["d", "m"] : opts.includes("m") || opts.includes("n") ? ["m", "k"] : opts.includes("t") ? [] : ["m", "k"]);
const chars = (str) => [...str];

const cases = [];
snippets.forEach((s, i) => {
	const opts = s.options || "";
	if (opts.includes("c")) return;            // 代码块里的片段：公式框里没有代码块
	const trig = triggerSample(s);
	if (!trig) return;
	const visual = typeof s.replacement === "string" && s.replacement.includes("${VISUAL}");
	const keys = [...prefixFor(opts)];
	if (visual) keys.push("a", "b", "S-Left", "S-Left");
	keys.push(...chars(trig));
	if (!opts.includes("A") && !visual) keys.push(s.triggerKey || "Tab");
	keys.push("x", "Tab", "y", "Tab", "Tab");
	cases.push({ id: `snippet-${i}`, keys });
});

// 手写的典型场景（昨天验证过的 14 组 + 常见编辑操作）
const manual = [
	"mk", "mkx^2{Tab}{Tab}", "mkx2", "mk//a{Tab}b{Tab}", "mkx/", "mksum{Tab}", "mk(sum", "mkalpha", "dm",
	"mkpmat1{Tab}2{Enter}3", "mklim{Tab}m", "mkx^2{Tab}+2x+c{S-Left}{S-Left}{S-Left}{", "mk{Backspace}", "mkab{Left}X",
	"mkx/y{Tab}{Tab}z", "mk(a+b)/c{Tab}", "mkint{Tab}", "mkx^2{C-z}", "mkx^2{C-z}{C-y}", "mkbmat1{Tab}2{S-Enter}",
];
manual.forEach((m, i) => {
	const keys = [];
	for (let j = 0; j < m.length;) {
		const t = /^\{([\w-]+)\}/.exec(m.slice(j));
		if (t) { keys.push(t[1]); j += t[0].length; } else keys.push(m[j++]);
	}
	cases.push({ id: `manual-${i}`, keys });
});

// 随机按键：偏向数学输入里常见的字符和编辑键
const pool = [
	..."abcdefgxyzntkm0123456789".split(""), ..."^_/\\{}()[]+-=,. $".split(""),
	"Tab", "Tab", "Tab", "S-Tab", "S-Enter", "Backspace", "Backspace", "Left", "Right", "S-Left", "Home", "End",
];
for (let i = 0; i < fuzzCount; i++) {
	const n = 3 + Math.floor(rng() * 25);
	const keys = ["m", "k"];
	for (let j = 0; j < n; j++) keys.push(pool[Math.floor(rng() * pool.length)]);
	cases.push({ id: `fuzz-${i}`, keys });
}

// ---------- 交给原版逐键跑 ----------
const host = spawn(process.execPath, [path.join(here, "oracle.mjs"), ...(args.data ? [`--data=${args.data}`] : [])], { stdio: ["pipe", "pipe", "ignore"] });
const rl = readline.createInterface({ input: host.stdout });
const queue = [];
rl.on("line", (l) => queue.shift()?.(JSON.parse(l)));
const send = (o) => new Promise((r) => { queue.push(r); host.stdin.write(JSON.stringify(o) + "\n"); });
await new Promise((r) => queue.push(r));

const KEY = { Left: "ArrowLeft", Right: "ArrowRight", Up: "ArrowUp", Down: "ArrowDown" };
function toMsg(tok) {
	const m = /^([SC])-(.+)$/.exec(tok);
	const base = m ? m[2] : tok;
	return { op: "key", key: KEY[base] || base, shift: m?.[1] === "S", ctrl: m?.[1] === "C" };
}

const out = fs.createWriteStream(outPath);
let n = 0;
for (const c of cases) {
	await send({ op: "reset", doc: "", anchor: 0, head: 0 });
	const steps = [];
	for (const k of c.keys) {
		const r = await send(toMsg(k));
		if (!r.ok) { steps.push(["ERROR", r.error]); break; }
		steps.push([r.doc, r.anchor, r.head, r.commit ? 1 : 0]);
		if (r.commit) break;
	}
	out.write(JSON.stringify({ ...c, steps }) + "\n");
	if (++n % 500 === 0) process.stderr.write(`${n}/${cases.length}\n`);
}
out.end();
host.kill();
process.stderr.write(`完成 ${n} 组 -> ${outPath}\n`);
