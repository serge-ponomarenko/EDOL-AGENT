#include "FtpsClient.h"

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
        return false;
    }

    int h1, h2, h3, h4, p1, p2;

    if (sscanf(
            lastReply_.c_str(),
            "227 (%d,%d,%d,%d,%d,%d)",
            &h1,
            &h2,
            &h3,
            &h4,
            &p1,
            &p2) != 6)
    {
        return false;
    }

    endpoint.ip = IPAddress(h1, h2, h3, h4);
    endpoint.port = (p1 << 8) | p2;

    return true;
}

bool FtpsClient::openDataConnection(const PassiveEndpoint &endpoint)
{
    data_.stop();

    if (!data_.connect(endpoint.ip, endpoint.port))
    {
        Serial.println("Data connection failed");
        return false;
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
            char c = data_.read();
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

        return false;
    }

    uint8_t buffer[1024];

    while (data_.connected())
    {
        while (data_.available())
        {
            int bytesRead = data_.read(
                buffer,
                sizeof(buffer));

            if (bytesRead > 0)
            {
                size_t written =
                    output.write(
                        buffer,
                        bytesRead);

                if (written != static_cast<size_t>(bytesRead))
                {
                    Serial.println("Output stream write failed");
                    disconnect();
                    return false;
                }
            }
        }

        delay(1);
    }

    data_.stop();

    code = readReply();

    return code == 226;
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
    Serial.printf(
        "FTP state before read: connected=%s available=%d\n",
        control_.connected() ? "true" : "false",
        control_.available());

    lastReply_ =
        control_.readStringUntil('\n');

    lastReply_.trim();

    Serial.printf(
        "FTP state after read: connected=%s available=%d\n",
        control_.connected() ? "true" : "false",
        control_.available());

    Serial.println("FTP reply received");

    if (lastReply_.length() < 3)
    {
        Serial.println(
            "FTP invalid reply");

        return -1;
    }

    return lastReply_
        .substring(0, 3)
        .toInt();
}

void FtpsClient::writeCommand(const String &command)
{
    control_.write(
        reinterpret_cast<const uint8_t *>(command.c_str()),
        command.length());

    control_.flush();
}
