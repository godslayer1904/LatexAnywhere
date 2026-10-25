#include "util.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace la {

std::string Home() {
	const char* h = getenv("HOME");
	return h && *h ? h : "/tmp";
}

static std::string XdgDir(const char* env, const char* fallback) {
	const char* x = getenv(env);
	std::string d = (x && *x) ? x : Home() + fallback;
	d += "/LatexAnywhere";
	// 逐级建目录
	for (size_t p = 1; (p = d.find('/', p)) != std::string::npos; p++) mkdir(d.substr(0, p).c_str(), 0700);
	mkdir(d.c_str(), 0700);
	return d;
}
std::string ConfigDir() { return XdgDir("XDG_CONFIG_HOME", "/.config"); }
std::string StateDir() { return XdgDir("XDG_STATE_HOME", "/.local/state"); }

static std::mutex g_logMu;
static std::string g_logPath;

void LogInit() {
	g_logPath = StateDir() + "/log.txt";
	struct stat st;
	if (stat(g_logPath.c_str(), &st) == 0 && st.st_size > 1024 * 1024) (void)!truncate(g_logPath.c_str(), 0);
}

void Log(const char* fmt, ...) {
	char msg[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	tm t;
	localtime_r(&ts.tv_sec, &t);
	std::lock_guard<std::mutex> lk(g_logMu);
	if (g_logPath.empty()) g_logPath = StateDir() + "/log.txt";
	if (FILE* f = fopen(g_logPath.c_str(), "a")) {
		fprintf(f, "%02d:%02d:%02d.%03ld [%d] %s\n", t.tm_hour, t.tm_min, t.tm_sec, ts.tv_nsec / 1000000, (int)getpid(), msg);
		fclose(f);
	}
}

std::string ExePath() {
	char b[4096];
	ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1);
	return n > 0 ? std::string(b, n) : "latexanywhere";
}

double NowMs() {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

bool ReadFile(const std::string& path, std::string* out) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	out->clear();
	char b[65536];
	size_t n;
	while ((n = fread(b, 1, sizeof b, f)) > 0) out->append(b, n);
	fclose(f);
	return true;
}

bool WriteFile(const std::string& path, const std::string& data) {
	std::string tmp = path + ".tmp";
	FILE* f = fopen(tmp.c_str(), "wb");
	if (!f) return false;
	bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
	ok = fclose(f) == 0 && ok;
	return ok && rename(tmp.c_str(), path.c_str()) == 0;   // 先写临时文件再改名：断电也不会留下半个文件
}

static void PutUtf8(std::string& o, char32_t c) {
	if (c < 0x80) o += (char)c;
	else if (c < 0x800) { o += (char)(0xC0 | c >> 6); o += (char)(0x80 | (c & 0x3F)); }
	else if (c < 0x10000) { o += (char)(0xE0 | c >> 12); o += (char)(0x80 | (c >> 6 & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
	else { o += (char)(0xF0 | c >> 18); o += (char)(0x80 | (c >> 12 & 0x3F)); o += (char)(0x80 | (c >> 6 & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
}

std::string ToUtf8(const std::u16string& s) {
	std::string o;
	o.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++) {
		char32_t c = s[i];
		if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
		else if (c >= 0xD800 && c < 0xE000) c = 0xFFFD;
		PutUtf8(o, c);
	}
	return o;
}

std::u16string FromUtf8(const std::string& s) {
	std::u16string o;
	o.reserve(s.size());
	for (size_t i = 0; i < s.size();) {
		unsigned char b = s[i];
		char32_t c;
		int n;
		if (b < 0x80) { c = b; n = 1; }
		else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; n = 2; }
		else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; n = 3; }
		else if ((b & 0xF8) == 0xF0) { c = b & 0x07; n = 4; }
		else { o += u'�'; i++; continue; }
		if (i + n > s.size()) { o += u'�'; break; }
		for (int k = 1; k < n; k++) c = c << 6 | (s[i + k] & 0x3F);
		i += n;
		if (c >= 0x10000) { c -= 0x10000; o += (char16_t)(0xD800 + (c >> 10)); o += (char16_t)(0xDC00 + (c & 0x3FF)); }
		else o += (char16_t)c;
	}
	return o;
}

std::string WideToUtf8(const wchar_t* s) {
	std::string o;
	for (; s && *s; s++) PutUtf8(o, (char32_t)*s);
	return o;
}

}  // namespace la
