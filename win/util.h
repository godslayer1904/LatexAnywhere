// 公用小工具：日志、路径、字符串转换、计时
#pragma once
#include <windows.h>
#include <string>

namespace la {

std::wstring AppDataDir();          // %LOCALAPPDATA%\LatexAnywhere（不存在就建）
void LogInit();
void Log(const char* fmt, ...);     // UTF-8，带时间；超过 1MB 清空重写
std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
std::wstring ExePath();
double NowMs();
bool ReadFileUtf8(const std::wstring& path, std::string* out);
bool WriteFileBytes(const std::wstring& path, const void* data, size_t size);

}  // namespace la
