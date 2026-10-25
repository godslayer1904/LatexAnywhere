#include "js_engine.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>

extern "C" {
#include "quickjs.h"
}

namespace la {
namespace {

// ---------- UTF-8 <-> UTF-16（不依赖平台 API，Linux 版共用） ----------
std::u16string Utf8ToUtf16(const char* s, size_t n) {
	std::u16string out;
	out.reserve(n);
	size_t i = 0;
	while (i < n) {
		uint32_t c = static_cast<unsigned char>(s[i]);
		int extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
		if (extra) c &= (0x3F >> extra);
		if (i + extra >= n + (extra ? 0 : 1)) { out.push_back(0xFFFD); break; }
		for (int k = 1; k <= extra; k++) c = (c << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
		i += extra + 1;
		if (c >= 0x10000) {
			c -= 0x10000;
			out.push_back(static_cast<char16_t>(0xD800 + (c >> 10)));
			out.push_back(static_cast<char16_t>(0xDC00 + (c & 0x3FF)));
		} else {
			out.push_back(static_cast<char16_t>(c));
		}
	}
	return out;
}

std::string Utf16ToUtf8(const std::u16string& s) {
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++) {
		uint32_t c = s[i];
		if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
			c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
		} else if (c >= 0xD800 && c <= 0xDFFF) {
			c = 0xFFFD;  // 落单的代理项
		}
		if (c < 0x80) out.push_back(static_cast<char>(c));
		else if (c < 0x800) { out.push_back(static_cast<char>(0xC0 | (c >> 6))); out.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
		else if (c < 0x10000) { out.push_back(static_cast<char>(0xE0 | (c >> 12))); out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
		else { out.push_back(static_cast<char>(0xF0 | (c >> 18))); out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
	}
	return out;
}

double NowMs() {
	using namespace std::chrono;
	return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

std::string TakeException(JSContext* ctx) {
	JSValue e = JS_GetException(ctx);
	std::string msg;
	if (const char* s = JS_ToCString(ctx, e)) { msg = s; JS_FreeCString(ctx, s); }
	if (JS_IsObject(e)) {
		JSValue st = JS_GetPropertyStr(ctx, e, "stack");
		if (const char* s = JS_ToCString(ctx, st)) { if (*s) { msg += "\n"; msg += s; } JS_FreeCString(ctx, s); }
		JS_FreeValue(ctx, st);
	}
	JS_FreeValue(ctx, e);
	return msg;
}

// 宿主环境的 JS 部分：定时器、Blob/URL（插件用它把片段文本当模块加载）、console
const char kPrelude[] = R"JS(
(() => {
	const g = globalThis;
	g.window = g;
	g.__la_blobs = Object.create(null);
	let blobN = 0;
	g.Blob = class { constructor(parts) { this._s = (parts || []).map(String).join(""); } };
	g.URL = g.URL || {};
	g.URL.createObjectURL = (b) => { const k = "la-blob:" + (++blobN); g.__la_blobs[k] = b._s; return k; };
	g.URL.revokeObjectURL = (k) => { /* 模块加载完成后再删，见 C++ 端 */ };
	const log = (lvl) => (...a) => g.__la_native_log(lvl, a.map((x) => (x && x.stack) ? x.stack : (typeof x === "object" ? (() => { try { return JSON.stringify(x); } catch { return String(x); } })() : String(x))).join(" "));
	g.console = { log: log("log"), info: log("info"), debug: log("debug"), warn: log("warn"), error: log("error"), trace() {}, assert() {}, group() {}, groupEnd() {}, time() {}, timeEnd() {} };
	g.__la_log = (lvl, m) => g.__la_native_log(lvl, String(m));
	g.queueMicrotask = g.queueMicrotask || ((f) => Promise.resolve().then(f));
	g.performance = g.performance || { now: () => g.__la_now() };
	g.navigator = g.navigator || { userAgent: "LatexAnywhere", platform: "Win32" };
})();
)JS";

}  // namespace

struct Engine::Impl {
	struct Timer { JSValue fn; double due; double interval; };
	std::map<int, Timer> timers;
	int nextTimer = 1;
	JSValue api = JS_UNDEFINED;   // globalThis.LatexAnywhereEngine
	JSValue fnReset = JS_UNDEFINED, fnKey = JS_UNDEFINED, fnInsert = JS_UNDEFINED, fnSel = JS_UNDEFINED, fnMarks = JS_UNDEFINED;
	JSAtom aDoc, aAnchor, aHead, aHandled, aCommit, aCancel;
};

namespace {

Engine::Impl* ImplOf(JSContext* ctx) { return static_cast<Engine::Impl*>(JS_GetContextOpaque(ctx)); }

JSValue NativeLog(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
	if (argc >= 2 && std::getenv("LA_DEBUG")) {
		const char* lvl = JS_ToCString(ctx, argv[0]);
		const char* msg = JS_ToCString(ctx, argv[1]);
		std::fprintf(stderr, "[%s] %s\n", lvl ? lvl : "?", msg ? msg : "");
		JS_FreeCString(ctx, lvl);
		JS_FreeCString(ctx, msg);
	}
	return JS_UNDEFINED;
}

JSValue NativeNow(JSContext* ctx, JSValueConst, int, JSValueConst*) { return JS_NewFloat64(ctx, NowMs()); }

JSValue AddTimer(JSContext* ctx, int argc, JSValueConst* argv, bool repeat) {
	auto* im = ImplOf(ctx);
	if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);
	double ms = 0;
	if (argc >= 2) JS_ToFloat64(ctx, &ms, argv[1]);
	if (ms < 0) ms = 0;
	int id = im->nextTimer++;
	im->timers[id] = {JS_DupValue(ctx, argv[0]), NowMs() + ms, repeat ? (ms < 1 ? 1 : ms) : -1};
	return JS_NewInt32(ctx, id);
}
JSValue SetTimeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) { return AddTimer(ctx, argc, argv, false); }
JSValue SetInterval(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) { return AddTimer(ctx, argc, argv, true); }
JSValue ClearTimer(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
	auto* im = ImplOf(ctx);
	int id = 0;
	if (argc >= 1 && JS_ToInt32(ctx, &id, argv[0]) == 0) {
		auto it = im->timers.find(id);
		if (it != im->timers.end()) { JS_FreeValue(ctx, it->second.fn); im->timers.erase(it); }
	}
	return JS_UNDEFINED;
}

// 模块：只服务插件用 import(blob 地址) 加载的片段文本
char* NormalizeModule(JSContext* ctx, const char*, const char* name, void*) { return js_strdup(ctx, name); }
JSModuleDef* LoadModule(JSContext* ctx, const char* name, void*) {
	if (std::strncmp(name, "la-blob:", 8) != 0) {
		JS_ThrowReferenceError(ctx, "module not found: %s", name);
		return nullptr;
	}
	JSValue g = JS_GetGlobalObject(ctx);
	JSValue blobs = JS_GetPropertyStr(ctx, g, "__la_blobs");
	JSValue src = JS_GetPropertyStr(ctx, blobs, name);
	JSModuleDef* m = nullptr;
	size_t len = 0;
	const char* s = JS_IsString(src) ? JS_ToCStringLen(ctx, &len, src) : nullptr;
	if (s) {
		JSValue fn = JS_Eval(ctx, s, len, name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
		JS_FreeCString(ctx, s);
		if (!JS_IsException(fn)) { m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(fn)); JS_FreeValue(ctx, fn); }
		JSAtom key = JS_NewAtom(ctx, name);
		JS_DeleteProperty(ctx, blobs, key, 0);
		JS_FreeAtom(ctx, key);
	} else {
		JS_ThrowReferenceError(ctx, "blob not found: %s", name);
	}
	JS_FreeValue(ctx, src);
	JS_FreeValue(ctx, blobs);
	JS_FreeValue(ctx, g);
	return m;
}

}  // namespace

Engine::Engine() : impl_(new Impl) {}

Engine::~Engine() {
	if (ctx_) {
		for (auto& [id, t] : impl_->timers) JS_FreeValue(ctx_, t.fn);
		for (JSValue v : {impl_->api, impl_->fnReset, impl_->fnKey, impl_->fnInsert, impl_->fnSel, impl_->fnMarks}) JS_FreeValue(ctx_, v);
		for (JSAtom a : {impl_->aDoc, impl_->aAnchor, impl_->aHead, impl_->aHandled, impl_->aCommit, impl_->aCancel}) JS_FreeAtom(ctx_, a);
		JS_FreeContext(ctx_);
	}
	if (rt_) JS_FreeRuntime(rt_);
	delete impl_;
}

void Engine::DrainJobs() {
	JSContext* c = nullptr;
	for (int guard = 0; guard < 100000 && JS_IsJobPending(rt_); guard++) {
		if (JS_ExecutePendingJob(rt_, &c) < 0 && c) TakeException(c);
	}
}

void Engine::RunPending() {
	if (!ctx_) return;
	double now = NowMs();
	std::vector<int> due;
	for (auto& [id, t] : impl_->timers) if (t.due <= now) due.push_back(id);
	for (int id : due) {
		auto it = impl_->timers.find(id);
		if (it == impl_->timers.end()) continue;
		JSValue fn = JS_DupValue(ctx_, it->second.fn);
		if (it->second.interval >= 0) it->second.due = now + it->second.interval;
		else { JS_FreeValue(ctx_, it->second.fn); impl_->timers.erase(it); }
		JSValue r = JS_Call(ctx_, fn, JS_UNDEFINED, 0, nullptr);
		if (JS_IsException(r)) TakeException(ctx_);
		JS_FreeValue(ctx_, r);
		JS_FreeValue(ctx_, fn);
	}
	DrainJobs();
}

size_t Engine::MemoryUsed() const {
	if (!rt_) return 0;
	JSMemoryUsage u;
	JS_ComputeMemoryUsage(rt_, &u);
	return static_cast<size_t>(u.malloc_size);
}

bool Engine::Init(const std::string& script, bool isBytecode, const std::string& settingsJson, std::string* error) {
	rt_ = JS_NewRuntime();
	if (!rt_) { if (error) *error = "JS_NewRuntime failed"; return false; }
	JS_SetMemoryLimit(rt_, 256u * 1024 * 1024);   // 防失控：引擎正常用量是几 MB
	JS_SetMaxStackSize(rt_, 4u * 1024 * 1024);
	JS_SetModuleLoaderFunc(rt_, NormalizeModule, LoadModule, nullptr);
	ctx_ = JS_NewContext(rt_);
	if (!ctx_) { if (error) *error = "JS_NewContext failed"; return false; }
	JS_SetContextOpaque(ctx_, impl_);

	JSValue g = JS_GetGlobalObject(ctx_);
	JS_SetPropertyStr(ctx_, g, "__la_native_log", JS_NewCFunction(ctx_, NativeLog, "__la_native_log", 2));
	JS_SetPropertyStr(ctx_, g, "__la_now", JS_NewCFunction(ctx_, NativeNow, "__la_now", 0));
	JS_SetPropertyStr(ctx_, g, "setTimeout", JS_NewCFunction(ctx_, SetTimeout, "setTimeout", 2));
	JS_SetPropertyStr(ctx_, g, "setInterval", JS_NewCFunction(ctx_, SetInterval, "setInterval", 2));
	JS_SetPropertyStr(ctx_, g, "clearTimeout", JS_NewCFunction(ctx_, ClearTimer, "clearTimeout", 1));
	JS_SetPropertyStr(ctx_, g, "clearInterval", JS_NewCFunction(ctx_, ClearTimer, "clearInterval", 1));

	auto fail = [&](const char* where) {
		if (error) *error = std::string(where) + ": " + TakeException(ctx_);
		JS_FreeValue(ctx_, g);
		return false;
	};
	JSValue r = JS_Eval(ctx_, kPrelude, sizeof(kPrelude) - 1, "<prelude>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(r)) return fail("prelude");
	JS_FreeValue(ctx_, r);

	if (isBytecode) {
		JSValue fn = JS_ReadObject(ctx_, reinterpret_cast<const uint8_t*>(script.data()), script.size(), JS_READ_OBJ_BYTECODE);
		if (JS_IsException(fn)) return fail("read bytecode");
		r = JS_EvalFunction(ctx_, fn);
	} else {
		r = JS_Eval(ctx_, script.data(), script.size(), "engine.js", JS_EVAL_TYPE_GLOBAL);
	}
	if (JS_IsException(r)) return fail("engine");
	JS_FreeValue(ctx_, r);

	impl_->api = JS_GetPropertyStr(ctx_, g, "LatexAnywhereEngine");
	if (!JS_IsObject(impl_->api)) { JS_FreeValue(ctx_, g); if (error) *error = "LatexAnywhereEngine missing"; return false; }
	impl_->fnReset = JS_GetPropertyStr(ctx_, impl_->api, "reset");
	impl_->fnKey = JS_GetPropertyStr(ctx_, impl_->api, "key");
	impl_->fnInsert = JS_GetPropertyStr(ctx_, impl_->api, "insertText");
	impl_->fnSel = JS_GetPropertyStr(ctx_, impl_->api, "setSelection");
	impl_->fnMarks = JS_GetPropertyStr(ctx_, impl_->api, "marks");
	impl_->aDoc = JS_NewAtom(ctx_, "doc");
	impl_->aAnchor = JS_NewAtom(ctx_, "anchor");
	impl_->aHead = JS_NewAtom(ctx_, "head");
	impl_->aHandled = JS_NewAtom(ctx_, "handled");
	impl_->aCommit = JS_NewAtom(ctx_, "commit");
	impl_->aCancel = JS_NewAtom(ctx_, "cancel");

	JSValue settings = JS_NULL;
	if (!settingsJson.empty()) {
		settings = JS_ParseJSON(ctx_, settingsJson.data(), settingsJson.size(), "settings");
		if (JS_IsException(settings)) return fail("settings json");
	}
	JSValue init = JS_GetPropertyStr(ctx_, impl_->api, "init");
	JSValue promise = JS_Call(ctx_, init, impl_->api, 1, &settings);
	JS_FreeValue(ctx_, init);
	JS_FreeValue(ctx_, settings);
	if (JS_IsException(promise)) return fail("init");
	for (int i = 0; i < 1000 && JS_PromiseState(ctx_, promise) == JS_PROMISE_PENDING; i++) {
		DrainJobs();
		if (JS_PromiseState(ctx_, promise) == JS_PROMISE_PENDING) RunPending();
	}
	JSPromiseStateEnum st = JS_PromiseState(ctx_, promise);
	if (st != JS_PROMISE_FULFILLED) {
		if (error) {
			*error = "init not fulfilled";
			if (st == JS_PROMISE_REJECTED) {
				JSValue why = JS_PromiseResult(ctx_, promise);
				if (const char* s = JS_ToCString(ctx_, why)) { *error += ": "; *error += s; JS_FreeCString(ctx_, s); }
				JS_FreeValue(ctx_, why);
			}
		}
		JS_FreeValue(ctx_, promise);
		JS_FreeValue(ctx_, g);
		return false;
	}
	JS_FreeValue(ctx_, promise);
	JS_FreeValue(ctx_, g);
	return true;
}

EditResult Engine::CallEdit(const char* fn, int argc, void* argvp) {
	EditResult out;
	JSValue* argv = static_cast<JSValue*>(argvp);
	JSValue f = !std::strcmp(fn, "reset") ? impl_->fnReset : !std::strcmp(fn, "key") ? impl_->fnKey
		: !std::strcmp(fn, "insertText") ? impl_->fnInsert : impl_->fnSel;
	JSValue r = JS_Call(ctx_, f, impl_->api, argc, argv);
	for (int i = 0; i < argc; i++) JS_FreeValue(ctx_, argv[i]);
	if (JS_IsException(r)) { out.error = TakeException(ctx_); return out; }
	JSValue doc = JS_GetProperty(ctx_, r, impl_->aDoc);
	size_t len = 0;
	if (const char* s = JS_ToCStringLen(ctx_, &len, doc)) { out.doc = Utf8ToUtf16(s, len); JS_FreeCString(ctx_, s); }
	JS_FreeValue(ctx_, doc);
	auto getInt = [&](JSAtom a) { JSValue v = JS_GetProperty(ctx_, r, a); int32_t x = 0; JS_ToInt32(ctx_, &x, v); JS_FreeValue(ctx_, v); return x; };
	auto getBool = [&](JSAtom a) { JSValue v = JS_GetProperty(ctx_, r, a); bool b = JS_ToBool(ctx_, v) > 0; JS_FreeValue(ctx_, v); return b; };
	out.anchor = getInt(impl_->aAnchor);
	out.head = getInt(impl_->aHead);
	out.handled = getBool(impl_->aHandled);
	out.commit = getBool(impl_->aCommit);
	out.cancel = getBool(impl_->aCancel);
	JS_FreeValue(ctx_, r);
	DrainJobs();
	out.ok = true;
	return out;
}

EditResult Engine::Reset(const std::u16string& doc, int anchor, int head) {
	std::string u8 = Utf16ToUtf8(doc);
	JSValue argv[3] = {JS_NewStringLen(ctx_, u8.data(), u8.size()), JS_NewInt32(ctx_, anchor), JS_NewInt32(ctx_, head)};
	return CallEdit("reset", 3, argv);
}

EditResult Engine::Key(const char* name, bool shift, bool ctrl, bool alt) {
	JSValue argv[4] = {JS_NewString(ctx_, name), JS_NewBool(ctx_, shift), JS_NewBool(ctx_, ctrl), JS_NewBool(ctx_, alt)};
	return CallEdit("key", 4, argv);
}

EditResult Engine::InsertText(const std::u16string& text) {
	std::string u8 = Utf16ToUtf8(text);
	JSValue argv[1] = {JS_NewStringLen(ctx_, u8.data(), u8.size())};
	EditResult r = CallEdit("insertText", 1, argv);
	return r;
}

EditResult Engine::SetSelection(int anchor, int head) {
	JSValue argv[2] = {JS_NewInt32(ctx_, anchor), JS_NewInt32(ctx_, head)};
	return CallEdit("setSelection", 2, argv);
}

std::vector<Mark> Engine::Marks() {
	std::vector<Mark> out;
	JSValue r = JS_Call(ctx_, impl_->fnMarks, impl_->api, 0, nullptr);
	if (JS_IsException(r)) { TakeException(ctx_); return out; }
	int64_t n = 0;
	JS_GetLength(ctx_, r, &n);
	for (int64_t i = 0; i < n; i++) {
		JSValue m = JS_GetPropertyInt64(ctx_, r, i);
		Mark mk{};
		JSValue v0 = JS_GetPropertyInt64(ctx_, m, 0), v1 = JS_GetPropertyInt64(ctx_, m, 1);
		JSValue v2 = JS_GetPropertyInt64(ctx_, m, 2), v3 = JS_GetPropertyInt64(ctx_, m, 3);
		JS_ToInt32(ctx_, &mk.from, v0);
		JS_ToInt32(ctx_, &mk.to, v1);
		if (const char* s = JS_ToCString(ctx_, v2)) { mk.cls = s; JS_FreeCString(ctx_, s); }
		if (const char* s = JS_ToCString(ctx_, v3)) { mk.style = s; JS_FreeCString(ctx_, s); }
		for (JSValue v : {v0, v1, v2, v3, m}) JS_FreeValue(ctx_, v);
		out.push_back(std::move(mk));
	}
	JS_FreeValue(ctx_, r);
	return out;
}

bool Engine::Compile(const std::string& source, std::vector<uint8_t>* out, std::string* error) {
	JSRuntime* rt = JS_NewRuntime();
	JSContext* ctx = JS_NewContext(rt);
	JSValue fn = JS_Eval(ctx, source.data(), source.size(), "engine.js", JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
	bool ok = false;
	if (JS_IsException(fn)) {
		if (error) *error = TakeException(ctx);
	} else {
		size_t size = 0;
		uint8_t* buf = JS_WriteObject(ctx, &size, fn, JS_WRITE_OBJ_BYTECODE);
		if (buf) { out->assign(buf, buf + size); js_free(ctx, buf); ok = true; }
		JS_FreeValue(ctx, fn);
	}
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
	return ok;
}

}  // namespace la
