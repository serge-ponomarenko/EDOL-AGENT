#include <cassert>
#include <iostream>
#include <thread>
#include "util/TransferMailbox.h"
#include "util/BulkAdmission.h"

FakeSerial Serial;
uint32_t fakeMillis = 0;

int main()
{
    TransferMailbox mailbox;
    char callback[] = "model.3mf";
    assert(mailbox.submit(callback, 0));
    callback[0] = 'X';
    assert(std::strcmp(mailbox.name(), "model.3mf") == 0);
    assert(!mailbox.submit(nullptr, 0));
    std::atomic<bool> release{false}, running{false};
    std::thread worker([&]() {
        assert(mailbox.take());
        assert(!mailbox.take());
        running.store(true);
        while (!release.load()) std::this_thread::yield();
        mailbox.complete(false);
    });
    unsigned printerLoops = 0, edolLoops = 0;
    while (!running.load()) std::this_thread::yield();
    for (unsigned i = 0; i < 1000; ++i) {
        ++printerLoops; ++edolLoops;
        assert(!mailbox.submit(nullptr, i));
    }
    release.store(true);
    worker.join();
    bool result = true;
    assert(mailbox.collect(1000, result) && !result);
    assert(printerLoops == 1000 && edolLoops == 1000);
    assert(!mailbox.submit(nullptr, 15999));
    assert(mailbox.submit(nullptr, 16000));
    assert(mailbox.take() && !mailbox.model());
    mailbox.complete(true);
    assert(mailbox.collect(17000, result) && result);
    char oversized[257]; std::memset(oversized, 'a', 256); oversized[256] = 0;
    assert(!mailbox.submit(oversized, 32000));
    assert(!mailbox.submit("", 32000));
    assert(bulkRecordBudget(32768, 16384, 16384));
    assert(!bulkRecordBudget(32767, 16384, 16384));
    assert(!bulkRecordBudget(100000, 16383, 16384));
    // Record admission does not assert that transient signature allocations fit.
    assert(mailbox.submit("retry.3mf", 32000));
    std::cout << "PASS: bounded mailbox, ownership, servicing opportunity, cooldown and fragmented-heap admission\n";
}
