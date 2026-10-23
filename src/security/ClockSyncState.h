#pragma once

#include <atomic>
#include <cstdint>

// A plausible epoch alone does not establish that SNTP synchronized this boot.
class ClockSyncState
{
public:
    void record(int64_t epoch, uint32_t tick)
    {
        if (epoch <= 0 || epoch > UINT32_MAX) return;
        tick_.store(tick);
        epoch_.store(static_cast<uint32_t>(epoch));
    }

    bool observed() const { return epoch_.load() != 0; }

    bool ready(int64_t now, uint32_t tick, int64_t buildEpoch) const
    {
        const uint32_t epoch = epoch_.load();
        if (epoch == 0 || now < buildEpoch - 86400) return false;
        const int64_t expected = static_cast<int64_t>(epoch) + static_cast<uint32_t>(tick - tick_.load()) / 1000;
        return now >= expected - 5 && now <= expected + 5;
    }

private:
    std::atomic<uint32_t> tick_{0};
    std::atomic<uint32_t> epoch_{0};
};
