// 公用小工具（Linux）：日志、路径、字符串转换、计时
#pragma once
#include <string>

namespace la {

std::string ConfigDir();            // ~/.config/LatexAnywhere（不存在就建）
std::string StateDir();             // ~/.local/state/LatexAnywhere：日志、崩溃记录
void LogInit();
void Log(const char* fmt, ...);     // UTF-8，带时间和进程号；超过 1MB 清空重写
std::string ExePath();
double NowMs();
bool ReadFile(const std::string& path, std::string* out);
bool WriteFile(const std::string& path, const std::string& data);
std::string ToUtf8(const std::u16string& s);
std::u16string FromUtf8(const std::string& s);
std::string WideToUtf8(const wchar_t* s);   // Linux 的 wchar_t 是 UTF-32
std::string Home();

}  // namespace la
