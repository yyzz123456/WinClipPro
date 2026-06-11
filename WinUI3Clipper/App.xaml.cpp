#include "pch.h"
#include "App.xaml.h"
#include "App.g.cpp"
#if __has_include("App.xaml.g.hpp")
#include "App.xaml.g.hpp"
#endif
#include "MainWindow.xaml.h"
#include <shellapi.h>
#include <MddBootstrap.h>
#include <thread>

#pragma comment(lib, "shell32.lib")

namespace winrt::WinUI3Clipper::implementation
{
    App* App::s_instance = nullptr;

    App::App()
    {
        s_instance = this;
        UnhandledException([this](winrt::Windows::Foundation::IInspectable const&,
                                   winrt::Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& e)
        {
            wchar_t buf[512];
            swprintf_s(buf, L"[Clipper] Unhandled XAML: hr=0x%08X, msg=%s\n",
                       static_cast<uint32_t>(e.Exception()), e.Message().c_str());
            OutputDebugStringW(buf);
            e.Handled(true);
        });
        InitializeComponent();
    }

    App::~App()
    {
        RemoveTrayIcon();
        StopJavaBackend();
        if (m_hotkeyManager) { m_hotkeyManager->unregister(m_helperWnd, HOTKEY_ID); delete m_hotkeyManager; }
        if (m_clipMonitor) { m_clipMonitor->stop(); delete m_clipMonitor; }
        if (m_ipc) delete m_ipc;
        if (m_helperWnd) DestroyWindow(m_helperWnd);
        if (m_trayIcon) DestroyIcon(m_trayIcon);
        s_instance = nullptr;
    }

    void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&)
    {
        // Single-instance check
        HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ClipperAppSingletonW3");
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            HWND hExisting = FindWindowW(L"ClipperPopupWindow", L"Clipper");
            if (hExisting) { ShowWindow(hExisting, SW_SHOW); SetForegroundWindow(hExisting); }
            CloseHandle(hMutex);
            ExitProcess(0);
            return;
        }

        m_ipc = new IpcClient();
        std::thread([]() { s_instance->StartJavaBackend(); }).detach();

        CreateHelperWindow();
        StartClipboardMonitor();
        StartHotkey();
        SetupTrayIcon();

        m_window = winrt::make<MainWindow>();
        m_window.Activate();
        m_windowVisible = true;
    }

    void App::CreateHelperWindow()
    {
        const wchar_t* HELPER_CLASS = L"ClipperHelperW3";
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = HelperWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = HELPER_CLASS;
        RegisterClassExW(&wc);

        m_helperWnd = CreateWindowExW(0, HELPER_CLASS, L"", 0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        SetWindowLongPtrW(m_helperWnd, GWLP_USERDATA, (LONG_PTR)this);
    }

    LRESULT CALLBACK App::HelperWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        App* self = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        switch (msg)
        {
        case WM_CLIPBOARDUPDATE:
            if (self && self->m_clipMonitor) self->m_clipMonitor->handleClipboardUpdate();
            return 0;
        case WM_HOTKEY:
            if (self && self->m_hotkeyManager) self->m_hotkeyManager->handleHotkey((int)wp);
            return 0;
        case WM_FLUSH_PENDING:
            if (self) self->FlushPendingClips();
            return 0;
        case WM_TRAYICON:
            if (LOWORD(lp) == WM_RBUTTONUP && self) self->ShowTrayMenu();
            else if (LOWORD(lp) == WM_LBUTTONUP && self) self->ToggleMainWindow();
            return 0;
        case WM_COMMAND:
            if (self && LOWORD(wp) == IDM_TRAY_SHOW) self->ToggleMainWindow();
            else if (self && LOWORD(wp) == IDM_TRAY_EXIT) { self->RemoveTrayIcon(); PostQuitMessage(0); }
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    void App::StartClipboardMonitor()
    {
        m_clipMonitor = new ClipboardMonitor();
        m_clipMonitor->start(m_helperWnd, [this](auto&& c, auto&& t) { OnClipboardChanged(c, t); });
    }

    void App::StartHotkey()
    {
        m_hotkeyManager = new HotkeyManager();
        m_hotkeyManager->registerHotkey(m_helperWnd, MOD_ALT, VK_OEM_COMMA, HOTKEY_ID, [this]() { OnHotkey(); });
    }

    void App::OnClipboardChanged(const std::string& content, const std::string& contentType)
    {
        long long now = (long long)time(nullptr);
        if (m_ipc)
        {
            std::string result = m_ipc->saveClipboard(content, contentType, now);
            if (result.find("\"status\":\"error\"") != std::string::npos)
            {
                m_pendingClips.push_back({content, contentType, now});
                if (!m_flushTimer && m_helperWnd)
                    m_flushTimer = SetTimer(m_helperWnd, WM_FLUSH_PENDING, 2000, nullptr);
            }
            else RefreshMainWindow();
        }
    }

    void App::OnHotkey() { ToggleMainWindow(); }

    void App::ToggleMainWindow()
    {
        if (!m_window) return;
        auto mainWnd = m_window;
        if (m_windowVisible) { mainWnd.Hide(); m_windowVisible = false; }
        else { mainWnd.Show(); m_windowVisible = true; }
    }

    void App::RefreshMainWindow()
    {
        if (!m_window || !m_windowVisible) return;
        m_window.DispatcherQueue().TryEnqueue([this]() {
            m_window.LoadData();
        });
    }

    void App::FlushPendingClips()
    {
        if (m_pendingClips.empty()) return;
        IpcClient ipc;
        if (!ipc.connect()) return;
        for (auto& [c, t, ts] : m_pendingClips) ipc.saveClipboard(c, t, ts);
        m_pendingClips.clear();
        if (m_flushTimer && m_helperWnd) { KillTimer(m_helperWnd, m_flushTimer); m_flushTimer = 0; }
        RefreshMainWindow();
    }

    // ── Tray Icon ──
    static HICON CreateClipboardIcon()
    {
        const int SIZE = 32;
        HDC hdc = GetDC(nullptr);
        BITMAPV5HEADER bi{};
        bi.bV5Size = sizeof(bi); bi.bV5Width = SIZE; bi.bV5Height = SIZE; bi.bV5Planes = 1; bi.bV5BitCount = 32; bi.bV5Compression = BI_RGB;
        VOID* pBits = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (!hBmp || !pBits) { ReleaseDC(nullptr, hdc); return nullptr; }
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);
        RECT rc = {0, 0, SIZE, SIZE};
        FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        memset(pBits, 0, SIZE * SIZE * 4);

        HBRUSH gray = CreateSolidBrush(RGB(134,134,132));
        HBRUSH white = CreateSolidBrush(RGB(252,252,249));
        HPEN grayPen = CreatePen(PS_SOLID, 1, RGB(134,134,132));
        HGDIOBJ oldPen = SelectObject(memDC, GetStockObject(NULL_PEN));
        SelectObject(memDC, gray);
        Rectangle(memDC, 5, 8, 29, 30); Rectangle(memDC, 10, 2, 23, 10);
        SelectObject(memDC, white); Rectangle(memDC, 9, 13, 25, 27);
        SelectObject(memDC, grayPen);
        MoveToEx(memDC, 12, 17, nullptr); LineTo(memDC, 21, 17);
        MoveToEx(memDC, 12, 20, nullptr); LineTo(memDC, 21, 20);
        MoveToEx(memDC, 12, 23, nullptr); LineTo(memDC, 17, 23);
        SelectObject(memDC, oldPen);
        DeleteObject(gray); DeleteObject(white); DeleteObject(grayPen);

        DWORD* px = (DWORD*)pBits;
        for (int i = 0; i < SIZE * SIZE; i++, px++)
            *px = (*px & 0x00FFFFFF) ? (*px | 0xFF000000) : 0x00000000;

        HDC andDC = CreateCompatibleDC(hdc);
        HBITMAP hMask = CreateBitmap(SIZE, SIZE, 1, 1, nullptr);
        SelectObject(andDC, hMask);
        FillRect(andDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        DeleteDC(andDC);
        ICONINFO ii = {TRUE, 0, 0, hMask, hBmp};
        HICON hIcon = CreateIconIndirect(&ii);
        DeleteObject(hMask);
        SelectObject(memDC, hOld); DeleteDC(memDC); DeleteObject(hBmp);
        ReleaseDC(nullptr, hdc);
        return hIcon;
    }

    void App::SetupTrayIcon()
    {
        m_trayIcon = CreateClipboardIcon();
        if (!m_trayIcon) return;
        m_nid.cbSize = sizeof(m_nid); m_nid.hWnd = m_helperWnd; m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON; m_nid.hIcon = m_trayIcon;
        wcscpy_s(m_nid.szTip, L"Clipper"); Shell_NotifyIconW(NIM_ADD, &m_nid);
    }

    void App::RemoveTrayIcon() { if (m_nid.hWnd) Shell_NotifyIconW(NIM_DELETE, &m_nid); }

    void App::ShowTrayMenu()
    {
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW, L"Show / Hide");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit");
        POINT pt; GetCursorPos(&pt);
        SetForegroundWindow(m_helperWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, m_helperWnd, nullptr);
        PostMessageW(m_helperWnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);
    }

    // ── Java Backend ──
    void App::StartJavaBackend()
    {
        IpcClient testConn;
        if (testConn.connect()) { testConn.disconnect(); return; }

        wchar_t javaPath[MAX_PATH];
        if (GetEnvironmentVariableW(L"JAVA_HOME", javaPath, MAX_PATH) == 0) wcscpy_s(javaPath, L"java");
        else wcscat_s(javaPath, L"\\bin\\java.exe");

        wchar_t exePath[MAX_PATH]; GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *(lastSlash + 1) = L'\0';
        std::wstring projRoot = std::wstring(exePath) + L"..\\..\\..\\";
        std::wstring classpath = projRoot + L"JavaBackend\\lib\\sqlite-jdbc-3.42.0.0.jar;" +
            projRoot + L"JavaBackend\\lib\\gson-2.10.1.jar;" + projRoot + L"JavaBackend\\out\\production\\JavaBackend";
        std::wstring cmdLine = L"\"" + std::wstring(javaPath) + L"\" -cp \"" + classpath + L"\" Main";

        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');
        if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, projRoot.c_str(), &si, &pi))
        {
            m_hJavaProcess = pi.hProcess; CloseHandle(pi.hThread);
            for (int i = 0; i < 20; i++) { Sleep(500); IpcClient probe; if (probe.connect()) { probe.disconnect(); return; } }
        }
    }

    void App::StopJavaBackend() { if (m_hJavaProcess) { TerminateProcess(m_hJavaProcess, 0); CloseHandle(m_hJavaProcess); m_hJavaProcess = nullptr; } }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const UINT32 majorMinorVersion{ 0x00020000 };
    PCWSTR versionTag{ L"" };
    const PACKAGE_VERSION minVersion{};
    HRESULT hr = MddBootstrapInitialize(majorMinorVersion, versionTag, minVersion);
    if (FAILED(hr)) { wchar_t buf[256]; swprintf_s(buf, L"[Clipper] Bootstrap FAILED: 0x%X\n", hr); OutputDebugStringW(buf); return hr; }

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    ::winrt::Microsoft::UI::Xaml::Application::Start(
        [](auto&&) { winrt::make<winrt::WinUI3Clipper::implementation::App>(); });
    MddBootstrapShutdown();
    return 0;
}
