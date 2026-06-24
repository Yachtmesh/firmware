#include "UdpBroadcaster.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#ifdef ESP32
#include <esp_log.h>
static const char* TAG = "UdpBroadcaster";
#endif

bool UdpBroadcaster::start(uint16_t port) {
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ < 0) {
#ifdef ESP32
        ESP_LOGE(TAG, "Failed to create UDP socket");
#endif
        return false;
    }

    int broadcast = 1;
    setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    int flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags | O_NONBLOCK);

    dest_.sin_family      = AF_INET;
    dest_.sin_port        = htons(port);
    dest_.sin_addr.s_addr = INADDR_BROADCAST;

#ifdef ESP32
    ESP_LOGI(TAG, "UDP broadcast ready on port %d", port);
#endif
    return true;
}

void UdpBroadcaster::stop() {
    if (sock_ >= 0) {
        close(sock_);
        sock_ = -1;
    }
}

void UdpBroadcaster::sendToAll(const char* data, size_t len) {
    if (sock_ < 0 || len == 0) return;
    sendto(sock_, data, len, MSG_NOSIGNAL,
           reinterpret_cast<const struct sockaddr*>(&dest_), sizeof(dest_));
}
