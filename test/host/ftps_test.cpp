#include <cassert>
#include <iostream>
#include "ftps/FtpsClient.h"
#include "ftps/FtpReply.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
uint32_t fakeMillis = 0;
FtpSocketFixture ftpControl, ftpData;
unsigned ftpSocketCount = 0;
String ConfigManager::getBambuPrinterHost() { return "synthetic.invalid"; }
String ConfigManager::getBambuAccessCode() { return "synthetic"; }
String ConfigManager::getFtpsUser() { return "bblp"; }
uint16_t ConfigManager::getFtpsPort() { return 990; }

class Output : public Stream
{
public:
    bool shortWrite = false;
    std::string bytes;
    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t *data, size_t size) override
    {
        if (shortWrite) return 0;
        bytes.append(reinterpret_cast<const char *>(data), size);
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};

void reset(const char *replies)
{
    ftpControl = FtpSocketFixture{};
    ftpData = FtpSocketFixture{};
    ftpControl.input = replies;
    ftpControl.connected = true;
    ftpData.input = "abc";
    fakeMillis = 0;
}

int main()
{
    uint8_t tuple[6];
    for (const char *reply : {"227 (192,0,2,1,10,5)", "227 Entering Passive Mode (192,0,2,1,10,5).",
                             "227 Passive 192,0,2,1,10,5"})
        assert(FtpReply::passive(reply, tuple) && tuple[4] == 10 && tuple[5] == 5);
    for (const char *reply : {"227 (999,0,2,1,10,5)", "227 (192,0,2,1,0,0)", "227 (192,0,2,1,-1,5)",
                             "227 (192,0,2,1,10,5,6)", "500 (192,0,2,1,10,5)", "227 incomplete", ""})
        assert(!FtpReply::passive(reply, tuple));

    FtpsClient ftp;
    reset("220-Welcome\r\nPrinter ready\r\n220 Ready\r\n331 Password\r\n230 Logged in\r\n");
    assert(ftp.connect() && ftp.login());
    assert(ftpControl.commands == "USER bblp\r\nPASS synthetic\r\n");

    reset("227 Entering Passive Mode (192,0,2,1,10,5).\r\n150 Transfer\r\n226 Done\r\n");
    ftpData.closeWithBufferedData = true;
    Output output;
    assert(ftp.downloadFile("Куб.gcode.3mf", output));
    assert(output.bytes == "abc" && ftp.transferredBytes() == 3);
    assert(ftpControl.commands == "PASV\r\nRETR Куб.gcode.3mf\r\n");

    reset("227 incomplete\r\n");
    assert(!ftp.downloadFile("model.3mf", output));
    assert(std::strcmp(ftp.lastFailure(), "PASV_PARSE") == 0);
    reset("227 (192,0,2,1,10,5)\r\n550 Missing\r\n");
    assert(!ftp.downloadFile("model.3mf", output) && ftp.lastReplyCode() == 550);
    assert(std::strcmp(ftp.lastFailure(), "RETR_REPLY") == 0);
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n");
    ftpData.input.clear();
    assert(!ftp.downloadFile("model.3mf", output) && fakeMillis >= 10000);
    assert(std::strcmp(ftp.lastFailure(), "DATA_TIMEOUT") == 0);
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n");
    output.shortWrite = true;
    assert(!ftp.downloadFile("model.3mf", output));
    assert(std::strcmp(ftp.lastFailure(), "HTTP_STREAM_WRITE") == 0);
    std::cout << "FTPS parser, multiline replies, Unicode transfer, buffered EOF and failure stages PASS\n";
}
