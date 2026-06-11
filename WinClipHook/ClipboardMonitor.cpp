#include "ClipboardMonitor.h"
#include <string>

ClipboardMonitor& ClipboardMonitor::Instance()
{
    static ClipboardMonitor instance;
    return instance;
}

ClipboardMonitor::~ClipboardMonitor()
{
    Stop();
}

bool ClipboardMonitor::Start(void (*callback)(const wchar_t*, int, long long, const wchar_t*))
{
    if (m_running) return true;

    m_callback = callback;

    HINSTANCE hInst = GetModuleHandle(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WinClipHookMonitor";
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(0, L"WinClipHookMonitor", L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!m_hwnd) return false;

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!AddClipboardFormatListener(m_hwnd))
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        return false;
    }

    m_running = true;
    return true;
}

void ClipboardMonitor::Stop()
{
    if (!m_running) return;

    if (m_hwnd)
    {
        RemoveClipboardFormatListener(m_hwnd);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    m_callback = nullptr;
    m_running = false;
}

LRESULT CALLBACK ClipboardMonitor::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CLIPBOARDUPDATE)
    {
        auto* self = reinterpret_cast<ClipboardMonitor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self) self->NotifyClipboardChange();
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ClipboardMonitor::NotifyClipboardChange()
{
    if (!m_callback) return;

    // Try to get text content
    if (OpenClipboard(nullptr))
    {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData)
        {
            wchar_t* text = static_cast<wchar_t*>(GlobalLock(hData));
            if (text)
            {
                long long timestamp = static_cast<long long>(GetTickCount64());

                // Simple hash (djb2) for dedup
                unsigned long hash = 5381;
                for (const wchar_t* p = text; *p; ++p)
                    hash = ((hash << 5) + hash) + static_cast<unsigned long>(*p);
                wchar_t hashStr[32];
                swprintf_s(hashStr, L"%08lx", hash);

                m_callback(text, 0, timestamp, hashStr);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}
