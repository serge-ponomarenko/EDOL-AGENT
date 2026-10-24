#pragma once

#include "AgentService.h"
#include "../util/TransferMailbox.h"

// All camera/FTPS/HTTP calls belong to this task; MQTT belongs to loop().
class BulkTransferTask
{
public:
    explicit BulkTransferTask(AgentService &service) : service_(service) {}
    bool begin();
    bool request(const char *model = nullptr);
    void collect();
    bool busy() const { return mailbox_.busy() || *pendingModel_; }
private:
    static void run(void *context);
    AgentService &service_;
    TransferMailbox mailbox_;
    char pendingModel_[256] = {}; // Main-loop owned; one coalesced model outranks snapshots.
    bool ready_ = false, attempted_ = false;
    uint32_t lastAttempt_ = 0;
};
