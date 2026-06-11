#pragma once
#include "App.g.h"
#include "ClipboardMonitor.h"
#include "HotkeyManager.h"
#include "IpcClient.h"

namespace winrt::WinUI3Clipper::implementation
{
    struct App : AppT<App>
    {
        App();
        ~App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
        void ToggleMainWindow();

    private:
        static LRESULT CALLBACK HelperWndProc(HWND, UINT, WPARAM, LPARAM);
        void CreateHelperWindow();
        void StartClipboardMonitor();
        void StartHotkey();
        void SetupTrayIcon();
        void RemoveTrayIcon();
        void ShowTrayMenu();
        void OnClipboardChanged(const std::string& content, const std::string& contentType);
        void OnHotkey();
        void StartJavaBackend();
        void StopJavaBackend();
        void FlushPendingClips();
        void RefreshMainWindow();

        winrt::WinUI3Clipper::MainWindow m_window{ nullptr };
        HWND m_helperWnd = nullptr;

        ClipboardMonitor* m_clipMonitor = nullptr;
        HotkeyManager* m_hotkeyManager = nullptr;
        IpcClient* m_ipc = nullptr;

        NOTIFYICONDATAW m_nid = {};
        HICON m_trayIcon = nullptr;
        HANDLE m_hJavaProcess = nullptr;
        UINT_PTR m_flushTimer = 0;
        bool m_windowVisible = false;

        std::vector<std::tuple<std::string, std::string, long long>> m_pendingClips;

        static App* s_instance;

        static constexpr UINT WM_TRAYICON = WM_APP + 100;
        static constexpr UINT WM_FLUSH_PENDING = WM_APP + 101;
        static constexpr UINT IDM_TRAY_SHOW = 4001;
        static constexpr UINT IDM_TRAY_EXIT = 4002;
        static constexpr int HOTKEY_ID = 1;
    };
}

namespace winrt::WinUI3Clipper::factory_implementation
{
    struct App : AppT<App, implementation::App> {};
}
