#include "util.h"

#include <shlobj.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace la {
namespace {
std::mutex g_logMu;
std::wstring g_logPath;
}

std::wstring Utf8ToWide(const std::string& s) {
	if (s.empty()) return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

std::string WideToUtf8(const std::wstring& w) {
	if (w.empty()) return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

std::wstring AppDataDir() {
	PWSTR p = nullptr;
	std::wstring dir;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) dir = p;
	CoTaskMemFree(p);
	dir += L"\\LatexAnywhere";
	CreateDirectoryW(dir.c_str(), nullptr);
	return dir;
}

std::wstring ExePath() {
	wchar_t buf[MAX_PATH * 2];
	DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
	return std::wstring(buf, n);
}

double NowMs() {
	static LARGE_INTEGER f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x; }();
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	return c.QuadPart * 1000.0 / f.QuadPart;
}

void LogInit() {
	g_logPath = AppDataDir() + L"\\log.txt";
	WIN32_FILE_ATTRIBUTE_DATA a{};
	if (GetFileAttributesExW(g_logPath.c_str(), GetFileExInfoStandard, &a) && a.nFileSizeLow > 1024 * 1024)
		DeleteFileW(g_logPath.c_str());
}

void Log(const char* fmt, ...) {
	char msg[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	SYSTEMTIME t;
	GetLocalTime(&t);
	char line[2200];
	int n = snprintf(line, sizeof line, "%02d:%02d:%02d.%03d [%lu] %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentProcessId(), msg);
	std::lock_guard<std::mutex> lk(g_logMu);
	if (g_logPath.empty()) return;
	HANDLE h = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
	if (h == INVALID_HANDLE_VALUE) return;
	DWORD w;
	WriteFile(h, line, (DWORD)n, &w, nullptr);
	CloseHandle(h);
}

bool ReadFileUtf8(const std::wstring& path, std::string* out) {
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER sz;
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart > 64 * 1024 * 1024) { CloseHandle(h); return false; }
	out->resize((size_t)sz.QuadPart);
	DWORD r = 0;
	BOOL ok = ReadFile(h, out->data(), (DWORD)sz.QuadPart, &r, nullptr);
	CloseHandle(h);
	if (!ok) return false;
	out->resize(r);
	if (out->size() >= 3 && (unsigned char)(*out)[0] == 0xEF && (unsigned char)(*out)[1] == 0xBB && (unsigned char)(*out)[2] == 0xBF) out->erase(0, 3);
	return true;
}

bool WriteFileBytes(const std::wstring& path, const void* data, size_t size) {
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD w = 0;
	BOOL ok = WriteFile(h, data, (DWORD)size, &w, nullptr);
	CloseHandle(h);
	return ok && w == size;
}

}  // namespace la
