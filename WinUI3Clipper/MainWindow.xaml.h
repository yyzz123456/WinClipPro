#pragma once
#include "MainWindow.g.h"
#include "ClipboardItem.h"
#include "IpcClient.h"
#include "JsonHelper.h"

namespace winrt::WinUI3Clipper::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void Show();
        void Hide();
        void LoadData();

        Windows::Foundation::Collections::IObservableVector<WinUI3Clipper::ClipboardItem> Items() { return m_items; }

        int32_t SelectedIndex() { return m_selectedIndex; }
        void SelectedIndex(int32_t value) { if (m_selectedIndex != value) m_selectedIndex = value; }

        void OnSearchTextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void OnCloseClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnItemRightTapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const&);
        void OnItemDoubleTapped(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&);
        void OnListKeyDown(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);

    private:
        void CopyItem(int32_t index);
        void PinItem(int32_t index);
        void DeleteItem(int32_t index);
        void ShowContextMenu(int32_t index, Windows::Foundation::Point const& pt);
        void FilterItems();
        void DoShow();
        void DoHide();
        void OnActivated(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowActivatedEventArgs const&);

        Windows::Foundation::Collections::IObservableVector<WinUI3Clipper::ClipboardItem> m_items;
        std::vector<WinUI3Clipper::ClipboardItem> m_allItems;
        IpcClient m_ipc;
        int32_t m_selectedIndex = -1;

        // Animation
        winrt::Windows::UI::Xaml::DispatcherTimer m_animTimer{ nullptr };
        int m_animStep = 0, m_animTotalSteps = 15;
        double m_animStartY = 0, m_animEndY = 0;
        bool m_animShowing = false;

        // Window position
        int m_targetX = 0, m_targetY = 0, m_targetW = 0, m_targetH = 0;
        bool m_showing = false;
    };
}

namespace winrt::WinUI3Clipper::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
