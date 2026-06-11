#pragma once
#include <windows.h>
#include <Unknwn.h>
#undef GetCurrentTime  // windows.h macro conflicts with WinRT
#undef GetClassName    // windows.h macro conflicts with WinRT
#undef GetObject

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Xaml.Navigation.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <shellapi.h>
#include "App.xaml.h"
#include "MainWindow.xaml.h"

// IWindowNative: COM interop to get HWND from WinUI 3 Window
#ifndef __IWINDOWNATIVE_DEFINED__
#define __IWINDOWNATIVE_DEFINED__
struct __declspec(uuid("E7A04557-B1E3-4E1D-8F37-0C9A3F3A3BAE")) IWindowNative : ::IUnknown
{
    virtual HRESULT __stdcall get_WindowHandle(HWND* hWnd) = 0;
};
#endif

inline HWND GetHwndFromWindow(winrt::Windows::Foundation::IInspectable const& window)
{
    HWND hwnd = nullptr;
    auto native = window.try_as<IWindowNative>();
    if (native)
        native->get_WindowHandle(&hwnd);
    return hwnd;
}
