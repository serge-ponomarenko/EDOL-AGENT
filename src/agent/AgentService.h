#pragma once

#include <atomic>
#include <functional>
#include "../ftps/FtpsClient.h"
#include "../http/HttpUploadClient.h"
#include "../camera/CameraClient.h"
#include "../mqtt/AgentEventPublisher.h"
#include "../mqtt/BambuMqttClient.h"

enum class ModelUploadFailureReason
{
    NONE,
    OPERATION_IN_PROGRESS,
    FTPS_CONNECT_FAILED,
    SIZE_FAILED,
    UPLOAD_BEGIN_FAILED,
    DOWNLOAD_FAILED,
    HTTP_UPLOAD_FAILED
};

enum class AgentOperation
{
    NONE,
    MODEL_UPLOAD,
    SNAPSHOT_UPLOAD
};

class AgentService
{
public:
    AgentService(
        FtpsClient &ftpsClient,
        HttpUploadClient &uploadClient,
        CameraClient &cameraClient,
        AgentEventPublisher &eventPublisher,
        BambuMqttClient &printerTelemetry);

    bool uploadModel(
        const String &fileName);

    bool uploadSnapshot();

    void enableSnapshotScheduler();
    void disableSnapshotScheduler();

    void processSnapshotScheduler();

    bool isCameraHealthy() const;
    void setSnapshotRequest(std::function<void()> request) { snapshotRequest_ = request; }

private:
    bool connectToPrinter();

    FtpsClient &ftpsClient_;
    HttpUploadClient &uploadClient_;
    CameraClient &cameraClient_;
    AgentEventPublisher &eventPublisher_;
    BambuMqttClient &printerTelemetry_;
    std::function<void()> snapshotRequest_;

    uint32_t lastSnapshotMillis_ = 0;
    std::atomic<bool> lastSnapshotSuccess_{false};
    bool snapshotSchedulerEnabled_ = false;

    AgentOperation currentOperation_ = AgentOperation::NONE;
};
