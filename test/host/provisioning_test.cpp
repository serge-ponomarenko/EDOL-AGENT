#include <cassert>
#include <iostream>
#include <vector>
#include "setup/ProvisioningFlow.h"

class Actions : public ProvisioningActions
{
public:
    std::vector<SetupStage> calls;
    ProvisioningResult result = ProvisioningResult::Success;
    uint32_t duration = 0;
    uint32_t completionTime(uint32_t startedAt) const override { return startedAt + duration; }
    ProvisioningResult run(SetupStage stage, const char *code) override
    {
        calls.push_back(stage);
        if (stage == SetupStage::Enrollment) assert(std::strcmp(code, "0123456789AB") == 0);
        return result;
    }
};

int main()
{
    Actions actions;
    ProvisioningFlow flow;
    assert(!flow.start("", false, 0));
    assert(!flow.start("invalid-code", false, 0));
    assert(flow.start("0123456789AB", false, 0));
    const std::vector<SetupStage> expected = {SetupStage::Wifi, SetupStage::Clock, SetupStage::Tls,
        SetupStage::Enrollment, SetupStage::MqttCredentials, SetupStage::MqttConnect};
    for (auto stage : expected)
    {
        assert(flow.stage() == stage);
        flow.tick(actions, 1000);
        if (stage == SetupStage::Enrollment) assert(!flow.hasCode());
    }
    assert(actions.calls == expected && flow.stage() == SetupStage::Ready);

    // Each pre-send transient failure retries the same stage without re-entering settings/code.
    const ProvisioningResult transient[] = {ProvisioningResult::WifiTimeout, ProvisioningResult::ClockTimeout,
        ProvisioningResult::TlsFailed, ProvisioningResult::RateLimited};
    for (unsigned index = 0; index < 4; ++index)
    {
        ProvisioningFlow retry;
        Actions fail;
        assert(retry.start("0123456789AB", false, 0));
        for (unsigned step = 0; step < index; ++step) retry.tick(fail, 0);
        const auto failedStage = retry.stage();
        const auto before = fail.calls.size();
        fail.result = transient[index];
        retry.tick(fail, 1);
        assert(retry.waiting() && !retry.blocked() && retry.hasCode());
        retry.tick(fail, 10000);
        assert(fail.calls.size() == before + 1 && retry.stage() == failedStage);
        fail.result = ProvisioningResult::Success;
        retry.tick(fail, 15001);
        assert(fail.calls.size() == before + 2 && retry.stage() != failedStage);
    }
    for (auto failure : {ProvisioningResult::CodeRejected, ProvisioningResult::EnrollmentUncertain})
    {
        ProvisioningFlow rejected;
        Actions fail;
        rejected.start("0123456789AB", false, 0);
        fail.result = failure;
        rejected.tick(fail, 1);
        assert(rejected.blocked() && !rejected.hasCode() && !rejected.retry(2));
    }
    ProvisioningFlow slow;
    Actions timeout;
    timeout.duration = 20000;
    timeout.result = ProvisioningResult::WifiTimeout;
    slow.start("0123456789AB", false, 0);
    slow.tick(timeout, 1);
    slow.tick(timeout, 20001);
    assert(timeout.calls.size() == 1); // Backoff starts after, not before, the blocking step.
    slow.tick(timeout, 35001);
    assert(timeout.calls.size() == 2);
    ProvisioningFlow expired;
    Actions expiry;
    expired.start("0123456789AB", false, UINT32_MAX - 100000);
    expired.tick(expiry, 200000);
    assert(expired.blocked() && !expired.hasCode() && expiry.calls.empty());

    // Blank resubmission retains the original deadline, not a new five-minute lease.
    ProvisioningFlow resaved;
    resaved.start("0123456789AB", false, 0);
    assert(resaved.start("", false, 290000));
    resaved.tick(expiry, 300000);
    assert(resaved.blocked() && !resaved.hasCode());

    // Existing issued identity skips enrollment and never requires a code for network edits.
    ProvisioningFlow existing;
    Actions resume;
    assert(existing.start("", true, 0));
    for (int i = 0; i < 5; ++i) existing.tick(resume, 1000);
    assert(existing.stage() == SetupStage::Ready);
    for (auto stage : resume.calls) assert(stage != SetupStage::Enrollment);

    ProvisioningFlow uncertain;
    Actions blocked;
    uncertain.start("", true, 0);
    for (int i = 0; i < 3; ++i) uncertain.tick(blocked, 1);
    blocked.result = ProvisioningResult::MqttUncertain;
    uncertain.tick(blocked, 1);
    assert(uncertain.blocked() && !uncertain.retry(2));
    ProvisioningFlow restarted;
    Actions noReplay;
    restarted.restoreMqttFailure();
    restarted.tick(noReplay, 1);
    assert(restarted.blocked() && !restarted.hasCode() && noReplay.calls.empty());
    assert(restarted.result() == ProvisioningResult::MqttUncertain);
    assert(restarted.start("0123456789AB", true, 2));
    std::cout << "Setup sequencing, transient retry, code lifetime and identity recovery tests passed\n";
}
