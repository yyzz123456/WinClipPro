#pragma once
#include <string>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

class IpcClient {
public:
    IpcClient();
    ~IpcClient();

    bool connect();
    void disconnect();
    bool isConnected() const;

    // Request methods
    std::string saveClipboard(const std::string& content,
                              const std::string& contentType,
                              long long timestamp);
    std::string queryHistory(int lastId, int limit);
    std::string deleteItem(int id);
    std::string pinItem(int id, int isPinned);
    std::string searchItems(const std::string& keyword, int limit);

private:
    std::string sendRequest(const std::string& json);
    SOCKET m_socket;
    bool m_connected;
};
