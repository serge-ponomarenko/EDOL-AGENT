#include <cassert>
#include <iostream>
#include "agent/AgentService.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
FakeSocket socketState;
uint32_t fakeMillis = 0;

enum class Failure { NONE, CONNECT, LOGIN, PROTECTION, DIRECTORY, SIZE, BEGIN, STREAM, FINISH, AUTH, FRAME };
Failure failure = Failure::NONE;
unsigned ftpStops = 0, cameraStops = 0, httpStops = 0, finishes = 0;
unsigned completed = 0, failed = 0;

String ConfigManager::getFtpsDirectory() { return "/"; }
String ConfigManager::getEdolAgentId() { return "example-agent"; }
EdolMqttClient::EdolMqttClient() = default;
AgentEventPublisher::AgentEventPublisher(EdolMqttClient &client) : mqttClient_(client) {}
void AgentEventPublisher::publishModelUploadStarted(const String &) {}
void AgentEventPublisher::publishModelUploadCompleted(const String &) { ++completed; }
void AgentEventPublisher::publishModelUploadFailed(const String &, const char *) { ++failed; }

bool FtpsClient::connect() { return failure != Failure::CONNECT; }
bool FtpsClient::login() { return failure != Failure::LOGIN; }
bool FtpsClient::initializeProtection() { return failure != Failure::PROTECTION; }
bool FtpsClient::changeDirectory(const char *) { return failure != Failure::DIRECTORY; }
bool FtpsClient::getFileSize(const char *, size_t &size) { size = 3; return failure != Failure::SIZE; }
bool FtpsClient::downloadFile(const char *, Stream &) { return failure != Failure::STREAM; }
void FtpsClient::disconnect() { ++ftpStops; }

bool HttpUploadClient::begin(const char *, size_t) { return failure != Failure::BEGIN; }
bool HttpUploadClient::beginSnapshotUpload(const char *, size_t) { return failure != Failure::BEGIN; }
size_t HttpUploadClient::write(const uint8_t *, size_t count) { return count; }
size_t HttpUploadClient::write(uint8_t) { return 1; }
bool HttpUploadClient::finish(uint16_t) { ++finishes; return failure != Failure::FINISH; }
void HttpUploadClient::abort() { ++httpStops; }

bool CameraClient::connect() { return failure != Failure::CONNECT; }
bool CameraClient::authenticate() { return failure != Failure::AUTH; }
size_t CameraClient::readSnapshotSize() { return failure == Failure::FRAME ? 0 : 3; }
void CameraClient::disconnect() { ++cameraStops; }
bool CameraClient::streamSnapshot(uint32_t, std::function<bool(const uint8_t *, size_t)> handler)
{
    const uint8_t bytes[] = {1, 2, 3};
    return failure != Failure::STREAM && handler(bytes, sizeof(bytes));
}

int main()
{
    FtpsClient ftp;
    HttpUploadClient http;
    CameraClient camera;
    EdolMqttClient mqtt;
    AgentEventPublisher events(mqtt);
    AgentService agent(ftp, http, camera, events);
    for (auto stage : {Failure::CONNECT, Failure::LOGIN, Failure::PROTECTION,
                       Failure::DIRECTORY, Failure::SIZE, Failure::BEGIN,
                       Failure::STREAM, Failure::FINISH})
    {
        failure = stage;
        unsigned oldStops = ftpStops, oldHttpStops = httpStops, oldFailed = failed;
        assert(!agent.uploadModel("example.3mf"));
        assert(ftpStops > oldStops && httpStops > oldHttpStops);
        assert(failed == oldFailed + 1);
        // A second operation proves the previous operation released its lock.
        failure = Failure::NONE;
        unsigned oldCompleted = completed;
        assert(agent.uploadModel("example.3mf"));
        assert(completed == oldCompleted + 1);
    }
    for (auto stage : {Failure::CONNECT, Failure::AUTH, Failure::FRAME,
                       Failure::BEGIN, Failure::STREAM, Failure::FINISH})
    {
        failure = stage;
        unsigned oldStops = cameraStops, oldHttpStops = httpStops, oldFinishes = finishes;
        assert(!agent.uploadSnapshot());
        assert(!agent.isCameraHealthy());
        assert(cameraStops > oldStops && httpStops > oldHttpStops);
        if (stage == Failure::STREAM) { assert(finishes == oldFinishes); }
        failure = Failure::NONE;
        assert(agent.uploadSnapshot());
        assert(agent.isCameraHealthy());
    }
    std::cout << "PASS: model/snapshot failure paths, HTTP health, resource and operation release\n";
}
