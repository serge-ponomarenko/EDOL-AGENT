#pragma once

#include <WiFiClientSecure.h>

struct PassiveEndpoint
{
    IPAddress ip;
    uint16_t port;
};

class FtpsClient
{
public:
    bool connect();
    bool login();
    bool initializeProtection();
    bool changeDirectory(const char *path);

    bool enterPassiveMode(PassiveEndpoint &endpoint);

    bool listFiles();

    bool getFileSize(
        const char *remoteFile,
        size_t &size);

    bool downloadFile(
        const char *remoteFile,
        Stream &output);

    void disconnect();

private:
    int sendCommand(const char *command);
    int sendCommand(const char *command, const char *argument);

    // Bambu FTP server requires commands to be sent
    // as a single TLS write(). Using print() may cause
    // authentication failures (PASS -> 332).
    void writeCommand(const String &command);

    int readReply();

    bool openDataConnection(const PassiveEndpoint &endpoint);

    WiFiClientSecure control_;
    String lastReply_;
    WiFiClientSecure data_;
};
