#include <cassert>
#include <iostream>
#include "http/HttpUploadClient.h"
#include "config/ConfigManager.h"
#include "security/AgentCredentialStore.h"
#include "security/SecureTransport.h"

FakeSerial Serial;
FakeSocket socketState;
uint32_t fakeMillis = 0;
bool paired = true;
bool trusted = true;
String ConfigManager::getCoreHost() { return "core.example.invalid"; }
uint16_t ConfigManager::getCorePort() { return 443; }
bool AgentCredentialStore::load(AgentIdentity &identity)
{
    identity.id = "00000000-0000-4000-8000-000000000001";
    identity.secret = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return paired;
}
bool SecureTransport::prepare(WiFiClientSecure &) { return trusted; }

int main()
{
    HttpUploadClient upload;
    const uint8_t image[] = {1, 2, 3};
    assert(upload.beginSnapshotUpload("spoofed-legacy-id", sizeof(image)));
    assert(socketState.headers.find("POST /api/agent/v1/camera/snapshot HTTP/1.1\r\n") == 0);
    assert(socketState.headers.find("X-Agent-Id:") == std::string::npos);
    assert(socketState.headers.find("spoofed-legacy-id") == std::string::npos);
    assert(socketState.headers.find("Authorization: EdolAgent 00000000-0000-4000-8000-000000000001.") != std::string::npos);
    assert(upload.write(image, sizeof(image)) == sizeof(image));
    socketState.status = "HTTP/1.1 401 Unauthorized\r";
    assert(!upload.finish());
    assert(!socketState.connected);
    socketState = FakeSocket{};
    assert(upload.begin("part name.3mf", sizeof(image)));
    assert(socketState.headers.find("POST /api/agent/v1/upload?fileName=part%20name.3mf HTTP/1.1\r\n") == 0);
    assert(upload.write(image, sizeof(image)) == sizeof(image));
    assert(upload.finish());
    socketState = FakeSocket{};
    paired = false;
    assert(!upload.begin("part.3mf", 1));
    assert(!socketState.connected && socketState.headers.empty());
    paired = true;
    trusted = false;
    assert(!upload.begin("part.3mf", 1));
    assert(!socketState.connected && socketState.headers.empty());
    trusted = true;
    socketState.connectOk = false;
    assert(!upload.begin("part.3mf", 1));
    assert(socketState.headers.empty());
    std::cout << "Secure upload routing and fail-closed tests passed (fake sockets, no TLS acceptance)\n";
}
