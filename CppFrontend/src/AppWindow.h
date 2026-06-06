#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include "IpcClient.h"
#include "nlohmann/json.hpp"

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

    bool create(int width, int height);
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

    void applyAcrylic();
    void updateListView(const std::vector<ClipItem>& items);
    void setWindowCorners();
    void updateWindowRegion(int width, int height);
    static std::string formatTimestamp(long long ts);
    static std::wstring toWide(const std::string& s);
    static std::string toNarrow(const std::wstring& ws);

    HWND m_hwnd = nullptr;
    HWND m_searchBox = nullptr;
    HWND m_listView = nullptr;
    HWND m_statusBar = nullptr;
    HWND m_closeBtn = nullptr;
    HFONT m_closeFont = nullptr;
    HINSTANCE m_hInstance;
    IpcClient m_ipc;
    std::vector<ClipItem> m_items;

    static AppWindow* s_instance;

    static constexpr int ID_SEARCH = 1001;
    static constexpr int ID_LIST = 1002;
    static constexpr int ID_CLOSE = 1003;
    static constexpr int IDM_COPY = 2001;
    static constexpr int IDM_PIN = 2002;
    static constexpr int IDM_DELETE = 2003;
};
