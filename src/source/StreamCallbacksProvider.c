/**
 * Kinesis Video Producer StreamCallbacks Provider
 */
#define LOG_CLASS "StreamCallbacksProvider"
#include "Include_i.h"

STATUS defaultStreamUnderflowCallback(UINT64 customData, STREAM_HANDLE streamHandle)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported streamUnderflow callback for stream handle %" PRIu64, streamHandle);
    return STATUS_SUCCESS;
}

STATUS defaultBufferDurationOverflowCallback(UINT64 customData, STREAM_HANDLE streamHandle, UINT64 remainDuration)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported bufferDurationOverflow callback for stream handle %" PRIu64 ". Remaining duration in 100ns: %" PRIu64, streamHandle,
          remainDuration);
    return STATUS_SUCCESS;
}

STATUS defaultStreamLatencyPressureCallback(UINT64 customData, STREAM_HANDLE streamHandle, UINT64 currentBufferDuration)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported streamLatencyPressure callback for stream handle %" PRIu64 ". Current buffer duration in 100ns: %" PRIu64, streamHandle,
          currentBufferDuration);
    return STATUS_SUCCESS;
}

STATUS defaultStreamConnectionStaleCallback(UINT64 customData, STREAM_HANDLE streamHandle, UINT64 timeSinceLastBufferingAck)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported streamConnectionStale callback for stream handle %" PRIu64 ". Time since last buffering ack in 100ns: %" PRIu64, streamHandle,
          timeSinceLastBufferingAck);
    return STATUS_SUCCESS;
}

STATUS defaultDroppedFrameReportCallback(UINT64 customData, STREAM_HANDLE streamHandle, UINT64 frameTimecode)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported droppedFrame callback for stream handle %" PRIu64 ". Dropped frame timecode in 100ns: %" PRIu64, streamHandle, frameTimecode);
    return STATUS_SUCCESS;
}

STATUS defaultDroppedFragmentReportCallback(UINT64 customData, STREAM_HANDLE streamHandle, UINT64 fragmentTimeCode)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported droppedFragment callback for stream handle %" PRIu64 ". Dropped fragment timecode in 100ns: %" PRIu64, streamHandle,
          fragmentTimeCode);
    return STATUS_SUCCESS;
}

STATUS defaultStreamErrorReportCallback(UINT64 customData, STREAM_HANDLE streamHandle, UPLOAD_HANDLE uploadHandle, UINT64 fragmentTimecode,
                                        STATUS errorStatus)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported streamError callback for stream handle %" PRIu64 ". Upload handle %" PRIu64 ". Fragment timecode in"
          " 100ns: %" PRIu64 ". Error status: 0x%08x",
          streamHandle, uploadHandle, fragmentTimecode, errorStatus);
    return STATUS_SUCCESS;
}

PCHAR getFragmentAckTypeStr(FRAGMENT_ACK_TYPE ackType)
{
    switch (ackType) {
        case FRAGMENT_ACK_TYPE_BUFFERING:
            return (PCHAR) "BUFFERING";
        case FRAGMENT_ACK_TYPE_RECEIVED:
            return (PCHAR) "RECEIVED";
        case FRAGMENT_ACK_TYPE_PERSISTED:
            return (PCHAR) "PERSISTED";
        case FRAGMENT_ACK_TYPE_ERROR:
            return (PCHAR) "ERROR";
        case FRAGMENT_ACK_TYPE_IDLE:
            return (PCHAR) "IDLE";
        case FRAGMENT_ACK_TYPE_UNDEFINED:
        default:
            return (PCHAR) "UNDEFINED";
    }
}

STATUS defaultFragmentAckReceivedCallback(UINT64 customData, STREAM_HANDLE streamHandle, UPLOAD_HANDLE uploadHandle, PFragmentAck pFragmentAck)
{
    UNUSED_PARAM(customData);

    if (pFragmentAck == NULL) {
        return STATUS_SUCCESS;
    }

    // RECEIVED/PERSISTED/ERROR ACKs are logged at INFO so they can be collected without turning on
    // verbose logging. BUFFERING and IDLE ACKs stay at DEBUG since they are the high volume ones.
    switch (pFragmentAck->ackType) {
        case FRAGMENT_ACK_TYPE_RECEIVED:
        case FRAGMENT_ACK_TYPE_PERSISTED:
            DLOGI("Reported fragmentAckReceived callback for stream handle %" PRIu64 ". Upload handle %" PRIu64 ". Ack type: %s."
                  " Fragment timecode in 100ns: %" PRIu64 ". Sequence number: %s",
                  streamHandle, uploadHandle, getFragmentAckTypeStr(pFragmentAck->ackType), pFragmentAck->timestamp,
                  pFragmentAck->sequenceNumber);
            break;

        case FRAGMENT_ACK_TYPE_ERROR:
            DLOGW("Reported fragmentAckReceived callback for stream handle %" PRIu64 ". Upload handle %" PRIu64 ". Ack type: ERROR."
                  " Fragment timecode in 100ns: %" PRIu64 ". Sequence number: %s. Error id: %u",
                  streamHandle, uploadHandle, pFragmentAck->timestamp, pFragmentAck->sequenceNumber, pFragmentAck->result);
            break;

        default:
            DLOGD("Reported fragmentAckReceived callback for stream handle %" PRIu64 ". Upload handle %" PRIu64 ". Ack type: %s."
                  " Fragment timecode in 100ns: %" PRIu64 ". Sequence number: %s",
                  streamHandle, uploadHandle, getFragmentAckTypeStr(pFragmentAck->ackType), pFragmentAck->timestamp,
                  pFragmentAck->sequenceNumber);
            break;
    }

    return STATUS_SUCCESS;
}

STATUS defaultStreamDataAvailableCallback(UINT64 customData, STREAM_HANDLE streamHandle, PCHAR streamName, UPLOAD_HANDLE uploadHandle,
                                          UINT64 durationAvailable, UINT64 bytesAvailable)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(streamName);
    DLOGV("Reported streamDataAvailable callback for stream handle %" PRIu64 ". Upload handle %" PRIu64 ". Duration available in"
          " 100ns: %" PRIu64 ". Bytes available: %" PRIu64,
          streamHandle, uploadHandle, durationAvailable, bytesAvailable);

    return STATUS_SUCCESS;
}

STATUS defaultStreamReadyCallback(UINT64 customData, STREAM_HANDLE streamHandle)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported streamReady callback for stream handle %" PRIu64, streamHandle);

    return STATUS_SUCCESS;
}

STATUS defaultStreamClosedCallback(UINT64 customData, STREAM_HANDLE streamHandle, UPLOAD_HANDLE uploadHandle)
{
    UNUSED_PARAM(customData);
    DLOGD("Reported streamClosed callback for stream handle %" PRIu64 ". Upload handle %" PRIu64, streamHandle, uploadHandle);

    return STATUS_SUCCESS;
}

STATUS defaultStreamShutdownCallback(UINT64 customData, STREAM_HANDLE streamHandle, BOOL resetStream)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(resetStream);
    DLOGD("Reported streamShutdown callback for stream handle %" PRIu64, streamHandle);

    return STATUS_SUCCESS;
}

STATUS defaultFreeStreamCallbacksFn(PUINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(customData != NULL, STATUS_NULL_ARG);
    PStreamCallbacks pStreamCallbacks = (PStreamCallbacks) *customData;
    CHK_STATUS(freeStreamCallbacks(&pStreamCallbacks));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS createStreamCallbacks(PStreamCallbacks* ppStreamCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStreamCallbacks pStreamCallbacks = NULL;

    CHK(ppStreamCallbacks != NULL, STATUS_NULL_ARG);

    // allocate the entire struct
    pStreamCallbacks = MEMCALLOC(1, SIZEOF(StreamCallbacks));
    CHK(pStreamCallbacks != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pStreamCallbacks->version = STREAM_CALLBACKS_CURRENT_VERSION;
    pStreamCallbacks->customData = (UINT64) pStreamCallbacks;

    pStreamCallbacks->streamUnderflowReportFn = defaultStreamUnderflowCallback;
    pStreamCallbacks->bufferDurationOverflowPressureFn = defaultBufferDurationOverflowCallback;
    pStreamCallbacks->streamLatencyPressureFn = defaultStreamLatencyPressureCallback;
    pStreamCallbacks->streamConnectionStaleFn = defaultStreamConnectionStaleCallback;
    pStreamCallbacks->droppedFrameReportFn = defaultDroppedFrameReportCallback;
    pStreamCallbacks->droppedFragmentReportFn = defaultDroppedFragmentReportCallback;
    pStreamCallbacks->streamErrorReportFn = defaultStreamErrorReportCallback;
    pStreamCallbacks->fragmentAckReceivedFn = defaultFragmentAckReceivedCallback;
    pStreamCallbacks->streamDataAvailableFn = defaultStreamDataAvailableCallback;
    pStreamCallbacks->streamReadyFn = defaultStreamReadyCallback;
    pStreamCallbacks->streamClosedFn = defaultStreamClosedCallback;
    pStreamCallbacks->streamShutdownFn = defaultStreamShutdownCallback;
    pStreamCallbacks->freeStreamCallbacksFn = defaultFreeStreamCallbacksFn;

CleanUp:

    if (!STATUS_SUCCEEDED(retStatus)) {
        freeStreamCallbacks(&pStreamCallbacks);
        pStreamCallbacks = NULL;
    }

    if (pStreamCallbacks != NULL) {
        *ppStreamCallbacks = pStreamCallbacks;
    }

    LEAVES();
    return retStatus;
}

STATUS freeStreamCallbacks(PStreamCallbacks* ppStreamCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppStreamCallbacks != NULL, STATUS_NULL_ARG);
    CHK(*ppStreamCallbacks != NULL, retStatus);

    MEMFREE(*ppStreamCallbacks);
    *ppStreamCallbacks = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}