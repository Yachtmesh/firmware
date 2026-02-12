#pragma once

#include <cstddef>
#include <cstdint>

class TcpServerInterface {
   public:
    virtual bool start(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    virtual void sendToAll(const char* data, size_t len) = 0;
    virtual ~TcpServerInterface() = default;
};

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
