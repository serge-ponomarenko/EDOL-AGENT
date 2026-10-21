#include <cassert>
#include <iostream>
#include <type_traits>

#include "http/HttpUploadClient.h"
#include "config/ConfigManager.h"
#include "util/ScopeExit.h"

FakeSerial Serial;
FakeSocket socketState;
uint32_t fakeMillis = 0;

String ConfigManager::getCoreHost() { return "core.example.invalid"; }
uint16_t ConfigManager::getCorePort() { return 8080; }
String ConfigManager::getCoreUploadPath() { return "/api/agent/upload"; }
String ConfigManager::getEdolAgentId() { return "example-agent"; }

void reset()
{
    socketState = FakeSocket{};
    fakeMillis = 0;
}

int main()
{
    unsigned cleanupCount = 0;
    auto earlyReturn = [&]() {
        auto cleanup = [&]() { ++cleanupCount; };
        ScopeExit<decltype(cleanup)> guard(cleanup);
        static_assert(!std::is_copy_constructible<decltype(guard)>::value, "No double cleanup");
        return false;
    };
    assert(!earlyReturn());
    assert(cleanupCount == 1);

    HttpUploadClient upload;
    const uint8_t bytes[] = {1, 2, 3};
    reset();
    assert(upload.begin("part name.3mf", sizeof(bytes)));
    assert(socketState.headers.find("POST /api/agent/upload?fileName=part%20name.3mf HTTP/1.1\r\n") == 0);
    assert(socketState.headers.find("X-Agent-Id: example-agent\r\n") != std::string::npos);
    assert(socketState.headers.find("Content-Length: 3\r\n") != std::string::npos);
    assert(upload.write(bytes, 2) == 2);
    assert(upload.write(bytes[2]) == 1);
    assert(upload.finish());
    assert(socketState.body == std::vector<uint8_t>({1, 2, 3}));
    assert(!socketState.connected);

    reset();
    assert(upload.beginSnapshotUpload("example-agent", 3));
    assert(socketState.headers.find("POST /api/agent/camera/snapshot HTTP/1.1\r\n") == 0);
    assert(socketState.headers.find("Content-Type: image/jpeg\r\n") != std::string::npos);
    assert(upload.write(bytes, 3) == 3);
    socketState.status = "HTTP/1.1 500 Failure\r";
    assert(!upload.finish());
    assert(!socketState.connected);

    reset();
    assert(upload.begin("x", 3));
    socketState.writeLimit = 1;
    assert(upload.write(bytes, 3) == 1);
    assert(upload.hasError());
    assert(!upload.finish());
    assert(!socketState.connected);

    reset();
    assert(upload.begin("x", 3));
    assert(upload.write(bytes, 2) == 2);
    assert(!upload.finish());
    assert(!socketState.connected);

    reset();
    assert(upload.begin("x", 0));
    socketState.status = "malformed";
    assert(!upload.finish());
    assert(!socketState.connected);

    reset();
    assert(upload.begin("x", 0));
    socketState.hasResponse = false;
    assert(!upload.finish());
    assert(fakeMillis >= 10000);
    assert(!socketState.connected);

    reset();
    assert(upload.begin("x", 0));
    socketState.hasResponse = false;
    socketState.connected = false;
    assert(!upload.finish());
    assert(fakeMillis == 0);

    reset();
    socketState.connectOk = false;
    assert(!upload.begin("x", 3));
    assert(!socketState.connected);
    assert(socketState.stops >= 2);

    reset();
    assert(upload.begin("x", 3));
    upload.abort();
    assert(!socketState.connected);
    assert(upload.begin("x", 0));
    assert(!upload.hasError());
    assert(upload.finish());

    std::cout << "PASS: HTTP streaming, status/error paths, cleanup and scope ownership\n";
}
