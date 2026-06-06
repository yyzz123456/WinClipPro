#include "ClipboardMonitor.h"
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

ClipboardMonitor::ClipboardMonitor() {}
ClipboardMonitor::~ClipboardMonitor() { stop(); }

bool ClipboardMonitor::start(HWND hwnd, Callback callback) {
    m_hwnd = hwnd;
    m_callback = callback;
    return AddClipboardFormatListener(hwnd);
}

void ClipboardMonitor::stop() {
    if (m_hwnd) {
        RemoveClipboardFormatListener(m_hwnd);
        m_hwnd = nullptr;
    }
}

void ClipboardMonitor::handleClipboardUpdate() {
    std::string content, contentType;

    if (IsClipboardFormatAvailable(CF_UNICODETEXT) ||
        IsClipboardFormatAvailable(CF_TEXT)) {
        content = getClipboardText();
        contentType = "text";
    } else if (IsClipboardFormatAvailable(CF_HDROP)) {
        content = getClipboardFileList();
        contentType = "files";
    } else if (IsClipboardFormatAvailable(CF_DIB) ||
               IsClipboardFormatAvailable(CF_DIBV5) ||
               IsClipboardFormatAvailable(CF_BITMAP)) {
        content = getClipboardImagePath();
        contentType = "image";
    } else {
        return;
    }

    if (content.empty() || (content == m_lastContent && contentType == m_lastContentType))
        return;

    m_lastContent = content;
    m_lastContentType = contentType;

    if (m_callback)
        m_callback(content, contentType);
}

std::string ClipboardMonitor::getClipboardText() {
    if (!OpenClipboard(nullptr)) return "";
    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* text = (wchar_t*)GlobalLock(hData);
        if (text) {
            int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                char* buf = new char[len];
                WideCharToMultiByte(CP_UTF8, 0, text, -1, buf, len, nullptr, nullptr);
                result = buf;
                delete[] buf;
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

std::string ClipboardMonitor::getClipboardImagePath() {
    if (!OpenClipboard(nullptr)) return "";
    std::string result;

    HANDLE hData = GetClipboardData(CF_DIB);
    if (!hData) hData = GetClipboardData(CF_DIBV5);
    if (!hData) { CloseClipboard(); return ""; }

    // Save bitmap to temp file
    BITMAPINFO* pBMI = (BITMAPINFO*)GlobalLock(hData);
    if (!pBMI) { CloseClipboard(); return ""; }

    void* pBits = (BYTE*)pBMI + pBMI->bmiHeader.biSize;
    if (pBMI->bmiHeader.biCompression == BI_BITFIELDS)
        pBits = (BYTE*)pBits + 12;

    // Get image dir
    wchar_t tempPath[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", tempPath, MAX_PATH);
    std::wstring imgDir = std::wstring(tempPath) + L"\\Clipper\\images\\";
    CreateDirectoryW(imgDir.c_str(), nullptr);

    // Generate filename with timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t filename[64];
    swprintf_s(filename, L"clip_%04d%02d%02d_%02d%02d%02d_%03d.png",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring fullPath = imgDir + filename;

    // Use GDI+ to save as PNG
    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken;
    Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);

    Gdiplus::Bitmap* bmp = nullptr;
    if (pBMI->bmiHeader.biBitCount == 32) {
        bmp = new Gdiplus::Bitmap(pBMI->bmiHeader.biWidth,
                                  abs(pBMI->bmiHeader.biHeight),
                                  pBMI->bmiHeader.biWidth * 4,
                                  PixelFormat32bppARGB, (BYTE*)pBits);
    } else {
        // Create from DIB
        HDC hdc = GetDC(nullptr);
        HBITMAP hBmp = CreateDIBitmap(hdc, &pBMI->bmiHeader, CBM_INIT, pBits, pBMI, DIB_RGB_COLORS);
        if (hBmp) {
            bmp = Gdiplus::Bitmap::FromHBITMAP(hBmp, nullptr);
            DeleteObject(hBmp);
        }
        ReleaseDC(nullptr, hdc);
    }

    if (bmp) {
        CLSID pngClsid;
        CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &pngClsid);
        bmp->Save(fullPath.c_str(), &pngClsid, nullptr);
        delete bmp;

        int len = WideCharToMultiByte(CP_UTF8, 0, fullPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        char* buf = new char[len];
        WideCharToMultiByte(CP_UTF8, 0, fullPath.c_str(), -1, buf, len, nullptr, nullptr);
        result = buf;
        delete[] buf;
    }

    Gdiplus::GdiplusShutdown(gdiToken);
    GlobalUnlock(hData);
    CloseClipboard();
    return result;
}

std::string ClipboardMonitor::getClipboardFileList() {
    if (!OpenClipboard(nullptr)) return "";
    std::string result;

    HANDLE hData = GetClipboardData(CF_HDROP);
    if (hData) {
        HDROP hDrop = (HDROP)GlobalLock(hData);
        if (hDrop) {
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            result = "[";
            for (UINT i = 0; i < count; i++) {
                wchar_t filePath[MAX_PATH];
                DragQueryFileW(hDrop, i, filePath, MAX_PATH);
                int len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, nullptr, 0, nullptr, nullptr);
                char* buf = new char[len + 4];
                WideCharToMultiByte(CP_UTF8, 0, filePath, -1, buf, len, nullptr, nullptr);
                if (i > 0) result += ",";
                result += "\"" + std::string(buf) + "\"";
                delete[] buf;
            }
            result += "]";
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}
