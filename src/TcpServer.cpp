#include "TcpServer.h"

#include <cerrno>
#include <esp_log.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <unistd.h>

static const char* TAG = "TcpServer";

bool TcpServer::start(uint16_t port) {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return false;
    }

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set non-blocking
    int flags = fcntl(serverSocket_, F_GETFL, 0);
    fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Bind failed on port %d", port);
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    if (listen(serverSocket_, 4) < 0) {
        ESP_LOGE(TAG, "Listen failed");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    ESP_LOGI(TAG, "Listening on port %d", port);
    return true;
}

void TcpServer::stop() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets_[i] >= 0) {
            close(clientSockets_[i]);
            clientSockets_[i] = -1;
        }
    }

    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
}

void TcpServer::loop() {
    if (serverSocket_ >= 0) {
        acceptNewClients();
    }
}

void TcpServer::acceptNewClients() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientSocket =
        accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientSocket < 0) {
        return;
    }

    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets_[i] < 0) {
            clientSockets_[i] = clientSocket;
            ESP_LOGI(TAG, "Client connected (slot %d)", i);
            return;
        }
    }

    ESP_LOGW(TAG, "Max clients reached, rejecting connection");
    close(clientSocket);
}

void TcpServer::sendToAll(const char* data, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets_[i] >= 0) {
            int sent = send(clientSockets_[i], data, len, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                ESP_LOGI(TAG, "Client disconnected (slot %d, errno %d)", i, errno);
                close(clientSockets_[i]);
                clientSockets_[i] = -1;
            }
        }
    }
}
