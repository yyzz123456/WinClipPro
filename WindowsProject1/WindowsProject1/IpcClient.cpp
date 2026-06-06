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
    if (m_socket == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9099);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (::connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

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

    char buffer[8192];
    int received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        m_connected = false;
        return R"({"status":"error","message":"recv failed"})";
    }
    buffer[received] = '\0';
    return std::string(buffer);
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
