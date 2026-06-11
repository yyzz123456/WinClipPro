#include "pch.h"
#include "ClipboardItem.h"
#include "ClipboardItem.g.cpp"
#include <ctime>

namespace winrt::WinUI3Clipper::implementation
{
    hstring ClipboardItem::DisplayContent() const
    {
        std::wstring content(m_content);
        size_t nl = content.find(L'\n');
        if (nl != std::wstring::npos)
            content = content.substr(0, nl);
        if (content.length() > 80)
            content = content.substr(0, 77) + L"...";
        return hstring(content);
    }

    hstring ClipboardItem::FormattedTimestamp() const
    {
        time_t t = (time_t)m_timestamp;
        struct tm tm_info;
        localtime_s(&tm_info, &t);
        wchar_t buf[64];
        wcsftime(buf, sizeof(buf) / sizeof(wchar_t), L"%Y-%m-%d %H:%M", &tm_info);
        return hstring(buf);
    }
}
