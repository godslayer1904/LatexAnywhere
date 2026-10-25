// 使用说明窗口（Linux / GTK4）：文字同 Windows 版（win/i18n.cpp），Windows 专有的几句换成 Linux 的说法。
#pragma once
#include <gtk/gtk.h>

namespace la {

// welcome=true：首次启动的欢迎窗口，底部多「下次启动时不再显示」和「开始使用」
void ShowHelp(GtkApplication* app, bool dark, bool welcome = false);   // 已打开就切到前台
void CloseHelp();
bool HelpOpen();
bool WelcomeHidden();

}  // namespace la
