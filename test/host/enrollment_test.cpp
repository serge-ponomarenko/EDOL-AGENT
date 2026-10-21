#include <cassert>
#include <iostream>
#include "security/AgentEnrollmentClient.h"
#include "security/AgentCredentialStore.h"
#include "security/PairingProtocol.h"
#include "security/SecureTransport.h"
#include "config/ConfigManager.h"
#include <HTTPClient.h>

FakeSerial Serial;
FakeSocket socketState;
FakeHttpState httpState;
uint32_t fakeMillis = 0;
bool clockReady = true;
bool trustReady = true;
bool storageOk = true;
bool stored = false;
String host = "core.example.invalid";
String ConfigManager::getCoreHost() { return host; }
uint16_t ConfigManager::getCorePort() { return 443; }
bool SecureTransport::synchronizeClock() { return clockReady; }
bool SecureTransport::prepare(WiFiClientSecure &) { return trustReady; }
bool AgentCredentialStore::save(const char *id, const char *secret)
{
    stored = storageOk && PairingProtocol::validIdentity(id, secret);
    return stored;
}

void reset()
{
    httpState = FakeHttpState{};
    socketState = FakeSocket{};
    httpState.response = "{\"agentId\":\"00000000-0000-4000-8000-000000000001\","
        "\"secret\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
    clockReady = trustReady = storageOk = true;
    stored = false;
    host = "core.example.invalid";
}

int main()
{
    reset();
    assert(AgentEnrollmentClient::enroll("0123456789AB"));
    assert(stored && httpState.posts == 1 && httpState.ends == 1);
    assert(httpState.url == "https://core.example.invalid:443/api/agent/v1/enroll");
    assert(httpState.request == "{\"pairingCode\":\"0123456789AB\"}");
    assert(httpState.redirects == HTTPC_DISABLE_FOLLOW_REDIRECTS);
    assert(socketState.stops == 1);
    reset();
    assert(!AgentEnrollmentClient::enroll("bad\r\ncode"));
    assert(httpState.posts == 0 && !stored);
    reset();
    host = "core.example.invalid\r\nHeader: value";
    assert(!AgentEnrollmentClient::enroll("0123456789AB"));
    assert(httpState.posts == 0);
    reset();
    clockReady = false;
    assert(!AgentEnrollmentClient::enroll("0123456789AB"));
    assert(httpState.posts == 0);
    reset();
    trustReady = false;
    assert(!AgentEnrollmentClient::enroll("0123456789AB"));
    assert(httpState.posts == 0);
    for (int status : {401, 429, 500, -1})
    {
        reset();
        httpState.status = status;
        assert(!AgentEnrollmentClient::enroll("0123456789AB"));
        assert(!stored && httpState.posts == 1 && httpState.ends == 1);
    }
    for (const std::string &body : {std::string(1025, 'x'), std::string("not-json"), std::string("{}"),
        std::string("{\"agentId\":null,\"secret\":null}"), std::string("{\"agentId\":\"bad\",\"secret\":\"bad\"}")})
    {
        reset();
        httpState.response = body;
        assert(!AgentEnrollmentClient::enroll("0123456789AB"));
        assert(!stored && httpState.posts == 1 && httpState.ends == 1);
    }
    reset();
    storageOk = false;
    assert(!AgentEnrollmentClient::enroll("0123456789AB"));
    assert(!stored && httpState.posts == 1);
    std::cout << "Enrollment contract and failure tests passed (fake HTTP/TLS, real JSON parser)\n";
}
