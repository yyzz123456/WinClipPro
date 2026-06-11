#define _WIN32_WINNT 0x0A00
#include "AppWindow.h"
#include "JsonHelper.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
#include <sstream>
#include <ctime>
#include <thread>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

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
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

AppWindow::~AppWindow() {
    destroyD2D();
    if (m_closeFont) DeleteObject(m_closeFont);
    s_instance = nullptr;
}

bool AppWindow::create() {
    const wchar_t* CLASS_NAME = L"ClipperPopupWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | 0x00020000;
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
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME, L"Clipper",
        WS_POPUP | WS_THICKFRAME | WS_SYSMENU,
        m_targetX, m_targetY, m_targetW, m_targetH,
        nullptr, nullptr, m_hInstance, nullptr);

    if (!m_hwnd) return false;

    // Child controls NOW — m_hwnd was NULL during WM_CREATE inside CreateWindowExW
    onCreate();

    bool dark = IsDarkMode();

    int cornerPref = 2;
    DwmSetWindowAttribute(m_hwnd, (DWMWINDOWATTRIBUTE)33, &cornerPref, sizeof(cornerPref));

    int backdrop = 3;
    DwmSetWindowAttribute(m_hwnd, (DWMWINDOWATTRIBUTE)38, &backdrop, sizeof(backdrop));

    MARGINS margins{-1};
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        using fn_t = BOOL(WINAPI*)(HWND, void*);
        auto fn = (fn_t)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
        if (fn) {
            struct { int State, Flags, Color, AnimId; } p{};
            p.State = 4;
            p.Flags = 0;
            p.Color = dark ? 0x80000000 : 0xC0FFFFFF;
            struct { DWORD A; void* D; SIZE_T S; } d{19, &p, sizeof(p)};
            fn(m_hwnd, &d);
        }
    }

    return true;
}

void AppWindow::show() {
    if (IsWindowVisible(m_hwnd)) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    onSize(rc.right, rc.bottom);

    m_loadingData = false;
    SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)L" Loading...");

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    SetWindowPos(m_hwnd, nullptr, m_targetX, workArea.bottom, m_targetW, m_targetH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    if (m_animTimer) { KillTimer(m_hwnd, ANIM_TIMER_ID); }

    m_animStep = 0;
    m_animTotalSteps = 15;
    m_animStartY = workArea.bottom;
    m_animEndY = m_targetY;
    m_animShowing = true;
    m_animTimer = SetTimer(m_hwnd, ANIM_TIMER_ID, 10, nullptr);
}

void AppWindow::hide() {
    if (!IsWindowVisible(m_hwnd)) return;

    RECT winRect;
    GetWindowRect(m_hwnd, &winRect);

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

    if (m_animTimer) { KillTimer(m_hwnd, ANIM_TIMER_ID); }

    m_animStep = 0;
    m_animTotalSteps = 10;
    m_animStartY = winRect.top;
    m_animEndY = workArea.bottom;
    m_animShowing = false;
    m_animTimer = SetTimer(m_hwnd, ANIM_TIMER_ID, 10, nullptr);
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
        case WM_CREATE: if (!m_searchBox) onCreate(); return 0;
        case WM_SIZE:   onSize(LOWORD(lp), HIWORD(lp)); return 0;
        case WM_TIMER:
            if (wp == ANIM_TIMER_ID) {
                m_animStep++;
                if (m_animStep >= m_animTotalSteps) {
                    KillTimer(m_hwnd, ANIM_TIMER_ID);
                    m_animTimer = 0;
                    if (m_animShowing) {
                        SetWindowPos(m_hwnd, nullptr, m_targetX, m_targetY, m_targetW, m_targetH, SWP_NOZORDER);
                        SetForegroundWindow(m_hwnd);
                        SetFocus(m_searchBox);
                        if (!m_loadingData) {
                            m_loadingData = true;
                            std::thread([this]() {
                                std::string response;
                                for (int retry = 0; retry < 10; retry++) {
                                    IpcClient ipc;
                                    response = ipc.queryHistory(0, 100);
                                    try {
                                        json j = json::parse(response);
                                        if (j["status"] == "ok") break;
                                    } catch (...) {}
                                    if (retry < 9) Sleep(800);
                                }
                                std::string* pResp = new std::string(std::move(response));
                                PostMessage(m_hwnd, WM_REFRESH_DATA, 0, (LPARAM)pResp);
                            }).detach();
                        }
                    } else {
                        SetWindowPos(m_hwnd, nullptr, m_targetX, m_targetY, m_targetW, m_targetH, SWP_NOZORDER);
                        ShowWindow(m_hwnd, SW_HIDE);
                    }
                } else {
                    int curY = m_animStartY + (m_animEndY - m_animStartY) * m_animStep / m_animTotalSteps;
                    SetWindowPos(m_hwnd, nullptr, m_targetX, curY, m_targetW, m_targetH,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
            return 0;
        case WM_REFRESH_DATA: {
            std::string* pResp = (std::string*)lp;
            if (pResp) {
                applyRefreshResponse(*pResp);
                delete pResp;
            }
            return 0;
        }
        case WM_PAINT:
            renderList();
            ValidateRect(m_hwnd, nullptr);
            return 0;
        case WM_ERASEBKGND:
            return 1; // we handle background in renderList
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
        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int idx = hitTestItem(pt.y);
            if (idx >= 0) {
                m_selectedIndex = idx;
                InvalidateRect(m_hwnd, &m_listRect, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int idx = hitTestItem(pt.y);
            if (idx >= 0 && idx < (int)m_items.size())
                copyToClipboard(idx);
            return 0;
        }
        case WM_RBUTTONUP: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int idx = hitTestItem(pt.y);
            if (idx >= 0) {
                m_selectedIndex = idx;
                InvalidateRect(m_hwnd, &m_listRect, FALSE);
            }
            POINT screenPt;
            GetCursorPos(&screenPt);
            onContextMenu(screenPt);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            m_scrollOffset -= delta;
            if (m_scrollOffset < 0) m_scrollOffset = 0;
            updateScrollRange();
            InvalidateRect(m_hwnd, &m_listRect, FALSE);
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE && !m_menuActive) hide();
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) hide();
            if (wp == VK_RETURN) {
                if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_items.size())
                    copyToClipboard(m_selectedIndex);
            }
            if (wp == VK_UP) {
                if (m_selectedIndex > 0) { m_selectedIndex--; InvalidateRect(m_hwnd, &m_listRect, FALSE); }
            }
            if (wp == VK_DOWN) {
                if (m_selectedIndex < (int)m_items.size() - 1) { m_selectedIndex++; InvalidateRect(m_hwnd, &m_listRect, FALSE); }
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

    m_statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, m_hwnd, nullptr, m_hInstance, nullptr);

    initD2D();
}

void AppWindow::onSize(int width, int height) {
    SetWindowPos(m_closeBtn, nullptr, width - 50, 8, 30, 30, SWP_NOZORDER);
    SetWindowPos(m_searchBox, nullptr, 0, 0, width - 66, 28, SWP_NOMOVE | SWP_NOZORDER);
    SendMessage(m_statusBar, WM_SIZE, 0, 0);

    m_listRect = {12, 48, width - 12, height - 24};
    updateScrollRange();

    if (m_renderTarget) {
        m_renderTarget->Resize(D2D1::SizeU(width, height));
    }
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
    (void)items;
    m_selectedIndex = -1;
    m_scrollOffset = 0;
    updateScrollRange();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void AppWindow::applyRefreshResponse(const std::string& jsonStr) {
    m_loadingData = false;
    try {
        json j = json::parse(jsonStr);
        if (j["status"] != "ok") {
            SendMessageW(m_statusBar, SB_SETTEXT, 0, (LPARAM)L" Backend not connected");
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

void AppWindow::onSearch() {
    wchar_t text[256];
    GetWindowTextW(m_searchBox, text, 256);
    refreshList(toNarrow(text));
}

void AppWindow::onContextMenu(POINT pt) {
    int idx = m_selectedIndex;
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

// ── D2D rendering for the list ──

void AppWindow::initD2D() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (!m_d2dFactory) return;

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(&m_writeFactory));
    if (m_writeFactory) {
        m_writeFactory->CreateTextFormat(
            L"Segoe UI Variable", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            13.0f, L"", &m_textFormat);
    }
    m_d2dInit = true;
}

void AppWindow::destroyD2D() {
    if (m_brush) { m_brush->Release(); m_brush = nullptr; }
    if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
    if (m_textFormat) { m_textFormat->Release(); m_textFormat = nullptr; }
    if (m_writeFactory) { m_writeFactory->Release(); m_writeFactory = nullptr; }
    if (m_d2dFactory) { m_d2dFactory->Release(); m_d2dFactory = nullptr; }
    m_d2dInit = false;
}

void AppWindow::renderList() {
    if (!m_d2dInit || m_items.empty()) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    // Create render target on first call (need window dimensions)
    if (!m_renderTarget) {
        D2D1_SIZE_U size = D2D1::SizeU(w, h);
        HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_renderTarget);
        if (FAILED(hr) || !m_renderTarget) return;
        m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (!m_brush) {
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &m_brush);
    }

    float lrW = (float)(m_listRect.right - m_listRect.left);
    int visibleCount = (m_listRect.bottom - m_listRect.top) / m_itemHeight;
    for (int i = 0; i < visibleCount && (i + m_scrollOffset) < (int)m_items.size(); i++) {
        int idx = i + m_scrollOffset;
        float y = (float)(m_listRect.top + i * m_itemHeight);

        if (idx == m_selectedIndex) {
            m_brush->SetColor(D2D1::ColorF(0.25f, 0.45f, 0.70f, 0.6f));
            m_renderTarget->FillRectangle(
                D2D1::RectF((float)m_listRect.left, y, (float)m_listRect.right, y + (float)m_itemHeight),
                m_brush);
        }

        std::string display = m_items[idx].content;
        size_t nl = display.find('\n');
        if (nl != std::string::npos) display = display.substr(0, nl);
        if (display.length() > 80) display = display.substr(0, 77) + "...";
        std::wstring wText = toWide(display);

        m_brush->SetColor(D2D1::ColorF(0.9f, 0.9f, 0.85f));
        m_renderTarget->DrawText(
            wText.c_str(), (UINT32)wText.size(), m_textFormat,
            D2D1::RectF((float)(m_listRect.left + 8), y + 4,
                        (float)(m_listRect.right - 8), y + (float)m_itemHeight),
            m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        if (m_brush) { m_brush->Release(); m_brush = nullptr; }
        if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
    }
}

int AppWindow::hitTestItem(int yScreen) {
    if (yScreen < m_listRect.top || yScreen >= m_listRect.bottom)
        return -1;
    int idx = (yScreen - m_listRect.top) / m_itemHeight + m_scrollOffset;
    if (idx >= (int)m_items.size()) return -1;
    return idx;
}

void AppWindow::updateScrollRange() {
    int visibleCount = (m_listRect.bottom - m_listRect.top) / m_itemHeight;
    int maxScroll = (int)m_items.size() - visibleCount;
    if (maxScroll < 0) maxScroll = 0;
    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
    if (m_scrollOffset < 0) m_scrollOffset = 0;
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
