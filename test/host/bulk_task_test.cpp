#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
#include "agent/BulkTransferTask.h"
#include "config/ConfigManager.h"
#if EDOL_AGENT_SECURE_BUILD
#include "security/AgentCredentialStore.h"
#include "security/AgentMqttEnrollmentClient.h"
#endif

FakeSerial Serial;
FakeSocket socketState;
uint32_t fakeMillis = 0;
std::thread worker;
std::thread::id mqttOwner;
std::atomic<bool> entered{false}, releaseTransfer{false}, stopWorker{false}, finished{false};
unsigned taskCount = 0;
struct StopWorker {};
int xTaskCreate(void (*entry)(void *), const char *, unsigned stack, void *context, unsigned priority, void *)
{
    assert(stack == 12288 && priority == 0);
    ++taskCount;
    worker = std::thread([=]() { try { entry(context); } catch (const StopWorker &) {} });
    return 1;
}
void vTaskDelay(unsigned) {
    if (stopWorker.load()) throw StopWorker{};
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
size_t HttpUploadClient::write(uint8_t) { return 1; }
size_t HttpUploadClient::write(const uint8_t *, size_t size) { return size; }
FakeMqttState mqttState;
String ConfigManager::getBambuPrinterHost() { return "synthetic.invalid"; }
String ConfigManager::getBambuAccessCode() { return "synthetic"; }
String ConfigManager::getBambuPrinterSerial() { return "synthetic-serial"; }
uint16_t ConfigManager::getBambuMqttPort() { return 8883; }
String ConfigManager::getEdolMqttHost() { return "synthetic.invalid"; }
uint16_t ConfigManager::getEdolMqttPort() { return 1883; }
String ConfigManager::getEdolAgentId() { return "synthetic-agent"; }
String ConfigManager::getEdolAgentPassword() { return "synthetic"; }
#if EDOL_AGENT_SECURE_BUILD
bool AgentCredentialStore::load(AgentIdentity &identity) {
    identity.id = "00000000-0000-4000-8000-000000000001";
    identity.secret = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return true;
}
bool AgentCredentialStore::loadMqtt(String &secret) { secret = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"; return true; }
bool AgentMqttEnrollmentClient::provision() { assert(false); return false; }
bool SecureTransport::synchronizeClock() { return true; }
bool SecureTransport::prepare(SecureTransport::Client &) { return true; }
#endif
EdolMqttClient *edolLink;
void forwardTelemetry(const char *, const byte *payload, unsigned length) {
    assert(edolLink->publish("synthetic-report", payload, length));
}
AgentService::AgentService(FtpsClient &ftp, HttpUploadClient &http, CameraClient &camera,
                          AgentEventPublisher &events, BambuMqttClient &printer)
    : ftpsClient_(ftp), uploadClient_(http), cameraClient_(camera), eventPublisher_(events), printerTelemetry_(printer) {}
bool AgentService::uploadModel(const String &) { return uploadSnapshot(); }
bool AgentService::uploadSnapshot() {
    assert(std::this_thread::get_id() != mqttOwner);
    eventPublisher_.publishModelUploadStarted("model.3mf");
    entered.store(true);
    while (!releaseTransfer.load()) std::this_thread::yield();
    eventPublisher_.publishModelUploadFailed("model.3mf", "SYNTHETIC_TLS_ALLOCATION_FAILURE");
    finished.store(true);
    return false; // TLS allocation/read failures do not touch MQTT.
}
int main() {
    mqttOwner = std::this_thread::get_id();
    FtpsClient ftp; HttpUploadClient http; CameraClient camera;
    EdolMqttClient mqtt; BambuMqttClient printer; AgentEventPublisher events(mqtt);
    FakeMqttState printerState, edolState;
    PubSubClient::instances()[0]->useState(edolState);
    PubSubClient::instances()[1]->useState(printerState);
    auto owner = []() { assert(std::this_thread::get_id() == mqttOwner); };
    printerState.ownerCheck = owner; edolState.ownerCheck = owner;
    edolLink = &mqtt;
    printer.setMessageHandler(forwardTelemetry);
    BulkTransferTask *taskPointer = nullptr;
    mqtt.setMessageHandler([&](const char *, const uint8_t *payload, unsigned length) {
        assert(printer.publish("synthetic-request", payload, length));
        if (taskPointer) assert(!taskPointer->request());
    });
    assert(printer.connect() && mqtt.connect());
    AgentService service(ftp, http, camera, events, printer);
    BulkTransferTask task(service);
    taskPointer = &task;
    assert(task.begin() && task.begin() && taskCount == 1);
    assert(task.request("model.3mf"));
    while (!entered.load()) std::this_thread::yield();
    unsigned printerLoops = 0, edolLoops = 0, printerCommands = 0;
    for (unsigned i = 0; i < 1000; ++i) {
        assert(std::this_thread::get_id() == mqttOwner);
        printerState.hasMessage = true; printerState.topic = "synthetic-report"; printerState.payload = "report";
        edolState.hasMessage = true; edolState.topic = "synthetic-command"; edolState.payload = "pause";
        printer.loop(); mqtt.loop(); events.drain();
        assert(printer.isConnected() && mqtt.isConnected());
        ++printerLoops; ++edolLoops; ++printerCommands;
        assert(!task.request());
    }
    assert(printerLoops == 1000 && edolLoops == 1000 && printerCommands == 1000);
    assert(printerState.loops == 1000 && edolState.loops == 1000);
    assert(printerState.publishes == 1000 && edolState.publishes == 1001);
    assert(printerState.disconnects == 0 && edolState.disconnects == 0);
    releaseTransfer.store(true);
    while (!finished.load()) std::this_thread::yield();
    stopWorker.store(true);
    worker.join();
    edolState.connected = false;
    events.drain();
    assert(events.hasPending() && edolState.publishes == 1001);
    edolState.connected = true;
    events.drain();
    assert(!events.hasPending() && edolState.publishes == 1002);
    edolState.publishOk = false;
    events.publishModelUploadFailed("model.3mf", "SYNTHETIC");
    events.drain();
    assert(events.hasPending());
    fakeMillis += 1000; events.drain(); assert(events.hasPending());
    fakeMillis += 1000; events.drain(); assert(!events.hasPending());
    edolState.connected = false;
    events.publishModelUploadFailed("model.3mf", "SYNTHETIC");
    fakeMillis += 60000; events.drain();
    assert(!events.hasPending());
    task.collect();
    assert(!task.request()); // Failure has the same cooldown; no retry queue.
    std::cout << "PASS: production bulk task isolates blocking I/O, single worker, bounded requests and failure cooldown\n";
}
