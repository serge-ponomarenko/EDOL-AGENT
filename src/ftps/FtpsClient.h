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
    const char *lastFailure() const { return failure_; }
    int lastReplyCode() const { return replyCode_; }
    size_t transferredBytes() const { return transferred_; }

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
    const char *failure_ = "NONE";
    int replyCode_ = -1;
    size_t transferred_ = 0;
    bool fail(const char *stage) { failure_ = stage; return false; }
};
