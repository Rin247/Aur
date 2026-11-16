#include "theme/FramelessWindow.hpp"

#include <QPoint>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace aur
{

    bool handleFramelessNativeEvent(QWidget *window, const QByteArray &eventType, void *message,
                                    qintptr *result)
    {
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG")
        {
            const auto *native_message = static_cast<MSG *>(message);
            if (native_message->message == WM_NCCALCSIZE && native_message->wParam)
            {
                // The client area is the whole window: the app draws its own frame,
                // and rgrc[0] is left as the new window rect so a maximize grows it.
                *result = 0;
                return true;
            }
            if (native_message->message == WM_GETMINMAXINFO)
            {
                auto *limits = reinterpret_cast<MINMAXINFO *>(native_message->lParam);
                const HMONITOR monitor = MonitorFromWindow(
                    reinterpret_cast<HWND>(window->winId()), MONITOR_DEFAULTTONEAREST);
                MONITORINFO information{};
                information.cbSize = sizeof(information);
                if (GetMonitorInfoW(monitor, &information))
                {
                    const RECT &work = information.rcWork;
                    const RECT &full = information.rcMonitor;
                    limits->ptMaxPosition.x = work.left - full.left;
                    limits->ptMaxPosition.y = work.top - full.top;
                    limits->ptMaxSize.x = work.right - work.left;
                    limits->ptMaxSize.y = work.bottom - work.top;
                    *result = 0;
                    return true;
                }
            }
            if (native_message->message == WM_NCHITTEST && !window->isMaximized())
            {
                const QPoint position(GET_X_LPARAM(native_message->lParam),
                                      GET_Y_LPARAM(native_message->lParam));
                const QPoint local = window->mapFromGlobal(position);
                constexpr int border = 8;
                const bool left = local.x() < border;
                const bool right = local.x() >= window->width() - border;
                const bool top = local.y() < border;
                const bool bottom = local.y() >= window->height() - border;
                if (top && left) *result = HTTOPLEFT;
                else if (top && right) *result = HTTOPRIGHT;
                else if (bottom && left) *result = HTBOTTOMLEFT;
                else if (bottom && right) *result = HTBOTTOMRIGHT;
                else if (left) *result = HTLEFT;
                else if (right) *result = HTRIGHT;
                else if (top) *result = HTTOP;
                else if (bottom) *result = HTBOTTOM;
                else return false;
                return true;
            }
        }
#else
        Q_UNUSED(window);
        Q_UNUSED(eventType);
        Q_UNUSED(message);
        Q_UNUSED(result);
#endif
        return false;
    }

}
