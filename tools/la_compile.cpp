// 构建时把 engine.js 编译成 QuickJS 字节码：la_compile engine.js engine.qbc
#include <cstdio>
#include <fstream>
#include <sstream>
#include "js_engine.h"

int main(int argc, char** argv) {
	if (argc < 3) { std::fprintf(stderr, "usage: la_compile in.js out.qbc\n"); return 2; }
	std::ifstream f(argv[1], std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	std::vector<uint8_t> bc;
	std::string err;
	if (!la::Engine::Compile(ss.str(), &bc, &err)) { std::fprintf(stderr, "compile failed: %s\n", err.c_str()); return 1; }
	std::ofstream o(argv[2], std::ios::binary);
	o.write(reinterpret_cast<const char*>(bc.data()), bc.size());
	std::printf("%s: %zu bytes\n", argv[2], bc.size());
	return 0;
}
