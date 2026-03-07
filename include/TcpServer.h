#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>

#ifdef ESP32
#include <lwip/sockets.h>
#else
#include <sys/socket.h>
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

class TcpServerInterface {
   public:
    virtual bool start(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    virtual void sendToAll(const char* data, size_t len) = 0;
    virtual ~TcpServerInterface() = default;
};

// Sends a frame. Returns false only on a hard socket error (ECONNRESET, EPIPE,
// etc.) — caller should close the client. Returns true for all other outcomes:
//   • full send (n == len) — success
//   • partial write (0 < n < len) — some bytes sent; stream may be slightly
//     corrupted for this frame but connection is kept (Actisense DLE+STX lets
//     the parser resync on the next frame; a disconnect would be worse)
//   • EAGAIN — buffer full, frame dropped cleanly, client kept
inline bool tcpSendFrame(int fd, const char* data, size_t len) {
    if (len == 0) return true;
    int n = send(fd, data, (int)len, MSG_NOSIGNAL);
    if (n > 0) return true;  // full or partial send — keep connection
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
    return false;  // hard error: ECONNRESET, EPIPE, etc.
}

// Drains the receive buffer to detect dead clients.
//   true  → client still alive (no data pending, or data drained)
//   false → client gone (FIN received or socket error)
inline bool tcpDrainRecv(int fd) {
    char buf[64];
    for (;;) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) continue;
        if (n == 0) return false;  // FIN
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        return false;  // error
    }
}

#ifdef ESP32
class TcpServer : public TcpServerInterface {
   public:
    bool start(uint16_t port) override;
    void stop() override;
    void loop() override;
    void sendToAll(const char* data, size_t len) override;

   private:
    int serverSocket_ = -1;
    static constexpr int MAX_CLIENTS = 8;
    int clientSockets_[MAX_CLIENTS] = {-1, -1, -1, -1, -1, -1, -1, -1};

    void acceptNewClients();
};
#endif
