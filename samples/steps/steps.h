/**
 * steps.h — The Producer-C steps catalogue.
 *
 * Every Producer-C entry file follows the same 7-step outline:
 *
 *   Step 1: Device info        -> step_device_info.c
 *   Step 2: Stream info        -> step_stream_info.c
 *   Step 3: Auth               -> step_auth.c
 *   Step 4: Stream callbacks   -> step_stream_callbacks.c
 *   Step 5: Client + stream    -> inline in entry (one form, no variants)
 *   Step 6: Frame loop         -> step_frame_loop.c
 *   Step 7: Teardown           -> inline in entry (one form, no variants)
 *
 * Each step function is a single teaching concept. A new sample is a short
 * entry file in entry/ that swaps one step function for another; two entries
 * read side-by-side show exactly which step differs.
 */
#ifndef __KVS_SAMPLE_STEPS_INCLUDE__
#define __KVS_SAMPLE_STEPS_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/sample_common.h"

// ============================================================================
// Step 1: Device info (step_device_info.c)
//
// The DeviceInfo controls process-wide producer settings: the content-store
// size shared by all streams on this client, log level, and client tags.
// ============================================================================

/**
 * Default device info: 128 MB content store, log level from the
 * AWS_KVS_LOG_LEVEL environment variable (defaults to DEBUG).
 */
STATUS configureDefaultDeviceInfo(PDeviceInfo* ppDeviceInfo);

// ============================================================================
// Step 2: Stream info (step_stream_info.c)
//
// The StreamInfo commits the stream to a streaming type (realtime vs offline),
// codec(s), retention, and buffering behavior. Multi-track streams must have
// codec private data (CPD) set on every track before the first putFrame.
// ============================================================================

/**
 * Realtime, video-only, H.264, 2h retention, relative fragment times.
 */
STATUS configureRealtimeVideoStreamInfo(PCHAR streamName, PStreamInfo* ppStreamInfo);

/**
 * Realtime, multi-track (H.264 video + AAC audio), 2h retention, relative
 * fragment times. Generates and attaches the AAC codec private data so both
 * tracks are fully described before the stream is created.
 */
STATUS configureAudioVideoStreamInfo(PCHAR streamName, PStreamInfo* ppStreamInfo);

// ============================================================================
// Step 3: Auth (step_auth.c)
//
// Each configureXxxAuth() creates the client-callbacks provider wired with one
// credential mechanism, reading its inputs from environment variables. The
// AuthConfig owns the callbacks provider and (for custom providers) the
// credential provider; teardown is always a single freeAuth() call.
// ============================================================================

typedef struct {
    PClientCallbacks pClientCallbacks;         // Passed to createKinesisVideoClient at step 5
    PAwsCredentialProvider pCredentialProvider; // Non-NULL only for custom-provider mechanisms (e.g. STS)
} AuthConfig, *PAuthConfig;

/**
 * Static IAM keys from AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY
 * (+ optional AWS_SESSION_TOKEN).
 *
 * ONLY for credentials that never rotate. If your credentials rotate
 * (STS, instance profiles, your own vending service), use
 * configureStsAssumeRoleAuth() as the reference for the credential-provider
 * pattern instead — passing rotating credentials here freezes the first set
 * inside the SDK and streams start timing out when it expires.
 */
STATUS configureStaticAuth(PAuthConfig pAuth);

/**
 * Rotating STS credentials via AssumeRole. Reference implementation of the
 * custom AwsCredentialProvider pattern: the SDK calls back into the provider
 * before every API call and token rotation, and the provider refreshes the
 * credentials on demand ahead of expiry.
 *
 * Environment variables:
 *   AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY - base credentials that sign AssumeRole
 *   AWS_SESSION_TOKEN                          - (optional) base session token
 *   AWS_STS_ROLE_ARN                           - role to assume
 *   AWS_STS_SESSION_NAME                       - (optional) session name
 *   AWS_DEFAULT_REGION                         - (optional) defaults to us-west-2
 */
STATUS configureStsAssumeRoleAuth(PAuthConfig pAuth);

/**
 * Frees the callbacks provider and, if present, the custom credential
 * provider — in that order, since the auth callbacks hold a reference to the
 * provider.
 */
STATUS freeAuth(PAuthConfig pAuth);

// ============================================================================
// Step 4: Stream callbacks (step_stream_callbacks.c)
//
// Stream callbacks are how the application observes and reacts to stream
// events: errors, ACKs, backpressure, lifecycle changes.
// ============================================================================

/**
 * The SDK's default retry orchestrator. Handles stream errors, staleness and
 * latency pressure by resetting the stream/connection with backoff. Start
 * here; attach custom handlers only when you need application-specific
 * behavior on top.
 */
STATUS attachContinuousRetryCallbacks(PClientCallbacks pClientCallbacks);

// ============================================================================
// Step 6: Frame loop (step_frame_loop.c)
//
// The putFrame loop paces frames against the wall clock (realtime mode derives
// fragment timestamps from frame PTS, so pacing must match the declared frame
// durations).
// ============================================================================

/**
 * Video-only realtime loop: single thread, keyframe every
 * SAMPLE_KEY_FRAME_INTERVAL frames cuts the fragments.
 */
STATUS runRealtimeVideoFrameLoop(STREAM_HANDLE streamHandle, PFrameSource pFrameSource, UINT64 streamingDuration);

/**
 * Audio+video realtime loop: one thread per track. The audio thread holds off
 * until the first video frame is put — in multi-track streams the video track
 * cuts fragments, and frames on other tracks must not lead the first fragment
 * boundary.
 */
STATUS runAudioVideoFrameLoop(STREAM_HANDLE streamHandle, PFrameSource pFrameSource, UINT64 streamingDuration);

#ifdef __cplusplus
}
#endif
#endif /* __KVS_SAMPLE_STEPS_INCLUDE__ */
