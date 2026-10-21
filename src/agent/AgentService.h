#pragma once

#include "../ftps/FtpsClient.h"
#include "../http/HttpUploadClient.h"
#include "../camera/CameraClient.h"
#include "../mqtt/AgentEventPublisher.h"

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
        AgentEventPublisher &eventPublisher);

    bool uploadModel(
        const String &fileName);

    bool uploadSnapshot();

    void enableSnapshotScheduler();
    void disableSnapshotScheduler();

    void processSnapshotScheduler();

    bool isCameraHealthy() const;

private:
    bool connectToPrinter();

    FtpsClient &ftpsClient_;
    HttpUploadClient &uploadClient_;
    CameraClient &cameraClient_;
    AgentEventPublisher &eventPublisher_;

    uint32_t lastSnapshotMillis_ = 0;
    bool lastSnapshotSuccess_ = false;
    bool snapshotSchedulerEnabled_ = false;

    AgentOperation currentOperation_ = AgentOperation::NONE;
};