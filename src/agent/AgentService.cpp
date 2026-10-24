#include "AgentService.h"

#include <Arduino.h>

#include "../config/Config.h"
#include "../config/ConfigManager.h"
#include "../util/ScopeExit.h"

AgentService::AgentService(
    FtpsClient &ftpsClient,
    HttpUploadClient &uploadClient,
    CameraClient &cameraClient,
    AgentEventPublisher &eventPublisher)
    : ftpsClient_(ftpsClient),
      uploadClient_(uploadClient),
      cameraClient_(cameraClient),
      eventPublisher_(eventPublisher)
{
}

void AgentService::enableSnapshotScheduler()
{
    snapshotSchedulerEnabled_ = true;

    Serial.println(
        "Snapshot scheduler enabled");
}

void AgentService::disableSnapshotScheduler()
{
    snapshotSchedulerEnabled_ = false;

    Serial.println(
        "Snapshot scheduler disabled");
}

bool AgentService::connectToPrinter()
{
    if (!ftpsClient_.connect())
    {
        ftpsClient_.disconnect();
        return false;
    }

    if (!ftpsClient_.login())
    {
        ftpsClient_.disconnect();
        return false;
    }

    if (!ftpsClient_.initializeProtection())
    {
        ftpsClient_.disconnect();
        return false;
    }

    if (!ftpsClient_.changeDirectory(
            ConfigManager::getFtpsDirectory().c_str()))
    {
        ftpsClient_.disconnect();
        return false;
    }

    return true;
}

bool AgentService::uploadModel(
    const String &fileName)
{
    if (currentOperation_ != AgentOperation::NONE)
    {
        eventPublisher_.publishModelUploadFailed(
            fileName,
            "OPERATION_IN_PROGRESS");

        Serial.println(
            "Another operation already in progress");

        return false;
    }

    currentOperation_ =
        AgentOperation::MODEL_UPLOAD;

    auto cleanup = [&]() {
        uploadClient_.abort();
        ftpsClient_.disconnect();
        currentOperation_ = AgentOperation::NONE;
    };
    ScopeExit<decltype(cleanup)> cleanupGuard(cleanup);

    eventPublisher_.publishModelUploadStarted(
        fileName);

    if (!connectToPrinter())
    {
        eventPublisher_.publishModelUploadFailed(
            fileName,
            "FTPS_CONNECT_FAILED");

        Serial.println(
            "Printer connection failed");

        return false;
    }

    size_t fileSize;

    if (!ftpsClient_.getFileSize(
            fileName.c_str(),
            fileSize))
    {
        eventPublisher_.publishModelUploadFailed(
            fileName,
            "SIZE_FAILED");

        Serial.println(
            "SIZE failed");

        return false;
    }

    if (!uploadClient_.begin(
            fileName.c_str(),
            fileSize))
    {
        eventPublisher_.publishModelUploadFailed(
            fileName,
            "UPLOAD_BEGIN_FAILED");

        Serial.println(
            "Upload begin failed");

        return false;
    }

    if (!ftpsClient_.downloadFile(
            fileName.c_str(),
            uploadClient_))
    {
        char reason[64];
        snprintf(reason, sizeof(reason), "DOWNLOAD_FAILED:%s", ftpsClient_.lastFailure());
        eventPublisher_.publishModelUploadFailed(
            fileName,
            reason);

        Serial.printf("Model transfer failed: stage=%s ftp_code=%d transferred=%u expected=%u\n",
                      ftpsClient_.lastFailure(), ftpsClient_.lastReplyCode(),
                      static_cast<unsigned>(ftpsClient_.transferredBytes()), static_cast<unsigned>(fileSize));

        return false;
    }

    bool uploadSuccess =
        uploadClient_.finish();

    if (!uploadSuccess)
    {
        eventPublisher_.publishModelUploadFailed(
            fileName,
            "HTTP_UPLOAD_FAILED");

        return false;
    }

    eventPublisher_.publishModelUploadCompleted(
        fileName);

    return true;
}

bool AgentService::uploadSnapshot()
{
    if (currentOperation_ ==
        AgentOperation::MODEL_UPLOAD)
    {
        Serial.println(
            "Snapshot skipped: model upload in progress");
        return false;
    }

    if (currentOperation_ != AgentOperation::NONE)
    {
        return false;
    }

    currentOperation_ =
        AgentOperation::SNAPSHOT_UPLOAD;
    auto cleanup = [&]() {
        uploadClient_.abort();
        cameraClient_.disconnect();
        currentOperation_ = AgentOperation::NONE;
    };
    ScopeExit<decltype(cleanup)> cleanupGuard(cleanup);

    lastSnapshotSuccess_ = false;
    if (!cameraClient_.connect())
    {
        return false;
    }

    if (!cameraClient_.authenticate())
    {
        return false;
    }

    size_t snapshotSize =
        cameraClient_.readSnapshotSize();

    if (snapshotSize == 0)
    {
        return false;
    }

    Serial.printf(
        "Snapshot received. Size: %u bytes\n",
        snapshotSize);

    if (!uploadClient_.beginSnapshotUpload(
            ConfigManager::getEdolAgentId().c_str(),
            snapshotSize))
    {
        return false;
    }

    bool uploadSuccess =
        cameraClient_.streamSnapshot(
            static_cast<uint32_t>(snapshotSize),
            [&](const uint8_t *data, size_t len)
            {
                return uploadClient_.write(
                           data,
                           len) == len;
            });

    uploadSuccess = uploadSuccess && uploadClient_.finish();

    lastSnapshotSuccess_ = uploadSuccess;

    return uploadSuccess;
}

void AgentService::processSnapshotScheduler()
{
    if (!snapshotSchedulerEnabled_)
    {
        return;
    }

    if (lastSnapshotMillis_ == 0)
    {
        lastSnapshotMillis_ = millis();
    }

    if (millis() - lastSnapshotMillis_ <
        Config::CAMERA_SNAPSHOT_INTERVAL_MS)
    {
        return;
    }

    lastSnapshotMillis_ = millis();

    uploadSnapshot();
}

bool AgentService::isCameraHealthy() const
{
    return lastSnapshotSuccess_;
}
