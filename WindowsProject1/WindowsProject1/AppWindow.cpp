#define _WIN32_WINNT 0x0A00
#undef WIN32_LEAN_AND_MEAN
#include "AppWindow.h"
#include "JsonHelper.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
#include <sstream>
#include <ctime>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

AppWindow* AppWindow::s_instance = nullptr;

static bool IsDarkMode() {
    DWORD dark = 0;
    DWORD size = sizeof(dark);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &dark, &size);
    return dark == 0;
}

AppWindow::AppWindow(HINSTANCE hInstance) : m_hInstance(hInstance) {
    s_instance = this;
    Gdiplus::GdiplusStartupInput gdiInput;
    Gdiplus::GdiplusStartup(&m_gdiToken, &gdiInput, nullptr);
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

AppWindow::~AppWindow() {
    delete m_blurBg;
    if (m_closeFont) DeleteObject(m_closeFont);
    Gdiplus::GdiplusShutdown(m_gdiToken);
    s_instance = nullptr;
}

bool AppWindow::create() {
    const wchar_t* CLASS_NAME = L"ClipperPopupWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | 0x00020000; // CS_DROPSHADOW
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int width = (int)(screenW * 0.212);
    int height = (int)(screenH * 0.376);

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    const int MARGIN = 8;
    m_targetX = workArea.right - width - MARGIN;
    m_targetY = workArea.bottom - height - MARGIN;
    m_targetW = width;
    m_targetH = height;

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        CLASS_NAME, L"Clipper",
        WS_POPUP | WS_THICKFRAME | WS_SYSMENU,
        m_targetX, m_targetY, m_targetW, m_targetH,
        nullptr, nullptr, m_hInstance, nullptr);

    if (!m_hwnd) return false;

    bool dark = IsDarkMode();
    BOOL useDark = FALSE; // Force light appearance for whiter base
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

    int backdrop = 4; // Acrylic (lighter than Mica)
    DwmSetWindowAttribute(m_hwnd, (DWMWINDOWATTRIBUTE)38, &backdrop, sizeof(backdrop));

    // DWMWCP_ROUND = 2 (NOT 1 which is DONOTROUND!)
    int cornerPref = 2;
    DwmSetWindowAttribute(m_hwnd, (DWMWINDOWATTRIBUTE)33, &cornerPref, sizeof(cornerPref));

    MARGINS margins{-1};
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    // Acrylic blur (DwmEnableBlurBehindWindow deprecated on Win8+)
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        using fn_t = BOOL(WINAPI*)(HWND, void*);
        auto fn = (fn_t)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
        if (fn) {
            struct { int State, Flags, Color, AnimId; } p{};
            p.State = 4;   // ACCENT_ENABLE_ACRYLICBLURBEHIND
            p.Flags = 2;   // full acrylic noise texture
            p.Color = dark ? 0x60000000 : 0x80FFFFFF;
            struct { int A; void* D; ULONG S; } d{19, &p, sizeof(p)};
            fn(m_hwnd, &d);
        }
    }

    return true;
}

void AppWindow::captureAndBlur() {
    delete m_blurBg;
    m_blurBg = nullptr;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, m_targetW, m_targetH);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, m_targetW, m_targetH, hdcScreen, m_targetX, m_targetY, SRCCOPY);

    m_blurBg = new Gdiplus::Bitmap(hBmp, nullptr);
    Gdiplus::BlurParams params;
    params.radius = (float)m_blurRadius;
    params.padding = 0;
    Gdiplus::Blur blur;
    blur.SetParameters(&params);
    m_blurBg->ApplyEffect(&blur, nullptr);

    SelectObject(hdcMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void AppWindow::show() {
    if (IsWindowVisible(m_hwnd)) return;

    captureAndBlur();

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    onSize(rc.right, rc.bottom);
    refreshList();

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    SetWindowPos(m_hwnd, nullptr, m_targetX, workArea.bottom, m_targetW, m_targetH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    const int STEPS = 4;
    const int DURATION = 50;
    for (int i = 1; i <= STEPS; i++) {
        int curY = workArea.bottom - (workArea.bottom - m_targetY) * i / STEPS;
        SetWindowPos(m_hwnd, nullptr, m_targetX, curY, m_targetW, m_targetH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Sleep(DURATION / STEPS);
    }

    SetWindowPos(m_hwnd, nullptr, m_targetX, m_targetY, m_targetW, m_targetH, SWP_NOZORDER);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_searchBox);
}

void AppWindow::hide() {
    if (!IsWindowVisible(m_hwnd)) return;

    RECT winRect;
    GetWindowRect(m_hwnd, &winRect);

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    const int STEPS = 3;
    const int DURATION = 30;
    for (int i = 1; i <= STEPS; i++) {
        int curY = winRect.top + (workArea.bottom - winRect.top) * i / STEPS;
        SetWindowPos(m_hwnd, nullptr, winRect.left, curY, m_targetW, m_targetH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Sleep(DURATION / STEPS);
    }

    SetWindowPos(m_hwnd, nullptr, m_targetX, m_targetY, m_targetW, m_targetH, SWP_NOZORDER);
    ShowWindow(m_hwnd, SW_HIDE);
}

void AppWindow::toggle() {
    if (IsWindowVisible(m_hwnd)) hide();
    else show();
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (s_instance && s_instance->m_hwnd == hwnd)
        return s_instance->handleMsg(msg, wp, lp);
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT AppWindow::handleMsg(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: onCreate(); return 0;
        case WM_SIZE:   onSize(LOWORD(lp), HIWORD(lp)); return 0;
        case WM_ERASEBKGND:
            if (m_blurBg) {
                RECT rc; GetClientRect(m_hwnd, &rc);
                Gdiplus::Graphics g((HDC)wp);
                g.DrawImage(m_blurBg, 0, 0, rc.right, rc.bottom);
                return 1;
            }
            break;
        case WM_NCHITTEST:
            {
                LRESULT hit = DefWindowProc(m_hwnd, msg, wp, lp);
                if (hit == HTCLIENT) {
                    POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                    ScreenToClient(m_hwnd, &pt);
                    if (pt.y < 40) return HTCAPTION;
                }
                return hit;
            }
        case WM_COMMAND:
            if (HIWORD(wp) == BN_CLICKED && LOWORD(wp) == ID_CLOSE) { hide(); return 0; }
            if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == ID_SEARCH) onSearch();
            return 0;
        case WM_NOTIFY:
            if (((NMHDR*)lp)->code == NM_RCLICK && ((NMHDR*)lp)->idFrom == ID_LIST) {
                POINT pt; GetCursorPos(&pt); onContextMenu(pt);
            }
            if (((NMHDR*)lp)->code == NM_DBLCLK && ((NMHDR*)lp)->idFrom == ID_LIST) {
                NMITEMACTIVATE* nmia = (NMITEMACTIVATE*)lp;
                if (nmia->iItem >= 0 && nmia->iItem < (int)m_items.size())
                    copyToClipboard(nmia->iItem);
            }
            return 0;
        case WM_CONTEXTMENU:
            if ((HWND)wp == m_listView) {
                POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; onContextMenu(pt);
            }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE && !m_menuActive) hide();
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) hide();
            if (wp == VK_RETURN) {
                int idx = ListView_GetSelectionMark(m_listView);
                if (idx >= 0 && idx < (int)m_items.size()) copyToClipboard(idx);
            }
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(m_hwnd, msg, wp, lp);
}

void AppWindow::onCreate() {
    m_closeBtn = CreateWindowExW(
        0, L"BUTTON", L"X",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 30, 30, m_hwnd, (HMENU)(UINT_PTR)ID_CLOSE, m_hInstance, nullptr);
    m_closeFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable");
    SendMessage(m_closeBtn, WM_SETFONT, (WPARAM)m_closeFont, TRUE);

    m_searchBox = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        12, 12, 0, 28, m_hwnd, (HMENU)(UINT_PTR)ID_SEARCH, m_hInstance, nullptr);
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable");
    SendMessage(m_searchBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_searchBox, EM_SETCUEBANNER, FALSE, (LPARAM)L"Type to search clipboard history...");

    m_listView = CreateWindowExW(
        0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
        12, 48, 0, 0, m_hwnd, (HMENU)(UINT_PTR)ID_LIST, m_hInstance, nullptr);
    ListView_SetExtendedListViewStyle(m_listView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW col{}; col.mask = LVCF_WIDTH; col.cx = 500;
    ListView_InsertColumn(m_listView, 0, &col);

    m_statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);
}

void AppWindow::onSize(int width, int height) {
    SetWindowPos(m_closeBtn, nullptr, width - 50, 8, 30, 30, SWP_NOZORDER);
    SetWindowPos(m_searchBox, nullptr, 0, 0, width - 66, 28, SWP_NOMOVE | SWP_NOZORDER);
    SetWindowPos(m_listView, nullptr, 0, 0, width - 24, height - 90, SWP_NOMOVE | SWP_NOZORDER);
    SendMessage(m_listView, WM_SIZE, 0, 0);
    SendMessage(m_statusBar, WM_SIZE, 0, 0);
    LVCOLUMNW col{}; col.mask = LVCF_WIDTH; col.cx = width - 40;
    ListView_SetColumn(m_listView, 0, &col);
}

void AppWindow::refreshList(const std::string& filter) {
    std::string response;
    if (filter.empty()) response = m_ipc.queryHistory(0, 100);
    else response = m_ipc.searchItems(filter, 100);

    try {
        json j = json::parse(response);
        if (j["status"] != "ok") {
            SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)L" Failed to load - is Java backend running?");
            return;
        }
        m_items.clear();
        for (auto& item : j["data"]["items"]) {
            ClipItem ci;
            ci.id = item["id"];
            ci.content = item["content"];
            ci.contentType = item.value("contentType", "text");
            ci.timestamp = item["timestamp"];
            ci.pinned = item.value("isPinned", 0) == 1;
            m_items.push_back(ci);
        }
        updateListView(m_items);
    } catch (...) {
        SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)L" Backend not connected - start Java first");
    }

    wchar_t status[64];
    swprintf_s(status, L" %zu items  |  Alt+, to toggle", m_items.size());
    SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)status);
}

void AppWindow::updateListView(const std::vector<ClipItem>& items) {
    SendMessage(m_listView, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(m_listView);
    for (size_t i = 0; i < items.size(); i++) {
        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;
        std::string display = items[i].content;
        size_t nl = display.find('\n');
        if (nl != std::string::npos) display = display.substr(0, nl);
        if (display.length() > 80) display = display.substr(0, 77) + "...";
        std::wstring wDisplay = toWide(display);
        lvi.pszText = (LPWSTR)wDisplay.c_str();
        ListView_InsertItem(m_listView, &lvi);
    }
    SendMessage(m_listView, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_listView, nullptr, TRUE);
}

void AppWindow::onSearch() {
    wchar_t text[256];
    GetWindowTextW(m_searchBox, text, 256);
    refreshList(toNarrow(text));
}

void AppWindow::onContextMenu(POINT pt) {
    int idx = ListView_GetSelectionMark(m_listView);
    if (idx < 0 || idx >= (int)m_items.size()) return;
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_COPY, L"Copy to clipboard");
    AppendMenuW(hMenu, MF_STRING, IDM_PIN, m_items[idx].pinned ? L"Unpin" : L"Pin");
    AppendMenuW(hMenu, MF_STRING, IDM_DELETE, L"Delete");
    m_menuActive = true;
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
    m_menuActive = false;
    DestroyMenu(hMenu);
    switch (cmd) {
        case IDM_COPY: copyToClipboard(idx); break;
        case IDM_PIN: pinItem(idx); break;
        case IDM_DELETE: deleteItem(idx); break;
    }
}

void AppWindow::copyToClipboard(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    auto& item = m_items[index];
    if (item.contentType == "text") {
        std::wstring wText = toWide(item.content);
        size_t size = (wText.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hMem) {
            memcpy(GlobalLock(hMem), wText.c_str(), size);
            GlobalUnlock(hMem);
            OpenClipboard(m_hwnd); EmptyClipboard();
            SetClipboardData(CF_UNICODETEXT, hMem); CloseClipboard();
        }
    } else if (item.contentType == "files") {
        try {
            json files = json::parse(item.content);
            std::wstring fileList;
            for (auto& f : files) fileList += toWide(f.get<std::string>()) + L'\0';
            fileList += L'\0';
            size_t fileListBytes = fileList.size() * sizeof(wchar_t);
            const UINT DROP_HEADER_SIZE = 20;
            size_t dropSize = DROP_HEADER_SIZE + fileListBytes;
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dropSize);
            if (hMem) {
                BYTE* p = (BYTE*)GlobalLock(hMem);
                p[0] = 20; p[1] = 0; p[2] = 0; p[3] = 0;
                *(UINT*)(p + 4) = 0; *(UINT*)(p + 8) = 0;
                *(UINT*)(p + 12) = 0; *(UINT*)(p + 16) = 1;
                memcpy(p + DROP_HEADER_SIZE, fileList.c_str(), fileListBytes);
                GlobalUnlock(hMem);
                OpenClipboard(m_hwnd); EmptyClipboard();
                SetClipboardData(CF_HDROP, hMem); CloseClipboard();
            }
        } catch (...) {}
    }
    hide();
}

void AppWindow::pinItem(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    auto& item = m_items[index];
    m_ipc.pinItem(item.id, item.pinned ? 0 : 1);
    item.pinned = !item.pinned;
    wchar_t text[256]; GetWindowTextW(m_searchBox, text, 256);
    refreshList(toNarrow(text));
}

void AppWindow::deleteItem(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    m_ipc.deleteItem(m_items[index].id);
    m_items.erase(m_items.begin() + index);
    updateListView(m_items);
    wchar_t status[64];
    swprintf_s(status, L" %zu items  |  Alt+, to toggle", m_items.size());
    SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)status);
}

std::wstring AppWindow::toWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], len);
    return result;
}

std::string AppWindow::toNarrow(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::string AppWindow::formatTimestamp(long long ts) {
    time_t t = (time_t)ts;
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_info);
    return buf;
}
