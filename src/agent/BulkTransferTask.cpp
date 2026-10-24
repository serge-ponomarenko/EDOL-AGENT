#include "BulkTransferTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool BulkTransferTask::begin()
{
    // ESP-IDF expresses stack depth in bytes. Never allocate another worker.
    if (ready_) return true;
    if (attempted_ && millis() - lastAttempt_ < 30000) return false;
    attempted_ = true;
    lastAttempt_ = millis();
    ready_ = xTaskCreate(run, "edol-bulk", 12288, this, tskIDLE_PRIORITY, nullptr) == pdPASS;
    return ready_;
}

bool BulkTransferTask::request(const char *model)
{
    if (!ready_) return false;
    if (model)
    {
        if (!TransferMailbox::validName(model)) return false;
        if (mailbox_.busy() && mailbox_.model() && std::strcmp(mailbox_.name(), model) == 0) return true;
        if (*pendingModel_) return std::strcmp(pendingModel_, model) == 0;
        if (mailbox_.submit(model, millis())) return true;
        std::memcpy(pendingModel_, model, std::strlen(model) + 1);
        return true;
    }
    const bool accepted = !*pendingModel_ && mailbox_.submit(nullptr, millis());
    if (!accepted) Serial.println("Snapshot deferred: busy, model pending or cooldown");
    return accepted;
}

void BulkTransferTask::collect()
{
    bool success;
    if (mailbox_.collect(millis(), success))
        Serial.printf("Bulk operation completed: %s\n", success ? "SUCCESS" : "FAILED");
    if (*pendingModel_ && mailbox_.submit(pendingModel_, millis())) *pendingModel_ = '\0';
}

void BulkTransferTask::run(void *context)
{
    auto &self = *static_cast<BulkTransferTask *>(context);
    for (;;)
    {
        if (self.mailbox_.take())
        {
            const bool success = self.mailbox_.model()
                ? self.service_.uploadModel(self.mailbox_.name()) : self.service_.uploadSnapshot();
            self.mailbox_.complete(success);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
