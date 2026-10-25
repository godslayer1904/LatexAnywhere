// 使用说明窗口：原生窗口（Direct2D + DirectWrite 绘制），不借助浏览器；文字随系统语言（见 i18n）。
#pragma once
#include <windows.h>

namespace la {

// welcome=true：首次启动的欢迎窗口，底部多「下次启动时不再显示」和「开始使用」
void ShowHelp(HINSTANCE inst, bool dark, bool welcome = false);   // 已打开就切到前台
bool WelcomeHidden();   // 用户是否勾选过「不再显示」

}  // namespace la
