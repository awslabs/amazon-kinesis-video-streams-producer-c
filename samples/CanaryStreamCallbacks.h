#ifndef __KINESIS_VIDEO_CONTINUOUS_RETRY_INCLUDE_I__
#define __KINESIS_VIDEO_CONTINUOUS_RETRY_INCLUDE_I__

#pragma once

#include <com/amazonaws/kinesis/video/cproducer/Include.h>
#include <aws/core/Aws.h>
#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, include_i, 1) // for byte alignment

struct __CallbackStateMachine;
struct __CallbacksProvider;

////////////////////////////////////////////////////////////////////////
// Struct definition
////////////////////////////////////////////////////////////////////////

typedef struct CanaryMetricsDuration CanaryMetricsDuration;

struct CanaryMetricsDuration {
    UINT64 currentViewDurationFrequency;
    UINT64 overallViewDurationFrequency;
    UINT64 currentViewSizeFrequency;
    UINT64 overallViewSizeFrequency;
    UINT64 currentFrameRateFrequency;
    UINT64 currentTransferRateFrequency;
    UINT64 uptimeFrequency;
    UINT64 transferredBytesFrequency;
    UINT64 totalSessionsFrequency;
    UINT64 totalActiveSessionsFrequency;
    UINT64 avgSessionDurationFrequency;
    UINT64 bufferedAcksFrequency;
    UINT64 receivedAcksFrequency;
    UINT64 persistedAcksFrequency;
    UINT64 errorAcksFrequency;
    UINT64 droppedFramesFrequency;
    UINT64 droppedFragmentsFrequency;
    UINT64 skippedFramesFrequency;
    UINT64 storagePressuresFrequency;
    UINT64 latencyPressuresFrequency;
    UINT64 bufferPressuresFrequency;
    UINT64 staleEventsFrequency;
    UINT64 putFrameErrorsFrequency;
    UINT64 cplApiCallLatencyFrequency;
    UINT64 dataApiCallLatencyFrequency;
};
typedef struct __CanaryMetricsDuration* PCanaryMetricsDuration;

typedef struct __CanaryStreamCallbacks CanaryStreamCallbacks;
struct __CanaryStreamCallbacks {
    // First member should be the stream callbacks
    StreamCallbacks streamCallbacks;
    PCHAR pStreamName;
    CanaryMetricsDuration canaryMetricsDuration;
    Aws::CloudWatch::CloudWatchClient* pCwClient;
    Aws::CloudWatch::Model::MetricDatum receivedAckDatum;
    Aws::CloudWatch::Model::MetricDatum persistedAckDatum;
    Aws::CloudWatch::Model::MetricDatum streamErrorDatum;
    Aws::CloudWatch::Model::MetricDatum currentFrameRateDatum;
    Aws::CloudWatch::Model::MetricDatum currentViewDurationDatum;
    Aws::CloudWatch::Model::MetricDatum contentStoreAvailableSizeDatum;
    std::map<UINT64, UINT64>* timeOfNextKeyFrame;
};
typedef struct __CanaryStreamCallbacks* PCanaryStreamCallbacks;


////////////////////////////////////////////////////////////////////////
// Auxiliary functionality
////////////////////////////////////////////////////////////////////////
STATUS removeMappingEntryCallback(UINT64, PHashEntry);
PVOID CanaryStreamRestartHandler(PVOID);

////////////////////////////////////////////////////////////////////////
// Callback function implementations
////////////////////////////////////////////////////////////////////////
STATUS createCanaryStreamCallbacks(Aws::CloudWatch::CloudWatchClient*, PCHAR, PStreamCallbacks*, PCanaryStreamCallbacks);
STATUS freeCanaryStreamCallbacks(PStreamCallbacks*);
STATUS CanaryStreamFragmentAckHandler(UINT64, STREAM_HANDLE, UPLOAD_HANDLE, PFragmentAck);
STATUS CanaryStreamConnectionStaleHandler(UINT64, STREAM_HANDLE, UINT64);
STATUS CanaryStreamErrorReportHandler(UINT64, STREAM_HANDLE, UPLOAD_HANDLE, UINT64, STATUS);
STATUS CanaryStreamLatencyPressureHandler(UINT64, STREAM_HANDLE, UINT64);
STATUS CanaryStreamReadyHandler(UINT64, STREAM_HANDLE);
STATUS CanaryStreamFreeHandler(PUINT64);
VOID CanaryStreamSendMetrics(PCanaryStreamCallbacks, Aws::CloudWatch::Model::MetricDatum&);
VOID CanaryStreamRecordFragmentEndSendTime(PStreamCallbacks, UINT64, UINT64);
VOID ComputeStreamMetricsFromCanary(STREAM_HANDLE, PCanaryStreamCallbacks);
VOID ComputeClientMetricsFromCanary(CLIENT_HANDLE, PCanaryStreamCallbacks);

#pragma pack(pop, include_i)

#ifdef  __cplusplus
}
#endif

#endif //__KINESIS_VIDEO_CONTINUOUS_RETRY_INCLUDE_I__
