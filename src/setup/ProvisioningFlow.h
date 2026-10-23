#pragma once

#include <cstdint>
#include <cstring>
#include "../security/ProvisioningResult.h"
#include "../security/PairingProtocol.h"

enum class SetupStage { Idle, Wifi, Clock, Tls, Enrollment, MqttCredentials, MqttConnect, Ready };

class ProvisioningActions
{
public:
    virtual ~ProvisioningActions() = default;
    virtual ProvisioningResult run(SetupStage stage, const char *code) = 0;
    virtual uint32_t completionTime(uint32_t startedAt) const { return startedAt; }
};

// The one-time code never enters NVS, HTML, status responses or logs.
class ProvisioningFlow
{
public:
    bool start(const char *code, bool identityReady, uint32_t now)
    {
        if (code == nullptr) code = "";
        if (*code)
        {
            if (!PairingProtocol::validCode(code)) return false;
            clearCode();
            std::memcpy(code_, code, 12);
            codeStarted_ = now;
        }
        expire(now);
        if (!identityReady && !*code_) { fail(ProvisioningResult::CodeRejected); return false; }
        enroll_ = *code_ != '\0';
        stage_ = SetupStage::Wifi;
        waiting_ = false;
        blocked_ = false;
        result_ = ProvisioningResult::Success;
        return true;
    }

    void tick(ProvisioningActions &actions, uint32_t now)
    {
        expire(now);
        if (blocked_ || stage_ == SetupStage::Idle || stage_ == SetupStage::Ready) return;
        if (waiting_ && now - waitStarted_ < 15000) return;
        waiting_ = false;
        result_ = actions.run(stage_, code_);
        if (result_ != ProvisioningResult::Success)
        {
            if (result_ == ProvisioningResult::CodeRejected || result_ == ProvisioningResult::EnrollmentUncertain)
                clearCode();
            blocked_ = result_ == ProvisioningResult::CodeRejected
                || result_ == ProvisioningResult::EnrollmentUncertain
                || result_ == ProvisioningResult::MqttUncertain
                || result_ == ProvisioningResult::InvalidEndpoint;
            waiting_ = !blocked_;
            waitStarted_ = actions.completionTime(now);
            return;
        }
        switch (stage_)
        {
        case SetupStage::Wifi: stage_ = SetupStage::Clock; break;
        case SetupStage::Clock: stage_ = SetupStage::Tls; break;
        case SetupStage::Tls: stage_ = enroll_ ? SetupStage::Enrollment : SetupStage::MqttCredentials; break;
        case SetupStage::Enrollment: clearCode(); enroll_ = false; stage_ = SetupStage::MqttCredentials; break;
        case SetupStage::MqttCredentials: stage_ = SetupStage::MqttConnect; break;
        case SetupStage::MqttConnect: stage_ = SetupStage::Ready; break;
        default: break;
        }
    }

    bool retry(uint32_t now)
    {
        expire(now);
        if (blocked_ || stage_ == SetupStage::Idle || stage_ == SetupStage::Ready) return false;
        waiting_ = false;
        return true;
    }
    bool hasCode() const { return *code_ != '\0'; }
    void restoreMqttFailure()
    {
        clearCode();
        enroll_ = false;
        stage_ = SetupStage::MqttCredentials;
        fail(ProvisioningResult::MqttUncertain);
    }
    bool blocked() const { return blocked_; }
    bool waiting() const { return waiting_; }
    SetupStage stage() const { return stage_; }
    ProvisioningResult result() const { return result_; }
    const char *stageName() const
    {
        switch (stage_)
        {
        case SetupStage::Wifi: return "Wi-Fi";
        case SetupStage::Clock: return "NTP time synchronization";
        case SetupStage::Tls: return "Core TLS verification";
        case SetupStage::Enrollment: return "Agent enrollment";
        case SetupStage::MqttCredentials: return "MQTT credential provisioning";
        case SetupStage::MqttConnect: return "MQTT TLS connection and subscriptions";
        case SetupStage::Ready: return "Setup complete; rebooting into normal operation";
        default: return "Enter settings and select Save & Reboot";
        }
    }

private:
    char code_[13] = {};
    uint32_t codeStarted_ = 0, waitStarted_ = 0;
    SetupStage stage_ = SetupStage::Idle;
    ProvisioningResult result_ = ProvisioningResult::Success;
    bool enroll_ = false, blocked_ = false, waiting_ = false;
    void clearCode()
    {
        volatile char *cursor = code_;
        for (unsigned i = 0; i < sizeof(code_); ++i) cursor[i] = 0;
    }
    void fail(ProvisioningResult result) { result_ = result; blocked_ = true; waiting_ = false; }
    void expire(uint32_t now)
    {
        if (*code_ && now - codeStarted_ >= 300000)
        {
            clearCode();
            fail(ProvisioningResult::CodeRejected);
        }
    }
};
