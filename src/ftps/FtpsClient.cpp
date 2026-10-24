#include "FtpsClient.h"
#include "FtpReply.h"

#include <Arduino.h>
#include <cerrno>
#include <climits>
#include "../util/BulkAdmission.h"

#include "../config/ConfigManager.h"

bool FtpsClient::connect()
{
    disconnect();
    if (!bulkAdmission("ftps-control")) return fail("MEMORY_ADMISSION");

    control_.setInsecure();
    data_.setInsecure();

    control_.setHandshakeTimeout(10);
    data_.setHandshakeTimeout(10);
    control_.setTimeout(10);
    data_.setTimeout(10);

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

    if (!bulkAdmission("ftps-data")) return fail("MEMORY_ADMISSION");
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

    String number = lastReply_.substring(4);
    if (number.length() == 0) return fail("SIZE_PARSE");
    for (size_t i = 0; i < number.length(); ++i)
        if (number.c_str()[i] < '0' || number.c_str()[i] > '9') return fail("SIZE_PARSE");
    errno = 0;
    char *end = nullptr;
    unsigned long parsed = strtoul(number.c_str(), &end, 10);
    if (errno == ERANGE || !end || *end || parsed == 0 || parsed > 32UL * 1024 * 1024)
        return fail("SIZE_LIMIT");
    size = static_cast<size_t>(parsed);
    return true;
}

bool FtpsClient::downloadFile(
    const char *remoteFile,
    Stream &output, size_t expectedSize)
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

    if (expectedSize == 0 || expectedSize > 32UL * 1024 * 1024) { disconnect(); return fail("SIZE_LIMIT"); }
    uint32_t lastData = millis();
    const uint32_t started = lastData;
    while (transferred_ < expectedSize)
    {
        if (millis() - started >= 120000) { disconnect(); return fail("TRANSFER_TIMEOUT"); }
        const int available = data_.available();
        if (available < 0) { disconnect(); return fail("DATA_READ"); }
        if (available > 0)
        {
            const size_t count = std::min(sizeof(buffer), expectedSize - transferred_);
            int bytesRead = data_.read(buffer, count);
            if (bytesRead < 0) { disconnect(); return fail("DATA_READ"); }
            if (bytesRead > 0)
            {
                if (output.write(buffer, bytesRead) != static_cast<size_t>(bytesRead))
                { disconnect(); return fail("HTTP_STREAM_WRITE"); }
                transferred_ += static_cast<size_t>(bytesRead);
                lastData = millis();
            }
        }
        else if (!data_.connected()) { disconnect(); return fail("SIZE_MISMATCH"); }
        if (millis() - lastData >= 10000) { disconnect(); return fail("DATA_TIMEOUT"); }
        delay(1);
    }
    // Never probe TLS read() after the declared payload. SDK available() may
    // return a negative close/error value which used to enter while(available()).
    if (data_.available() > 0) { disconnect(); return fail("SIZE_MISMATCH"); }
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
