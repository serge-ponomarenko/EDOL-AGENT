#include "FtpsClient.h"
#include "FtpReply.h"

#include <Arduino.h>

#include "../config/ConfigManager.h"

bool FtpsClient::connect()
{
    disconnect();

    control_.setInsecure();
    data_.setInsecure();

    control_.setTimeout(10000);
    data_.setTimeout(10000);

    if (!control_.connect(
            ConfigManager::getBambuPrinterHost().c_str(),
            ConfigManager::getFtpsPort()))
    {

        Serial.println("FTPS connect failed");
        return false;
    }

    int code = readReply();

    return code == 220;
}

bool FtpsClient::login()
{
    int code = sendCommand("USER", ConfigManager::getFtpsUser().c_str());

    if (code != 331)
    {
        return false;
    }

    code = sendCommand("PASS", ConfigManager::getBambuAccessCode().c_str());

    return code == 230;
}

bool FtpsClient::initializeProtection()
{
    int code = sendCommand("PBSZ", "0");

    if (code != 200)
    {
        return false;
    }

    code = sendCommand("PROT", "P");

    if (code != 200)
    {
        return false;
    }

    code = sendCommand("TYPE", "I");

    return code == 200;
}

bool FtpsClient::changeDirectory(const char *path)
{
    return sendCommand("CWD", path) == 250;
}

bool FtpsClient::enterPassiveMode(PassiveEndpoint &endpoint)
{
    int code = sendCommand("PASV");

    if (code != 227)
    {
        return fail("PASV_REPLY");
    }

    uint8_t tuple[6];
    if (!FtpReply::passive(lastReply_.c_str(), tuple)) return fail("PASV_PARSE");
    endpoint.ip = IPAddress(tuple[0], tuple[1], tuple[2], tuple[3]);
    endpoint.port = static_cast<uint16_t>((tuple[4] << 8) | tuple[5]);

    return true;
}

bool FtpsClient::openDataConnection(const PassiveEndpoint &endpoint)
{
    data_.stop();

    if (!data_.connect(endpoint.ip, endpoint.port))
    {
        Serial.println("Data connection failed");
        return fail("DATA_CONNECT");
    }

    return true;
}

bool FtpsClient::listFiles()
{
    PassiveEndpoint endpoint;

    if (!enterPassiveMode(endpoint))
    {
        return false;
    }

    if (!openDataConnection(endpoint))
    {
        return false;
    }

    int code = sendCommand("LIST");

    if (code != 125 && code != 150)
    {
        Serial.printf("LIST failed: %d\n", code);
        data_.stop();
        return false;
    }

    while (data_.connected())
    {
        while (data_.available())
        {
            char c = static_cast<char>(data_.read());
            Serial.write(c);
        }

        delay(1);
    }

    data_.stop();

    code = readReply();

    return code == 226;
}

bool FtpsClient::getFileSize(
    const char *remoteFile,
    size_t &size)
{
    int code = sendCommand(
        "SIZE",
        remoteFile);

    if (code != 213)
    {
        return false;
    }

    size = atol(
        lastReply_.substring(4).c_str());

    return true;
}

bool FtpsClient::downloadFile(
    const char *remoteFile,
    Stream &output)
{
    failure_ = "NONE";
    transferred_ = 0;
    PassiveEndpoint endpoint;

    if (!enterPassiveMode(endpoint))
    {
        return false;
    }

    if (!openDataConnection(endpoint))
    {
        return false;
    }

    int code = sendCommand("RETR", remoteFile);

    if (code != 125 && code != 150)
    {
        Serial.printf("RETR failed: %d\n", code);

        data_.stop();

        return fail("RETR_REPLY");
    }

    uint8_t buffer[1024];

    uint32_t lastData = millis();
    while (data_.connected() || data_.available())
    {
        while (data_.available())
        {
            int bytesRead = data_.read(
                buffer,
                sizeof(buffer));

            if (bytesRead < 0) { disconnect(); return fail("DATA_READ"); }
            if (bytesRead > 0)
            {
                lastData = millis();
                size_t written =
                    output.write(
                        buffer,
                        bytesRead);

                if (written != static_cast<size_t>(bytesRead))
                {
                    Serial.println("Output stream write failed");
                    disconnect();
                    return fail("HTTP_STREAM_WRITE");
                }
                transferred_ += static_cast<size_t>(bytesRead);
            }
            else break;
        }

        if (millis() - lastData >= 10000) { disconnect(); return fail("DATA_TIMEOUT"); }

        delay(1);
    }

    data_.stop();

    code = readReply();

    return code == 226 || fail("COMPLETION_REPLY");
}

void FtpsClient::disconnect()
{
    data_.stop();
    control_.stop();
}

int FtpsClient::sendCommand(
    const char *command,
    const char *argument)
{
    String cmd = String(command);

    if (argument != nullptr)
    {
        cmd += " ";
        cmd += argument;
    }

    cmd += "\r\n";

    Serial.println("FTP command sent");

    writeCommand(cmd);

    return readReply();
}

int FtpsClient::sendCommand(const char *command)
{
    return sendCommand(command, nullptr);
}

int FtpsClient::readReply()
{
    char line[512];
    size_t used = 0, total = 0;
    int multiline = -1;
    uint32_t started = millis();
    replyCode_ = -1;
    while (millis() - started < 10000 && total < 4096)
    {
        if (!control_.available())
        {
            if (!control_.connected()) break;
            delay(1);
            continue;
        }
        int value = control_.read();
        if (value < 0) break;
        ++total;
        if (value == '\r') continue;
        if (value != '\n')
        {
            if (used + 1 >= sizeof(line)) break;
            line[used++] = static_cast<char>(value);
            continue;
        }
        line[used] = '\0';
        int code = used >= 3 ? FtpReply::code(line) : -1;
        if (multiline < 0 && code >= 0 && used > 3 && line[3] == '-') multiline = code;
        else if (code >= 0 && (used == 3 || line[3] == ' ') && (multiline < 0 || code == multiline))
        {
            lastReply_ = line;
            replyCode_ = code;
            Serial.printf("FTP reply code=%d\n", code);
            return code;
        }
        used = 0;
    }
    lastReply_ = "";
    Serial.println("FTP reply unavailable, oversized or incomplete");
    return -1;
}

void FtpsClient::writeCommand(const String &command)
{
    control_.write(
        reinterpret_cast<const uint8_t *>(command.c_str()),
        command.length());

    control_.flush();
}
