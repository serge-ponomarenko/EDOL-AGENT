#include <cassert>
#include <iostream>
#include "ftps/FtpsClient.h"
#include "ftps/FtpReply.h"
#include "util/DeferredTailStream.h"
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
    assert(ftp.downloadFile("Куб.gcode.3mf", output, 3));
    assert(output.bytes == "abc" && ftp.transferredBytes() == 3);
    assert(ftpControl.commands == "PASV\r\nRETR Куб.gcode.3mf\r\n");

    reset("227 incomplete\r\n");
    assert(!ftp.downloadFile("model.3mf", output, 3));
    assert(std::strcmp(ftp.lastFailure(), "PASV_PARSE") == 0);
    reset("227 (192,0,2,1,10,5)\r\n550 Missing\r\n");
    assert(!ftp.downloadFile("model.3mf", output, 3) && ftp.lastReplyCode() == 550);
    assert(std::strcmp(ftp.lastFailure(), "RETR_REPLY") == 0);
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n");
    ftpData.input.clear();
    assert(!ftp.downloadFile("model.3mf", output, 3) && fakeMillis >= 10000);
    assert(std::strcmp(ftp.lastFailure(), "DATA_TIMEOUT") == 0);
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n");
    output.shortWrite = true;
    assert(!ftp.downloadFile("model.3mf", output, 3));
    assert(std::strcmp(ftp.lastFailure(), "HTTP_STREAM_WRITE") == 0);
    output.shortWrite = false;
    reset("213 37722\r\n");
    size_t size = 0;
    assert(ftp.getFileSize("Куб.gcode.3mf", size) && size == 37722);
    for (const char *bad : {"213 -1\r\n", "213 0\r\n", "213 abc\r\n", "213 99999999999999999999999\r\n"}) {
        reset(bad);
        assert(!ftp.getFileSize("model", size));
    }
    for (const char *completion : {"226 Done", "426 Failed", "250 Wrong", ""}) {
        std::string replies = std::string("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n") + completion + "\r\n";
        reset(replies.c_str());
        ftpData.negativeEof = true; // Real SDK: available() becomes -1 on TLS closure.
        output.bytes.clear();
        assert(ftp.downloadFile("Куб.gcode.3mf", output, 3) == (std::string(completion) == "226 Done"));
        assert(output.bytes == "abc" && ftp.transferredBytes() == 3);
    }
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n226 Done\r\n");
    assert(!ftp.downloadFile("model", output, 4)); // Truncated body, even with 226.
    assert(std::strcmp(ftp.lastFailure(), "SIZE_MISMATCH") == 0);
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n226 Done\r\n");
    assert(!ftp.downloadFile("model", output, 2)); // Excess body.
    reset("227 (192,0,2,1,10,5)\r\n150 Transfer\r\n226 Done\r\n");
    ftpData.negativeRead = true;
    assert(!ftp.downloadFile("model", output, 3));
    assert(std::strcmp(ftp.lastFailure(), "DATA_READ") == 0);
    reset("220 Ready\r\n");
    fakeLargestBlock() = 16383;
    assert(!ftp.connect() && !ftpControl.connected && !ftpData.connected);
    assert(std::strcmp(ftp.lastFailure(), "MEMORY_ADMISSION") == 0);
    fakeLargestBlock() = 100000;
    for (bool complete : {true, false}) {
        reset(complete ? "227 (192,0,2,1,10,5)\r\n150 Transfer\r\n226 Done\r\n"
                       : "227 (192,0,2,1,10,5)\r\n150 Transfer\r\n426 Failed\r\n");
        ftpData.input.assign(37722, 'g');
        ftpData.negativeEof = true;
        output.bytes.clear();
        DeferredTailStream tail(output);
        assert(ftp.downloadFile("Куб.gcode.3mf", tail, 37722) == complete);
        assert(ftp.transferredBytes() == 37722 && output.bytes.size() == 37721);
        if (complete) assert(tail.commit() && output.bytes.size() == 37722);
    }
    std::cout << "FTPS parser, multiline replies, Unicode transfer, buffered EOF and failure stages PASS\n";
}
