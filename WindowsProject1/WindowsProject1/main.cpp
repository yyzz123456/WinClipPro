#include <windows.h>
#include <combaseapi.h>
#include <shellapi.h>
#include <thread>
#include "AppWindow.h"
#include "ClipboardMonitor.h"
#include "HotkeyManager.h"
#include "IpcClient.h"
#include "JsonHelper.h"

// Forward declarations
static LRESULT CALLBACK HelperWndProc(HWND, UINT, WPARAM, LPARAM);

// Globals
static AppWindow* g_appWindow = nullptr;
static ClipboardMonitor* g_clipMonitor = nullptr;
static HotkeyManager* g_hotkeyManager = nullptr;
static IpcClient* g_ipc = nullptr;
static HWND g_helperWnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HICON g_trayIcon = nullptr;

static HANDLE g_hJavaProcess = nullptr;

// Pending clipboard items while backend is not ready
struct PendingClip {
    std::string content;
    std::string contentType;
    long long timestamp;
};
static std::vector<PendingClip> g_pendingClips;
static UINT_PTR g_flushTimer = 0;

static constexpr int HOTKEY_ID = 1;
static constexpr UINT WM_TRAYICON = WM_APP + 100;
static constexpr UINT IDM_TRAY_SHOW = 4001;
static constexpr UINT IDM_TRAY_EXIT = 4002;
static constexpr UINT WM_FLUSH_PENDING = WM_APP + 101;

// Try to flush pending clipboard items to backend
static void FlushPendingClips() {
    if (g_pendingClips.empty()) return;
    IpcClient ipc;
    if (!ipc.connect()) return; // backend still not ready

    OutputDebugStringW((L"[Clipper] Flushing " + std::to_wstring(g_pendingClips.size()) + L" pending clips\n").c_str());
    for (auto& pc : g_pendingClips) {
        ipc.saveClipboard(pc.content, pc.contentType, pc.timestamp);
    }
    g_pendingClips.clear();
    if (g_flushTimer) {
        KillTimer(g_helperWnd, g_flushTimer);
        g_flushTimer = 0;
    }
}

// Create clipboard tray icon from 1.svg design using raw GDI (no GDI+)
static HICON CreateClipboardIcon() {
    const int SIZE = 32;
    HDC hdc = GetDC(nullptr);

    // 32-bit ARGB DIB
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = SIZE;
    bi.bV5Height = SIZE;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_RGB;

    VOID* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBmp || !pBits) { ReleaseDC(nullptr, hdc); return nullptr; }

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);

    // Clear to transparent
    RECT rc = {0, 0, SIZE, SIZE};
    FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    memset(pBits, 0, SIZE * SIZE * 4);

    // Draw clipboard using GDI (1.svg design colors)
    HBRUSH grayBrush = CreateSolidBrush(RGB(134, 134, 132));
    HBRUSH whiteBrush = CreateSolidBrush(RGB(252, 252, 249));
    HPEN grayPen = CreatePen(PS_SOLID, 1, RGB(134, 134, 132));
    HGDIOBJ oldPen = SelectObject(memDC, GetStockObject(NULL_PEN));

    // Clipboard board
    SelectObject(memDC, grayBrush);
    Rectangle(memDC, 5, 8, 29, 30);
    // Top clip
    Rectangle(memDC, 10, 2, 23, 10);
    // Paper
    SelectObject(memDC, whiteBrush);
    Rectangle(memDC, 9, 13, 25, 27);
    // Text lines
    SelectObject(memDC, grayPen);
    MoveToEx(memDC, 12, 17, nullptr); LineTo(memDC, 21, 17);
    MoveToEx(memDC, 12, 20, nullptr); LineTo(memDC, 21, 20);
    MoveToEx(memDC, 12, 23, nullptr); LineTo(memDC, 17, 23);

    SelectObject(memDC, oldPen);
    DeleteObject(grayBrush);
    DeleteObject(whiteBrush);
    DeleteObject(grayPen);

    // GDI doesn't set alpha on 32-bit DIBs — manually fix:
    // pixels where RGB != 0 are icon content → alpha = 255
    // pixels where RGB == 0 are background → alpha = 0 (transparent)
    DWORD* px = (DWORD*)pBits;
    for (int i = 0; i < SIZE * SIZE; i++, px++) {
        *px = (*px & 0x00FFFFFF) ? (*px | 0xFF000000) : 0x00000000;
    }

    // AND mask: all pixels drawn
    HDC andDC = CreateCompatibleDC(hdc);
    HBITMAP hMask = CreateBitmap(SIZE, SIZE, 1, 1, nullptr);
    HBITMAP hOldAnd = (HBITMAP)SelectObject(andDC, hMask);
    FillRect(andDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SelectObject(andDC, hOldAnd);
    DeleteDC(andDC);

    ICONINFO ii = {TRUE, 0, 0, hMask, hBmp};
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hMask);
    SelectObject(memDC, hOld);
    DeleteDC(memDC);
    DeleteObject(hBmp);
    ReleaseDC(nullptr, hdc);
    return hIcon;
}

static void SetupTrayIcon(HWND hwnd) {
    g_trayIcon = CreateClipboardIcon();
    if (!g_trayIcon) return;

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_trayIcon;
    wcscpy_s(g_nid.szTip, L"Clipper");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void RemoveTrayIcon() {
    if (g_nid.hWnd)
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_trayIcon)
        DestroyIcon(g_trayIcon);
}

static void ShowTrayMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW, L"Show / Hide");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// Start the Java backend process
static bool startJavaBackend() {
    // Check if service is already running
    IpcClient testConn;
    if (testConn.connect()) {
        testConn.disconnect();
        OutputDebugStringW(L"[Clipper] Java backend already running\n");
        return true;
    }

    // Attempt to start Java backend
    wchar_t javaPath[MAX_PATH];
    wchar_t jarPath[MAX_PATH];

    // Look for Java
    if (GetEnvironmentVariableW(L"JAVA_HOME", javaPath, MAX_PATH) == 0) {
        wcscpy_s(javaPath, L"java");
    } else {
        wcscat_s(javaPath, L"\\bin\\java.exe");
    }

    // Get the directory of current executable
    GetModuleFileNameW(nullptr, jarPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(jarPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';

    // Navigate from exe dir (x64/Debug/) up to project root
    std::wstring projRoot = std::wstring(jarPath) + L"..\\..\\..\\";
    std::wstring classpath = projRoot + L"JavaBackend\\lib\\sqlite-jdbc-3.42.0.0.jar;" +
        projRoot + L"JavaBackend\\lib\\gson-2.10.1.jar;" +
        projRoot + L"JavaBackend\\out\\production\\JavaBackend";

    std::wstring cmdLine = L"\"" + std::wstring(javaPath) + L"\" -cp \"" +
                           classpath + L"\" Main";
    OutputDebugStringW((L"[Clipper] Starting Java: " + cmdLine + L"\n").c_str());
    OutputDebugStringW((L"[Clipper] WorkDir: " + projRoot + L"\n").c_str());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    // CreateProcessW may modify cmdLine — use writable copy
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(),
                       nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr,
                       projRoot.c_str(), &si, &pi)) {
        OutputDebugStringW(L"[Clipper] Java process started OK\n");
        g_hJavaProcess = pi.hProcess;
        CloseHandle(pi.hThread);

        // Wait for service to be ready (poll up to 10 seconds)
        for (int i = 0; i < 20; i++) {
            Sleep(500);
            IpcClient probe;
            if (probe.connect()) {
                probe.disconnect();
                OutputDebugStringW(L"[Clipper] Java backend ready\n");
                return true;
            }
        }
        OutputDebugStringW(L"[Clipper] Java backend TIMEOUT\n");
    } else {
        wchar_t buf[128];
        swprintf_s(buf, L"[Clipper] CreateProcess FAILED: err=%lu\n", GetLastError());
        OutputDebugStringW(buf);
    }
    return false;
}

static void StopJavaBackend() {
    if (g_hJavaProcess) {
        TerminateProcess(g_hJavaProcess, 0);
        CloseHandle(g_hJavaProcess);
        g_hJavaProcess = nullptr;
    }
}

// Clipboard update callback - save to Java backend
static void onClipboardChange(const std::string& content,
                               const std::string& contentType) {
    OutputDebugStringW(L"[Clipper] onClipboardChange() called\n");
    long long now = (long long)time(nullptr);

    if (g_ipc) {
        std::string result = g_ipc->saveClipboard(content, contentType, now);
        wchar_t buf[512];
        swprintf_s(buf, L"[Clipper] saveClipboard result: %hs (len=%zu, type=%hs)\n",
                   result.c_str(), content.size(), contentType.c_str());
        OutputDebugStringW(buf);

        // If save failed (backend not ready), queue it
        if (result.find("\"status\":\"error\"") != std::string::npos) {
            g_pendingClips.push_back({content, contentType, now});
            OutputDebugStringW(L"[Clipper] Queued pending clip, will retry when backend ready\n");
            // Start flush timer: retry every 2 seconds
            if (!g_flushTimer && g_helperWnd) {
                g_flushTimer = SetTimer(g_helperWnd, WM_FLUSH_PENDING, 2000, nullptr);
            }
        }
    } else {
        OutputDebugStringW(L"[Clipper] ERROR: g_ipc is null!\n");
    }
}

// Hotkey callback - toggle the popup
static void onHotkey() {
    if (g_appWindow) {
        g_appWindow->toggle();
    }
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Single-instance check: if already running, bring existing window to front
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ClipperAppSingleton");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"ClipperPopupWindow", L"Clipper");
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Initialize COM for clipboard operations
    CoInitialize(nullptr);

    // Create the popup window
    g_appWindow = new AppWindow(hInstance);
    if (!g_appWindow->create()) {
        CoUninitialize();
        return -1;
    }

    // Show the window on startup
    g_appWindow->show();

    // Create a hidden helper window for clipboard monitoring and hotkeys
    const wchar_t* HELPER_CLASS = L"ClipperHelperWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HelperWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = HELPER_CLASS;
    RegisterClassExW(&wc);

    g_helperWnd = CreateWindowExW(
        0, HELPER_CLASS, L"", 0,
        0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

    // Initialize IPC client
    g_ipc = new IpcClient();

    // Start Java backend BEFORE clipboard monitor
    std::thread([]() {
        startJavaBackend();
    }).detach();

    // Start clipboard monitoring
    g_clipMonitor = new ClipboardMonitor();
    g_clipMonitor->start(g_helperWnd, onClipboardChange);

    // Register global hotkey Alt+,
    g_hotkeyManager = new HotkeyManager();
    g_hotkeyManager->registerHotkey(g_helperWnd, MOD_ALT, VK_OEM_COMMA, HOTKEY_ID, onHotkey);

    // Setup system tray icon
    SetupTrayIcon(g_helperWnd);

    // Message loop
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    RemoveTrayIcon();
    StopJavaBackend();
    g_hotkeyManager->unregister(g_helperWnd, HOTKEY_ID);
    g_clipMonitor->stop();
    delete g_clipMonitor;
    delete g_hotkeyManager;
    delete g_ipc;
    delete g_appWindow;
    DestroyWindow(g_helperWnd);

    CoUninitialize();
    return 0;
}

// Helper window proc for clipboard messages and hotkeys
static LRESULT CALLBACK HelperWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLIPBOARDUPDATE:
            if (g_clipMonitor)
                g_clipMonitor->handleClipboardUpdate();
            return 0;
        case WM_HOTKEY:
            if (g_hotkeyManager)
                g_hotkeyManager->handleHotkey((int)wp);
            return 0;
        case WM_FLUSH_PENDING:
            FlushPendingClips();
            return 0;
        case WM_TRAYICON:
            if (LOWORD(lp) == WM_RBUTTONUP) {
                ShowTrayMenu(hwnd);
            } else if (LOWORD(lp) == WM_LBUTTONUP) {
                if (g_appWindow) g_appWindow->toggle();
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == IDM_TRAY_SHOW) {
                if (g_appWindow) g_appWindow->toggle();
            } else if (LOWORD(wp) == IDM_TRAY_EXIT) {
                RemoveTrayIcon();
                StopJavaBackend();
                PostQuitMessage(0);
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
