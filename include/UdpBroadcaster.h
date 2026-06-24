#pragma once

#include "TcpServer.h"

#ifdef ESP32
#include <lwip/sockets.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

// UDP broadcast implementation of TcpServerInterface.
// Drop-in replacement for TcpServer in WifiGatewayRole.
// Sends Actisense frames as UDP datagrams to 255.255.255.255.
// loop() is a no-op — UDP is connectionless, no clients to manage.
class UdpBroadcaster : public TcpServerInterface {
   public:
    bool start(uint16_t port) override;
    void stop() override;
    void loop() override {}
    void sendToAll(const char* data, size_t len) override;

   private:
    int sock_ = -1;
    struct sockaddr_in dest_ = {};
};
