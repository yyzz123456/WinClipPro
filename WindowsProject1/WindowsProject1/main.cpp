#include <windows.h>
#include <combaseapi.h>
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

static constexpr int HOTKEY_ID = 1;

// Start the Java backend process
static bool startJavaBackend() {
    // Check if service is already running
    IpcClient testConn;
    if (testConn.connect()) {
        testConn.disconnect();
        return true;
    }

    // Attempt to start Java backend
    wchar_t javaPath[MAX_PATH];
    wchar_t jarPath[MAX_PATH];

    // Look for Java in common locations
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

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmdLine = L"\"" + std::wstring(javaPath) + L"\" -cp \"" +
                           classpath + L"\" Main";

    if (CreateProcessW(nullptr, (LPWSTR)cmdLine.c_str(),
                       nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr,
                       projRoot.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Wait for service to be ready (poll up to 10 seconds)
        for (int i = 0; i < 20; i++) {
            Sleep(500);
            IpcClient probe;
            if (probe.connect()) {
                probe.disconnect();
                return true;
            }
        }
    }
    return false;
}

// Clipboard update callback - save to Java backend
static void onClipboardChange(const std::string& content,
                               const std::string& contentType) {
    if (g_ipc) {
        long long now = (long long)time(nullptr);
        g_ipc->saveClipboard(content, contentType, now);
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

    // Start clipboard monitoring
    g_clipMonitor = new ClipboardMonitor();
    g_clipMonitor->start(g_helperWnd, onClipboardChange);

    // Register global hotkey Win+V
    g_hotkeyManager = new HotkeyManager();
    g_hotkeyManager->registerHotkey(g_helperWnd, MOD_ALT, VK_OEM_COMMA, HOTKEY_ID, onHotkey);

    // Initialize IPC client
    g_ipc = new IpcClient();

    // Start Java backend
    std::thread([]() {
        startJavaBackend();
    }).detach();

    // Message loop
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
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
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
