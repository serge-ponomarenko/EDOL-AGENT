#pragma once

#include <utility>

// Run cleanup on every exit path. The callback must not throw.
template <typename Callback>
class ScopeExit
{
public:
    explicit ScopeExit(Callback callback) : callback_(std::move(callback)) {}
    ~ScopeExit() { callback_(); }

    ScopeExit(const ScopeExit &) = delete;
    ScopeExit &operator=(const ScopeExit &) = delete;

private:
    Callback callback_;
};
