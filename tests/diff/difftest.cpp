// 对照测试（C++ 版）：从 C++ 逐键调用引擎（QuickJS），与原版标准答案比对，并统计单键耗时与内存。
// 用法：la_difftest <engine.js|engine.qbc> <golden.jsonl> [settings.json]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "js_engine.h"
extern "C" {
#include "quickjs.h"
}

static std::string ReadFile(const char* p) {
	std::ifstream f(p, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static std::string Str(JSContext* c, JSValueConst v) {
	size_t n = 0;
	const char* s = JS_ToCStringLen(c, &n, v);
	std::string r = s ? std::string(s, n) : std::string();
	JS_FreeCString(c, s);
	return r;
}

static std::string ToU8(const std::u16string& s) {
	std::string o;
	for (size_t i = 0; i < s.size(); i++) {
		uint32_t c = s[i];
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
		if (c < 0x80) o += char(c);
		else if (c < 0x800) { o += char(0xC0 | (c >> 6)); o += char(0x80 | (c & 63)); }
		else if (c < 0x10000) { o += char(0xE0 | (c >> 12)); o += char(0x80 | ((c >> 6) & 63)); o += char(0x80 | (c & 63)); }
		else { o += char(0xF0 | (c >> 18)); o += char(0x80 | ((c >> 12) & 63)); o += char(0x80 | ((c >> 6) & 63)); o += char(0x80 | (c & 63)); }
	}
	return o;
}

int main(int argc, char** argv) {
	if (argc < 3) { std::fprintf(stderr, "usage: la_difftest engine golden [settings]\n"); return 2; }
	std::string enginePath = argv[1];
	bool bytecode = enginePath.size() > 4 && enginePath.substr(enginePath.size() - 4) == ".qbc";
	std::string script = ReadFile(argv[1]);
	std::string settings = argc > 3 ? ReadFile(argv[3]) : "";

	auto t0 = std::chrono::steady_clock::now();
	la::Engine eng;
	std::string err;
	if (!eng.Init(script, bytecode, settings, &err)) { std::fprintf(stderr, "init failed: %s\n", err.c_str()); return 3; }
	double initMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

	JSRuntime* prt = JS_NewRuntime();
	JSContext* pc = JS_NewContext(prt);
	std::ifstream in(argv[2], std::ios::binary);
	std::string line;
	int total = 0, pass = 0, shown = 0;
	long keys = 0;
	std::vector<double> times;
	while (std::getline(in, line)) {
		if (line.empty()) continue;
		JSValue c = JS_ParseJSON(pc, line.data(), line.size(), "golden");
		JSValue jk = JS_GetPropertyStr(pc, c, "keys"), js = JS_GetPropertyStr(pc, c, "steps"), jid = JS_GetPropertyStr(pc, c, "id");
		int64_t nsteps = 0;
		JS_GetLength(pc, js, &nsteps);
		total++;
		eng.Reset(u"", 0, 0);
		bool ok = true;
		for (int64_t i = 0; i < nsteps && ok; i++) {
			JSValue kv = JS_GetPropertyInt64(pc, jk, i);
			std::string tok = Str(pc, kv);
			JS_FreeValue(pc, kv);
			bool shift = false, ctrl = false;
			if (tok.size() > 2 && tok[1] == '-' && (tok[0] == 'S' || tok[0] == 'C')) { shift = tok[0] == 'S'; ctrl = tok[0] == 'C'; tok = tok.substr(2); }
			if (tok == "Left") tok = "ArrowLeft"; else if (tok == "Right") tok = "ArrowRight"; else if (tok == "Up") tok = "ArrowUp"; else if (tok == "Down") tok = "ArrowDown";
			auto k0 = std::chrono::steady_clock::now();
			la::EditResult r = eng.Key(tok.c_str(), shift, ctrl, false);
			times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - k0).count());
			keys++;
			JSValue st = JS_GetPropertyInt64(pc, js, i);
			JSValue w0 = JS_GetPropertyInt64(pc, st, 0), w1 = JS_GetPropertyInt64(pc, st, 1), w2 = JS_GetPropertyInt64(pc, st, 2), w3 = JS_GetPropertyInt64(pc, st, 3);
			std::string wdoc = Str(pc, w0);
			int32_t wa = 0, wh = 0, wc = 0;
			JS_ToInt32(pc, &wa, w1); JS_ToInt32(pc, &wh, w2); JS_ToInt32(pc, &wc, w3);
			std::string gdoc = ToU8(r.doc);
			if (!r.ok || gdoc != wdoc || r.anchor != wa || r.head != wh || int(r.commit) != wc) {
				ok = false;
				if (shown++ < 8) std::printf("x %s key %lld '%s': want [%s,%d,%d,%d] got [%s,%d,%d,%d] %s\n", Str(pc, jid).c_str(), (long long)i + 1, tok.c_str(),
					wdoc.c_str(), wa, wh, wc, gdoc.c_str(), r.anchor, r.head, int(r.commit), r.error.c_str());
			}
			for (JSValue v : {w0, w1, w2, w3, st}) JS_FreeValue(pc, v);
		}
		if (ok) pass++;
		for (JSValue v : {jk, js, jid, c}) JS_FreeValue(pc, v);
		if (total % 50 == 0) eng.RunPending();
	}
	std::sort(times.begin(), times.end());
	auto pct = [&](double p) { return times.empty() ? 0.0 : times[std::min(times.size() - 1, size_t(times.size() * p))]; };
	std::printf("pass %d/%d, keys %ld; init %.1f ms; per key median %.3f ms, P99 %.3f ms, max %.3f ms; JS heap %.1f MB\n",
		pass, total, keys, initMs, pct(0.5), pct(0.99), times.empty() ? 0 : times.back(), eng.MemoryUsed() / 1048576.0);
	JS_FreeContext(pc);
	JS_FreeRuntime(prt);
	return pass == total ? 0 : 1;
}
