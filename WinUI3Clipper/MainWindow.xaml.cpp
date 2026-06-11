#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#if __has_include("MainWindow.xaml.g.hpp")
#include "MainWindow.xaml.g.hpp"
#endif
#include <shellapi.h>
#include <dwmapi.h>
#include <thread>
#include <sstream>

#pragma comment(lib, "dwmapi.lib")

namespace winrt::WinUI3Clipper::implementation
{
    MainWindow::MainWindow()
    {
        m_items = winrt::single_threaded_observable_vector<WinUI3Clipper::ClipboardItem>();
        try
        {
            InitializeComponent();
        }
        catch (winrt::hresult_error const& e)
        {
            wchar_t buf[512];
            swprintf_s(buf, L"[Clipper] MainWindow init FAILED: hr=0x%08X, msg=%s\n",
                       static_cast<uint32_t>(e.code()), e.message().c_str());
            OutputDebugStringW(buf);
            throw;
        }
    }

    // ── Show / Hide ──
    void MainWindow::Show()
    {
        if (m_showing) return;
        m_showing = true;
        LoadData();
        DoShow();
    }

    void MainWindow::Hide()
    {
        if (!m_showing) return;
        DoHide();
    }

    void MainWindow::DoShow()
    {
        RECT workArea; SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

        int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
        m_targetW = (int)(screenW * 0.212); m_targetH = (int)(screenH * 0.376);
        const int MARGIN = 8;
        m_targetX = workArea.right - m_targetW - MARGIN;
        m_targetY = workArea.bottom - m_targetH - MARGIN;

        // DWM corner rounding + acrylic
        HWND hwnd = GetHwndFromWindow(*this);
        int cornerPref = 2; DwmSetWindowAttribute(hwnd, (DWMWINDOWATTRIBUTE)33, &cornerPref, sizeof(cornerPref));
        int backdrop = 3; DwmSetWindowAttribute(hwnd, (DWMWINDOWATTRIBUTE)38, &backdrop, sizeof(backdrop));
        MARGINS margins{-1}; DwmExtendFrameIntoClientArea(hwnd, &margins);

        auto appWindow = this->AppWindow();
        appWindow.IsShownInSwitchers(false);
        auto presenter = appWindow.Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>();
        presenter.IsAlwaysOnTop(true);
        presenter.IsResizable(false);
        presenter.IsMaximizable(false);
        presenter.IsMinimizable(false);

        appWindow.MoveAndResize(Windows::Graphics::RectInt32{ m_targetX, workArea.bottom, m_targetW, m_targetH });
        appWindow.Show(true);

        this->Activated({ this, &MainWindow::OnActivated });

        // Slide-up animation
        m_animTimer = winrt::Windows::UI::Xaml::DispatcherTimer();
        m_animTimer.Interval(std::chrono::milliseconds(10));
        m_animStep = 0; m_animTotalSteps = 15;
        m_animStartY = (double)workArea.bottom; m_animEndY = (double)m_targetY;
        m_animShowing = true;

        m_animTimer.Tick([this](auto&&...) {
            m_animStep++;
            if (m_animStep >= m_animTotalSteps) {
                m_animTimer.Stop(); m_animTimer = nullptr;
                this->AppWindow().MoveAndResize(Windows::Graphics::RectInt32{ m_targetX, m_targetY, m_targetW, m_targetH });
                SearchBox().Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic);
            } else {
                int curY = (int)(m_animStartY + (m_animEndY - m_animStartY) * m_animStep / m_animTotalSteps);
                this->AppWindow().MoveAndResize(Windows::Graphics::RectInt32{ m_targetX, curY, m_targetW, m_targetH });
            }
        });
        m_animTimer.Start();
    }

    void MainWindow::DoHide()
    {
        if (m_animTimer) { m_animTimer.Stop(); m_animTimer = nullptr; }
        RECT workArea; SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        auto cur = this->AppWindow().Position();

        m_animTimer = winrt::Windows::UI::Xaml::DispatcherTimer();
        m_animTimer.Interval(std::chrono::milliseconds(10));
        m_animStep = 0; m_animTotalSteps = 10;
        m_animStartY = (double)cur.Y; m_animEndY = (double)workArea.bottom;
        m_animShowing = false;

        m_animTimer.Tick([this](auto&&...) {
            m_animStep++;
            if (m_animStep >= m_animTotalSteps) {
                m_animTimer.Stop(); m_animTimer = nullptr;
                this->AppWindow().Hide();
                m_showing = false;
            } else {
                int curY = (int)(m_animStartY + (m_animEndY - m_animStartY) * m_animStep / m_animTotalSteps);
                this->AppWindow().MoveAndResize(Windows::Graphics::RectInt32{ m_targetX, curY, m_targetW, m_targetH });
            }
        });
        m_animTimer.Start();
    }

    void MainWindow::OnActivated(winrt::Windows::Foundation::IInspectable const&,
                                  winrt::Microsoft::UI::Xaml::WindowActivatedEventArgs const& args)
    {
        if (args.WindowActivationState() == winrt::Microsoft::UI::Xaml::WindowActivationState::Deactivated)
            Hide();
    }

    // ── Data Loading ──
    void MainWindow::LoadData()
    {
        StatusText().Text(L" Loading...");
        std::thread([this]() {
            IpcClient ipc;
            auto resp = ipc.queryHistory(0, 100);
            std::vector<WinUI3Clipper::ClipboardItem> parsed;
            try {
                json j = json::parse(resp);
                if (j["status"] == "ok") {
                    for (auto& item : j["data"]["items"]) {
                        auto ci = winrt::make<WinUI3Clipper::implementation::ClipboardItem>();
                        ci.Id(item["id"]);
                        ci.Content(winrt::to_hstring(std::string(item["content"])));
                        ci.ContentType(winrt::to_hstring(item.value("contentType", "text")));
                        ci.Timestamp(item["timestamp"]);
                        ci.IsPinned(item.value("isPinned", 0) == 1);
                        parsed.push_back(ci);
                    }
                }
            } catch (...) {}

            this->DispatcherQueue().TryEnqueue([this, parsed = std::move(parsed)]() mutable {
                m_allItems = std::move(parsed);
                FilterItems();
                wchar_t st[64]; swprintf_s(st, L" %zu items  |  Alt+, to toggle", m_allItems.size());
                StatusText().Text(st); BottomStatus().Text(st);
            });
        }).detach();
    }

    void MainWindow::FilterItems()
    {
        m_items.Clear(); m_selectedIndex = -1;
        std::wstring filter(SearchBox().Text());
        for (auto& item : m_allItems) {
            if (filter.empty() || std::wstring(item.Content()).find(filter) != std::wstring::npos)
                m_items.Append(item);
        }
        wchar_t st[64]; swprintf_s(st, L" %u/%zu items", m_items.Size(), m_allItems.size());
        BottomStatus().Text(st);
    }

    // ── Event Handlers ──
    void MainWindow::OnSearchTextChanged(winrt::Windows::Foundation::IInspectable const&,
                                          winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&)
    { FilterItems(); }

    void MainWindow::OnCloseClick(winrt::Windows::Foundation::IInspectable const&,
                                   winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    { Hide(); }

    void MainWindow::OnItemRightTapped(winrt::Windows::Foundation::IInspectable const&,
                                        winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
    {
        auto listView = ClipList();
        auto point = args.GetPosition(listView);
        for (int32_t i = 0; i < (int32_t)listView.Items().Size(); i++) {
            auto container = listView.ContainerFromIndex(i);
            if (container) {
                auto fe = container.as<winrt::Microsoft::UI::Xaml::FrameworkElement>();
                auto offset = fe.ActualOffset();
                auto size = fe.ActualSize();
                if (point.Y >= offset.y && point.Y < offset.y + size.y) {
                    SelectedIndex(i);
                    ShowContextMenu(i, args.GetPosition(nullptr));
                    break;
                }
            }
        }
    }

    void MainWindow::OnItemDoubleTapped(winrt::Windows::Foundation::IInspectable const&,
                                         winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&)
    {
        if (m_selectedIndex >= 0 && m_selectedIndex < (int32_t)m_items.Size()) {
            CopyItem(m_selectedIndex);
            Hide();
        }
    }

    void MainWindow::OnListKeyDown(winrt::Windows::Foundation::IInspectable const&,
                                    winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        switch (args.Key()) {
        case winrt::Windows::System::VirtualKey::Escape: Hide(); break;
        case winrt::Windows::System::VirtualKey::Enter:
            if (m_selectedIndex >= 0 && m_selectedIndex < (int32_t)m_items.Size()) { CopyItem(m_selectedIndex); Hide(); }
            break;
        case winrt::Windows::System::VirtualKey::Up:
            if (m_selectedIndex > 0) SelectedIndex(m_selectedIndex - 1); break;
        case winrt::Windows::System::VirtualKey::Down:
            if (m_selectedIndex < (int32_t)m_items.Size() - 1) SelectedIndex(m_selectedIndex + 1); break;
        }
    }

    // ── Context Menu ──
    void MainWindow::ShowContextMenu(int32_t index, Windows::Foundation::Point const& pt)
    {
        if (index < 0 || index >= (int32_t)m_items.Size()) return;
        auto item = m_items.GetAt(index);
        auto flyout = winrt::Microsoft::UI::Xaml::Controls::MenuFlyout();

        auto copy = winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        copy.Text(L"Copy"); copy.Click([this, index](auto&&...) { CopyItem(index); });
        flyout.Items().Append(copy);

        auto pin = winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        pin.Text(item.IsPinned() ? L"Unpin" : L"Pin");
        pin.Click([this, index](auto&&...) { PinItem(index); });
        flyout.Items().Append(pin);

        auto del = winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        del.Text(L"Delete"); del.Click([this, index](auto&&...) { DeleteItem(index); });
        flyout.Items().Append(del);

        flyout.ShowAt(nullptr, pt);
    }

    // ── Actions ──
    void MainWindow::CopyItem(int32_t index)
    {
        if (index < 0 || index >= (int32_t)m_items.Size()) return;
        auto item = m_items.GetAt(index);
        std::wstring content(item.Content());
        if (item.ContentType() == L"text") {
            size_t size = (content.size() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem) { memcpy(GlobalLock(hMem), content.c_str(), size); GlobalUnlock(hMem);
                OpenClipboard(nullptr); EmptyClipboard(); SetClipboardData(CF_UNICODETEXT, hMem); CloseClipboard(); }
        }
    }

    void MainWindow::PinItem(int32_t index)
    {
        if (index < 0 || index >= (int32_t)m_items.Size()) return;
        auto item = m_items.GetAt(index);
        m_ipc.pinItem(item.Id(), item.IsPinned() ? 0 : 1);
        item.IsPinned(!item.IsPinned());
        FilterItems();
    }

    void MainWindow::DeleteItem(int32_t index)
    {
        if (index < 0 || index >= (int32_t)m_items.Size()) return;
        auto item = m_items.GetAt(index);
        m_ipc.deleteItem(item.Id());
        for (auto it = m_allItems.begin(); it != m_allItems.end(); ++it)
            if (it->Id() == item.Id()) { m_allItems.erase(it); break; }
        FilterItems();
    }
}
