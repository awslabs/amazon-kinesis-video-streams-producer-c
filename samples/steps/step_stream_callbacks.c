/**
 * step_stream_callbacks.c — Step 4: Stream callbacks.
 *
 * Stream callbacks are the application's window into the stream: errors,
 * fragment ACKs, backpressure and lifecycle events all arrive here. The
 * variants in this file grow with the catalogue:
 *
 *   attachContinuousRetryCallbacks - the SDK's default retry orchestrator
 *                                    (this file, start here)
 *   attachCustomErrorHandler       - application-side error classification
 *   attachAckTracking              - persistence tracking via fragment ACKs
 *   attachBackpressureHandlers     - storage / buffer-duration / latency
 *   attachLifecycleObserver        - ready / closed / stale / dropped
 *
 * Rules that apply to EVERY stream callback implementation:
 *   - Callbacks fire on SDK threads. Do not block, sleep, or do long-running
 *     work inside them; hand work off to your own thread.
 *   - Do not make reentrant SDK calls (e.g. freeKinesisVideoStream) from
 *     inside a callback for that stream.
 *   - Callbacks can fire repeatedly during a sustained failure; handlers must
 *     be idempotent.
 */
#include "steps.h"

STATUS attachContinuousRetryCallbacks(PClientCallbacks pClientCallbacks)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStreamCallbacks pStreamCallbacks = NULL;

    CHK(pClientCallbacks != NULL, STATUS_NULL_ARG);

    // The continuous-retry callbacks implement the SDK's recommended reaction
    // to stream errors, staleness, and latency pressure: reset the stream or
    // connection with backoff and keep going. They are chained into the
    // callbacks provider and freed with it — no separate teardown needed.
    //
    // Not attaching anything here is the single most common callback
    // integration gap: without a retry orchestrator (or your own handlers) a
    // stream error simply stops the stream and nothing restarts it.
    CHK_STATUS(createContinuousRetryStreamCallbacks(pClientCallbacks, &pStreamCallbacks));

CleanUp:
    return retStatus;
}
