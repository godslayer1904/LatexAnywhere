// 编辑引擎的 C++ 封装：内嵌 QuickJS，运行 core/js 打包出的原版 Latex Suite 引擎。
// 单线程使用：所有调用必须在同一线程。任何 JS 异常都被捕获并转成返回值，不会抛到 C++。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct JSRuntime;
struct JSContext;

namespace la {

struct EditResult {
	bool ok = false;          // false 表示引擎出错（error 里有原因），调用方应保留原文本
	std::u16string doc;       // 编辑框全文（UTF-16，与 Win32 控件一致）
	int anchor = 0, head = 0; // 选区，UTF-16 下标
	bool handled = false;     // 插件或编辑器处理了这个键
	bool commit = false;      // Enter：上屏
	bool cancel = false;      // Esc：取消
	std::string error;
};

struct Mark {                 // 插件给出的标记（括号配色、光标括号、$ 高亮）
	int from, to;
	std::string cls, style;
};

class Engine {
public:
	Engine();
	~Engine();
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	// data/size 可直接指向 exe 资源（只读映射内存），不必复制
	struct Script { const char* data; size_t size; bool bytecode; const char* name; };
	// scripts：engine.js、math.js 等（源码或 QuickJS 字节码）；settingsJson：Latex Suite 的 data.json 内容，空则用默认设置
	bool Init(const std::vector<Script>& scripts, const std::string& settingsJson, std::string* error);
	bool Init(const std::string& script, bool isBytecode, const std::string& settingsJson, std::string* error) {
		return Init({{script.data(), script.size(), isBytecode, "engine.js"}}, settingsJson, error);
	}

	EditResult Reset(const std::u16string& doc, int anchor, int head);
	EditResult Key(const char* name, bool shift, bool ctrl, bool alt);
	EditResult InsertText(const std::u16string& text);
	EditResult Paste(const std::u16string& text);
	EditResult SetSelection(int anchor, int head);
	std::vector<Mark> Marks();
	// 调用 globalThis[obj][fn](arg)，返回字符串结果（失败返回 false，error 里是原因）
	bool CallString(const char* obj, const char* fn, const std::u16string& arg, std::u16string* out, std::string* error);

	// 运行到期的定时器与微任务（宿主在空闲时调用）
	void RunPending();
	size_t MemoryUsed() const;
	void CollectGarbage();   // 空闲时调用（框收起后），避免打字时触发回收

	// 把 engine.js 编译成字节码（构建时用）
	static bool Compile(const std::string& source, std::vector<uint8_t>* out, std::string* error);

	struct Impl;  // 内部状态（定时器、缓存的函数句柄）

private:
	EditResult CallEdit(const char* fn, int argc, void* argv);
	void DrainJobs();
	JSRuntime* rt_ = nullptr;
	JSContext* ctx_ = nullptr;
	Impl* impl_ = nullptr;
};

}  // namespace la
