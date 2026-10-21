#pragma once

class WiFiManager
{
public:
    bool connect();
    bool isConnected();
    void maintainConnection();
    bool connectionLost();
};