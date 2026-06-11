#pragma once
#include "ClipboardItem.g.h"

namespace winrt::WinUI3Clipper::implementation
{
    struct ClipboardItem : ClipboardItemT<ClipboardItem>
    {
        ClipboardItem() = default;

        int32_t Id() const { return m_id; }
        void Id(int32_t value) { if (m_id != value) { m_id = value; RaisePropertyChanged(L"Id"); } }

        hstring Content() const { return m_content; }
        void Content(hstring const& value) { if (m_content != value) { m_content = value; RaisePropertyChanged(L"Content"); RaisePropertyChanged(L"DisplayContent"); } }

        hstring ContentType() const { return m_contentType; }
        void ContentType(hstring const& value) { if (m_contentType != value) { m_contentType = value; RaisePropertyChanged(L"ContentType"); } }

        int64_t Timestamp() const { return m_timestamp; }
        void Timestamp(int64_t value) { if (m_timestamp != value) { m_timestamp = value; RaisePropertyChanged(L"Timestamp"); RaisePropertyChanged(L"FormattedTimestamp"); } }

        bool IsPinned() const { return m_isPinned; }
        void IsPinned(bool value) { if (m_isPinned != value) { m_isPinned = value; RaisePropertyChanged(L"IsPinned"); } }

        hstring DisplayContent() const;
        hstring FormattedTimestamp() const;

        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }
        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        int32_t m_id = 0;
        hstring m_content;
        hstring m_contentType;
        int64_t m_timestamp = 0;
        bool m_isPinned = false;
        event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

        void RaisePropertyChanged(hstring const& name)
        {
            m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(name));
        }
    };
}

namespace winrt::WinUI3Clipper::factory_implementation
{
    struct ClipboardItem : ClipboardItemT<ClipboardItem, implementation::ClipboardItem>
    {
    };
}
