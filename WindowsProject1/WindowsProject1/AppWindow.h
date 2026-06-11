#pragma once
#include <windows.h>
#include <commctrl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include "IpcClient.h"
#include "nlohmann/json.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

struct ClipItem {
    int id;
    std::string content;
    std::string contentType;
    long long timestamp;
    bool pinned;
};

class AppWindow {
public:
    AppWindow(HINSTANCE hInstance);
    ~AppWindow();

    bool create();
    void show();
    void hide();
    void toggle();
    void refreshList(const std::string& filter = "");
    HWND hwnd() const { return m_hwnd; }

    static AppWindow* instance() { return s_instance; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMsg(UINT msg, WPARAM wp, LPARAM lp);

    void onCreate();
    void onSize(int width, int height);
    void onSearch();
    void onContextMenu(POINT pt);
    void copyToClipboard(int index);
    void pinItem(int index);
    void deleteItem(int index);

    void initD2D();
    void destroyD2D();
    void renderList();
    void updateListView(const std::vector<ClipItem>& items);
    int hitTestItem(int yScreen);
    void updateScrollRange();

    static std::string formatTimestamp(long long ts);
    static std::wstring toWide(const std::string& s);
    static std::string toNarrow(const std::wstring& ws);

    HWND m_hwnd = nullptr;
    HWND m_searchBox = nullptr;
    HWND m_statusBar = nullptr;
    HWND m_closeBtn = nullptr;
    HFONT m_closeFont = nullptr;
    bool m_menuActive = false;
    bool m_loadingData = false;
    // Smooth timer-based animation
    UINT_PTR m_animTimer = 0;
    int m_animStep = 0;
    int m_animTotalSteps = 0;
    int m_animStartY = 0;
    int m_animEndY = 0;
    bool m_animShowing = false;
    int m_targetX = 0, m_targetY = 0, m_targetW = 0, m_targetH = 0;
    HINSTANCE m_hInstance;
    IpcClient m_ipc;
    std::vector<ClipItem> m_items;

    // D2D rendering for the list (replaces GDI ListView)
    ID2D1Factory* m_d2dFactory = nullptr;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    IDWriteFactory* m_writeFactory = nullptr;
    IDWriteTextFormat* m_textFormat = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    int m_scrollOffset = 0;
    int m_itemHeight = 26;
    int m_selectedIndex = -1;
    RECT m_listRect = {};
    bool m_d2dInit = false;

    static AppWindow* s_instance;

    static constexpr int ID_SEARCH = 1001;
    static constexpr int ID_CLOSE = 1003;
    static constexpr int IDM_COPY = 2001;
    static constexpr int IDM_PIN = 2002;
    static constexpr int IDM_DELETE = 2003;

    static constexpr UINT_PTR ANIM_TIMER_ID = 1;
    static constexpr int WM_REFRESH_DATA = WM_APP + 1;

    void applyRefreshResponse(const std::string& json);
};
