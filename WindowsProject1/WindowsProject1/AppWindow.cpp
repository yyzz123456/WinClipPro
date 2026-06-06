#include "AppWindow.h"
#include "JsonHelper.h"
#include <dwmapi.h>
#include <sstream>
#include <ctime>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

AppWindow* AppWindow::s_instance = nullptr;

AppWindow::AppWindow(HINSTANCE hInstance) : m_hInstance(hInstance) {
    s_instance = this;

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

AppWindow::~AppWindow() {
    s_instance = nullptr;
}

bool AppWindow::create(int width, int height) {
    const wchar_t* CLASS_NAME = L"ClipperPopupWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    // Get screen work area (excludes taskbar)
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    int x = (workArea.right - width) / 2;
    int y = (workArea.bottom - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        CLASS_NAME, L"Clipper",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        nullptr, nullptr, m_hInstance, nullptr);

    if (!m_hwnd) return false;

    setWindowCorners();
    applyAcrylic();

    return true;
}

void AppWindow::show() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    onSize(rc.right, rc.bottom);
    refreshList();
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_searchBox);
}

void AppWindow::hide() {
    ShowWindow(m_hwnd, SW_HIDE);
}

void AppWindow::toggle() {
    if (IsWindowVisible(m_hwnd))
        hide();
    else
        show();
}

// ---- Window Procedure ----

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (s_instance && s_instance->m_hwnd == hwnd)
        return s_instance->handleMsg(msg, wp, lp);
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT AppWindow::handleMsg(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: onCreate(); return 0;
        case WM_SIZE:   onSize(LOWORD(lp), HIWORD(lp)); return 0;
        case WM_COMMAND:
            if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == ID_SEARCH) onSearch();
            return 0;
        case WM_NOTIFY:
            if (((NMHDR*)lp)->code == NM_RCLICK && ((NMHDR*)lp)->idFrom == ID_LIST) {
                POINT pt;
                GetCursorPos(&pt);
                onContextMenu(pt);
            }
            // Double-click to copy
            if (((NMHDR*)lp)->code == NM_DBLCLK && ((NMHDR*)lp)->idFrom == ID_LIST) {
                NMITEMACTIVATE* nmia = (NMITEMACTIVATE*)lp;
                if (nmia->iItem >= 0 && nmia->iItem < (int)m_items.size())
                    copyToClipboard(nmia->iItem);
            }
            return 0;
        case WM_CONTEXTMENU:
            if ((HWND)wp == m_listView) {
                POINT pt = {LOWORD(lp), HIWORD(lp)};
                onContextMenu(pt);
            }
            return 0;
        case WM_CLOSE:
            hide();
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) hide();
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) hide();
            if (wp == VK_RETURN) {
                int idx = ListView_GetSelectionMark(m_listView);
                if (idx >= 0 && idx < (int)m_items.size())
                    copyToClipboard(idx);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(m_hwnd, msg, wp, lp);
}

// ---- Initialization ----

void AppWindow::onCreate() {
    // Search box
    m_searchBox = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        12, 12, 0, 28,
        m_hwnd, (HMENU)(UINT_PTR)ID_SEARCH, m_hInstance, nullptr);

    // Set modern font
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable");
    SendMessage(m_searchBox, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Placeholder text
    SendMessageW(m_searchBox, EM_SETCUEBANNER, FALSE,
                 (LPARAM)L"Type to search clipboard history...");

    // List view
    m_listView = CreateWindowExW(
        0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL |
        LVS_NOCOLUMNHEADER,
        12, 48, 0, 0,
        m_hwnd, (HMENU)(UINT_PTR)ID_LIST, m_hInstance, nullptr);

    ListView_SetExtendedListViewStyle(m_listView,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    // Columns
    LVCOLUMNW col{};
    col.mask = LVCF_WIDTH;
    col.cx = 500;
    ListView_InsertColumn(m_listView, 0, &col);

    // Status bar
    m_statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd, nullptr, m_hInstance, nullptr);
}

void AppWindow::onSize(int width, int height) {
    SetWindowPos(m_searchBox, nullptr, 0, 0,
                 width - 24, 28, SWP_NOMOVE | SWP_NOZORDER);
    SetWindowPos(m_listView, nullptr, 0, 0,
                 width - 24, height - 90, SWP_NOMOVE | SWP_NOZORDER);
    SendMessage(m_listView, WM_SIZE, 0, 0);
    SendMessage(m_statusBar, WM_SIZE, 0, 0);

    // Update column width
    LVCOLUMNW col{};
    col.mask = LVCF_WIDTH;
    col.cx = width - 40;
    ListView_SetColumn(m_listView, 0, &col);
}

// ---- Data Loading ----

void AppWindow::refreshList(const std::string& filter) {
    std::string response;
    if (filter.empty()) {
        response = m_ipc.queryHistory(0, 100);
    } else {
        response = m_ipc.searchItems(filter, 100);
    }

    try {
        json j = json::parse(response);
        if (j["status"] != "ok") {
            SendMessageW(m_statusBar, SB_SETTEXT, 0,
                (LPARAM)L" Failed to load - is Java backend running?");
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

        wchar_t status[64];
        swprintf_s(status, L" %zu items  |  Alt+V to toggle", m_items.size());
        SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)status);
    } catch (...) {
        SendMessageW(m_statusBar, SB_SETTEXT, 0,
            (LPARAM)L" Backend not connected - start Java first");
    }
}

void AppWindow::updateListView(const std::vector<ClipItem>& items) {
    SendMessage(m_listView, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(m_listView);

    for (size_t i = 0; i < items.size(); i++) {
        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;

        // Show truncated text + timestamp
        std::string display = items[i].content;
        // Use first line only
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

// ---- Search ----

void AppWindow::onSearch() {
    wchar_t text[256];
    GetWindowTextW(m_searchBox, text, 256);
    std::string filter = toNarrow(text);
    refreshList(filter);
}

// ---- Context Menu ----

void AppWindow::onContextMenu(POINT pt) {
    int idx = ListView_GetSelectionMark(m_listView);
    if (idx < 0 || idx >= (int)m_items.size()) return;

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_COPY, L"Copy to clipboard");
    AppendMenuW(hMenu, MF_STRING, IDM_PIN,
                m_items[idx].pinned ? L"Unpin" : L"Pin");
    AppendMenuW(hMenu, MF_STRING, IDM_DELETE, L"Delete");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                             pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
        case IDM_COPY:  copyToClipboard(idx); break;
        case IDM_PIN:   pinItem(idx); break;
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
            OpenClipboard(m_hwnd);
            EmptyClipboard();
            SetClipboardData(CF_UNICODETEXT, hMem);
            CloseClipboard();
        }
    } else if (item.contentType == "files") {
        try {
            json files = json::parse(item.content);
            std::wstring fileList;
            for (auto& f : files)
                fileList += toWide(f.get<std::string>()) + L'\0';
            fileList += L'\0';

            size_t size = fileList.size() * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem) {
                memcpy(GlobalLock(hMem), fileList.c_str(), size);
                GlobalUnlock(hMem);
                OpenClipboard(m_hwnd);
                EmptyClipboard();
                SetClipboardData(CF_HDROP, hMem); // Simplified - real CF_HDROP needs DROPFILES struct
                CloseClipboard();
            }
        } catch (...) {}
    }

    hide();
}

void AppWindow::pinItem(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    auto& item = m_items[index];
    int newState = item.pinned ? 0 : 1;
    m_ipc.pinItem(item.id, newState);
    item.pinned = !item.pinned;
    // Refresh to re-sort
    wchar_t text[256];
    GetWindowTextW(m_searchBox, text, 256);
    refreshList(toNarrow(text));
}

void AppWindow::deleteItem(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    m_ipc.deleteItem(m_items[index].id);
    m_items.erase(m_items.begin() + index);
    updateListView(m_items);
    wchar_t status[64];
    swprintf_s(status, L" %zu items  |  Alt+V to toggle", m_items.size());
    SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)status);
}

// ---- DWM Effects ----

void AppWindow::applyAcrylic() {
    BOOL useDark = FALSE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &useDark, sizeof(useDark));

    int backdrop = 4;
    DwmSetWindowAttribute(m_hwnd, (DWMWINDOWATTRIBUTE)38,
                          &backdrop, sizeof(backdrop));

    DWM_BLURBEHIND bb{};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(m_hwnd, &bb);

    MARGINS margins{-1};
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
}

void AppWindow::setWindowCorners() {
    // DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 1
    int cornerPref = 1;
    DwmSetWindowAttribute(m_hwnd, (DWMWINDOWATTRIBUTE)33,
                          &cornerPref, sizeof(cornerPref));
}

// ---- Utilities ----

std::wstring AppWindow::toWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, buf, len);
    std::wstring result(buf);
    delete[] buf;
    return result;
}

std::string AppWindow::toNarrow(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    char* buf = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, buf, len, nullptr, nullptr);
    std::string result(buf);
    delete[] buf;
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
