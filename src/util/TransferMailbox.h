#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

// One main-loop producer and one bulk worker. Callback memory is never retained.
class TransferMailbox
{
public:
    enum class State { IDLE, READY, RUNNING, COMPLETE };
    static bool validName(const char *name)
    {
        if (!name || !*name || std::strlen(name) >= 256) return false;
        for (const unsigned char *c = reinterpret_cast<const unsigned char *>(name); *c; ++c)
            if (*c < 32 || *c == 127) return false;
        return true;
    }
    bool submit(const char *name, uint32_t now)
    {
        if (state_.load() != State::IDLE || (cooling_ && now - completedAt_ < 15000)) return false;
        if (name && !validName(name)) return false;
        model_ = name != nullptr;
        if (name) std::memcpy(name_, name, std::strlen(name) + 1);
        state_.store(State::READY);
        return true;
    }
    bool take() { State expected = State::READY; return state_.compare_exchange_strong(expected, State::RUNNING); }
    bool busy() const { return state_.load() != State::IDLE; }
    bool model() const { return model_; }
    const char *name() const { return name_; }
    void complete(bool success) { success_ = success; state_.store(State::COMPLETE); }
    bool collect(uint32_t now, bool &success)
    {
        if (state_.load() != State::COMPLETE) return false;
        success = success_;
        completedAt_ = now;
        cooling_ = true;
        state_.store(State::IDLE);
        return true;
    }
private:
    std::atomic<State> state_{State::IDLE};
    char name_[256] = {};
    bool model_ = false, success_ = false, cooling_ = false;
    uint32_t completedAt_ = 0;
};
