#include "pch.h"
#include "IpcClient.h"
#include "JsonHelper.h"
#include <iostream>
#include <sstream>

IpcClient::IpcClient() : m_socket(INVALID_SOCKET), m_connected(false) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

IpcClient::~IpcClient() {
    disconnect();
    WSACleanup();
}

bool IpcClient::connect() {
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        OutputDebugStringW(L"[Clipper-W3] IPC socket() failed\n");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9099);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (::connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        wchar_t buf[128];
        swprintf_s(buf, L"[Clipper-W3] IPC connect() FAILED: err=%d\n", WSAGetLastError());
        OutputDebugStringW(buf);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    int timeout = 2000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    OutputDebugStringW(L"[Clipper-W3] IPC connected to backend OK\n");
    m_connected = true;
    return true;
}

void IpcClient::disconnect() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_connected = false;
}

bool IpcClient::isConnected() const {
    return m_connected;
}

std::string IpcClient::sendRequest(const std::string& json) {
    if (!m_connected && !connect()) {
        return R"({"status":"error","message":"not connected"})";
    }

    std::string msg = json + "\n";
    int sent = send(m_socket, msg.c_str(), (int)msg.size(), 0);
    if (sent <= 0) {
        m_connected = false;
        return R"({"status":"error","message":"send failed"})";
    }

    std::string result;
    char buffer[4096];
    while (true) {
        int received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            m_connected = false;
            return R"({"status":"error","message":"recv failed"})";
        }
        buffer[received] = '\0';
        result += buffer;
        if (result.find('\n') != std::string::npos) break;
    }
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

std::string IpcClient::saveClipboard(const std::string& content,
                                     const std::string& contentType,
                                     long long timestamp) {
    std::ostringstream oss;
    oss << R"({"type":"save","data":{)"
        << R"("content":")" << escapeJson(content) << R"(",)"
        << R"("contentType":")" << escapeJson(contentType) << R"(",)"
        << R"("timestamp":)" << timestamp
        << "}}";
    return sendRequest(oss.str());
}

std::string IpcClient::queryHistory(int lastId, int limit) {
    std::ostringstream oss;
    oss << R"({"type":"query","data":{)"
        << R"("lastId":)" << lastId << ","
        << R"("limit":)" << limit
        << "}}";
    return sendRequest(oss.str());
}

std::string IpcClient::deleteItem(int id) {
    std::ostringstream oss;
    oss << R"({"type":"delete","data":{"id":)" << id << "}}";
    return sendRequest(oss.str());
}

std::string IpcClient::pinItem(int id, int isPinned) {
    std::ostringstream oss;
    oss << R"({"type":"pin","data":{)"
        << R"("id":)" << id << ","
        << R"("isPinned":)" << isPinned
        << "}}";
    return sendRequest(oss.str());
}

std::string IpcClient::searchItems(const std::string& keyword, int limit) {
    std::ostringstream oss;
    oss << R"({"type":"search","data":{)"
        << R"("keyword":")" << escapeJson(keyword) << R"(",)"
        << R"("limit":)" << limit
        << "}}";
    return sendRequest(oss.str());
}
