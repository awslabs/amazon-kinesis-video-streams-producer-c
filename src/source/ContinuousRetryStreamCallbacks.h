#ifndef __KINESIS_VIDEO_CONTINUOUS_RETRY_INCLUDE_I__
#define __KINESIS_VIDEO_CONTINUOUS_RETRY_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct __CallbackStateMachine;
struct __CallbacksProvider;

/**
 * @brief Function returning AWS credentials
 */
typedef STATUS (*ExponentialWaitFn)(PVOID);

////////////////////////////////////////////////////////////////////////
// Struct definition
////////////////////////////////////////////////////////////////////////
typedef struct __ContinuousRetryStreamCallbacks ContinuousRetryStreamCallbacks;
struct __ContinuousRetryStreamCallbacks {
    // First member should be the stream callbacks
    StreamCallbacks streamCallbacks;

    // Back pointer to the callback provider object
    struct __CallbacksProvider* pCallbacksProvider;

    // Lock for synchronizing the mapping table and accessing variables
    MUTEX syncLock;

    // Streams state machine table stream handle -> callback state machine
    PHashTable pStreamMapping;

    ExponentialWaitFn continuousRetryExponentialWaitFn;
};
typedef struct __ContinuousRetryStreamCallbacks* PContinuousRetryStreamCallbacks;

struct __StreamLatencyStateMachine;
struct __ConnectionStaleStateMachine;

typedef struct __CallbackStateMachine {
    // Indicator of stream being ready
    volatile BOOL streamReady;

    // Reset thread id to keep track
    volatile TID resetTid;

    // Placeholder for the current stream handle
    volatile STREAM_HANDLE streamHandle;

    // Placeholder for the current upload handle
    UPLOAD_HANDLE uploadHandle;

    // Placeholder for the timecode
    UINT64 erroredTimecode;

    // Exponential backoff State
    PExponentialBackoffState pExponentialBackoffState;

    // Latency state machine
    struct __StreamLatencyStateMachine streamLatencyStateMachine;

    // Staleness state machine
    struct __ConnectionStaleStateMachine connectionStaleStateMachine;

    // Back pointer to the parent object
    PContinuousRetryStreamCallbacks pContinuousRetryStreamCallbacks;
} CallbackStateMachine, *PCallbackStateMachine;

////////////////////////////////////////////////////////////////////////
// Auxiliary functionality
////////////////////////////////////////////////////////////////////////
STATUS removeMappingEntryCallback(UINT64, PHashEntry);
STATUS freeStreamMapping(PContinuousRetryStreamCallbacks, STREAM_HANDLE, BOOL);
STATUS getStreamMapping(PContinuousRetryStreamCallbacks, STREAM_HANDLE, PCallbackStateMachine*);
PVOID continuousRetryStreamRestartHandler(PVOID);

////////////////////////////////////////////////////////////////////////
// Callback function implementations
////////////////////////////////////////////////////////////////////////
STATUS continuousRetryStreamConnectionStaleHandler(UINT64, STREAM_HANDLE, UINT64);
STATUS continuousRetryStreamErrorReportHandler(UINT64, STREAM_HANDLE, UPLOAD_HANDLE, UINT64, STATUS);
STATUS continuousRetryStreamLatencyPressureHandler(UINT64, STREAM_HANDLE, UINT64);
STATUS continuousRetryStreamReadyHandler(UINT64, STREAM_HANDLE);
STATUS continuousRetryStreamFreeHandler(PUINT64);
STATUS continuousRetryStreamShutdownHandler(UINT64, STREAM_HANDLE, BOOL);
STATUS continuousRetryStreamClosedHandler(UINT64, STREAM_HANDLE, UPLOAD_HANDLE);
STATUS continuousRetryExponentialWaitHandler(PVOID);

#ifdef __cplusplus
}
#endif

#endif //__KINESIS_VIDEO_CONTINUOUS_RETRY_INCLUDE_I__
