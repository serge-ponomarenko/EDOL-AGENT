#pragma once

#include <WiFiClientSecure.h>

// Arduino-ESP32 2.0.17 zeroes the descriptor in stop_ssl_socket(). Restore the
// invalid sentinel so repeated cleanup cannot close/configure another socket.
class SocketTlsClient : public WiFiClientSecure
{
public:
    void stop() override
    {
        WiFiClientSecure::stop();
        sslclient->socket = -1;
    }
};
