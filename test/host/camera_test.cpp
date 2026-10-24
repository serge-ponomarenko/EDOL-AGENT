#include <cassert>
#include <iostream>
#include "camera/CameraClient.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
uint32_t fakeMillis = 0;
FtpSocketFixture ftpControl, ftpData;
unsigned ftpSocketCount = 0;
String ConfigManager::getBambuPrinterHost() { return "synthetic.invalid"; }
String ConfigManager::getBambuAccessCode() { return "synthetic"; }

void frame(const std::string &bytes) {
    ftpControl = FtpSocketFixture{};
    ftpControl.input.assign(16, '\0');
    ftpControl.input[0] = static_cast<char>(bytes.size());
    ftpControl.input += bytes;
    fakeMillis = 0;
}
int main() {
    CameraClient camera;
    frame("abc");
    assert(camera.connect() && camera.authenticate());
    assert(camera.readSnapshotSize() == 3);
    std::string captured;
    assert(camera.streamSnapshot(3, [&](const uint8_t *data, size_t size) {
        captured.append(reinterpret_cast<const char *>(data), size); return true;
    }));
    assert(captured == "abc");
    camera.disconnect();
    assert(!ftpControl.connected);
    assert(!camera.streamSnapshot(256 * 1024 + 1, [](const uint8_t *, size_t) { return true; }));
    frame("abc"); assert(camera.connect()); assert(camera.readSnapshotSize() == 3);
    assert(!camera.streamSnapshot(3, [](const uint8_t *, size_t) { return false; }));
    frame("abc"); assert(camera.connect()); assert(camera.readSnapshotSize() == 3);
    ftpControl.negativeRead = true;
    assert(!camera.streamSnapshot(3, [](const uint8_t *, size_t) { return true; }));
    frame(""); assert(camera.connect()); ftpControl.input.clear();
    assert(camera.readSnapshotSize() == 0 && fakeMillis >= 10000);
    camera.disconnect();
    frame("abc"); ftpControl.connectOk = false;
    assert(!camera.connect());
    std::cout << "PASS: production camera byte count, frame bound, negative read, timeout and stream failure\n";
}
