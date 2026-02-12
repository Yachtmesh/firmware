#include "WifiGatewayRole.h"

#include <esp_log.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <unistd.h>
#include <cstring>

static const char* TAG = "WifiGateway";

void WifiGatewayRole::start() {
    wifi_.connect(config.ssid, config.password);
    nmea_.addListener(this);
    status_.running = true;
    ESP_LOGI(TAG, "Started, connecting to SSID: %s", config.ssid);
}

void WifiGatewayRole::stop() {
    nmea_.removeListener(this);

    // Close all client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets_[i] >= 0) {
            close(clientSockets_[i]);
            clientSockets_[i] = -1;
        }
    }

    // Close server socket
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }

    wifi_.disconnect();
    status_.running = false;
    ESP_LOGI(TAG, "Stopped");
}

void WifiGatewayRole::loop() {
    // Start TCP server once WiFi is connected
    if (wifi_.isConnected() && serverSocket_ < 0) {
        startTcpServer();
    }

    if (serverSocket_ >= 0) {
        acceptNewClients();
    }
}

void WifiGatewayRole::startTcpServer() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return;
    }

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set non-blocking
    int flags = fcntl(serverSocket_, F_GETFL, 0);
    fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Bind failed on port %d", config.port);
        close(serverSocket_);
        serverSocket_ = -1;
        return;
    }

    if (listen(serverSocket_, 4) < 0) {
        ESP_LOGE(TAG, "Listen failed");
        close(serverSocket_);
        serverSocket_ = -1;
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", config.port);
}

void WifiGatewayRole::acceptNewClients() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientSocket =
        accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientSocket < 0) {
        return;  // No pending connection (non-blocking)
    }

    // Set client socket non-blocking
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

    // Find a free slot
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets_[i] < 0) {
            clientSockets_[i] = clientSocket;
            ESP_LOGI(TAG, "Client connected (slot %d)", i);
            return;
        }
    }

    // No free slots, close the connection
    ESP_LOGW(TAG, "Max clients reached, rejecting connection");
    close(clientSocket);
}

void WifiGatewayRole::sendToClients(const char* data, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clientSockets_[i] >= 0) {
            int sent = send(clientSockets_[i], data, len, MSG_NOSIGNAL);
            if (sent < 0) {
                ESP_LOGI(TAG, "Client disconnected (slot %d)", i);
                close(clientSockets_[i]);
                clientSockets_[i] = -1;
            }
        }
    }
}

void WifiGatewayRole::onN2kMessage(unsigned long pgn, unsigned char source,
                                    unsigned char priority, int dataLen,
                                    const unsigned char* data,
                                    unsigned long msgTime) {
    char buffer[256];
    size_t len = encodeSeasmart(pgn, source, dataLen, data, msgTime, buffer,
                                sizeof(buffer));
    if (len > 0) {
        sendToClients(buffer, len);
    }
}
