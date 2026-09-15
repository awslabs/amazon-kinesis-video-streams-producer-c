/**
 * kvs_audio_video_sts_sample.c — Audio+video streaming with rotating STS
 * credentials.
 *
 * Variant of kvs_baseline.c: same 7-step outline, three steps swapped.
 *   Step 2: configureAudioVideoStreamInfo  (was configureRealtimeVideoStreamInfo)
 *   Step 3: configureStsAssumeRoleAuth     (was configureStaticAuth)
 *   Step 6: runAudioVideoFrameLoop         (was runRealtimeVideoFrameLoop)
 *
 * What this entry demonstrates:
 *   - Rotating STS credentials via a custom AwsCredentialProvider — the SDK
 *     pulls fresh credentials before each API call and token rotation, so
 *     long-running sessions survive credential expiry. See steps/step_auth.c.
 *   - Multi-track (H.264 + AAC) streaming with the CPD attached before stream
 *     creation and the audio track held back until the first video frame.
 *
 * Environment variables:
 *   AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY - base credentials that sign AssumeRole
 *   AWS_SESSION_TOKEN                          - (optional) base session token
 *   AWS_STS_ROLE_ARN                           - role to assume (required)
 *   AWS_STS_SESSION_NAME                       - (optional) session name
 *   AWS_DEFAULT_REGION                         - (optional) defaults to us-west-2
 *
 * Usage:
 *   ./kvsAudioVideoStsSample <stream_name> [duration_in_seconds] [frame_files_dir]
 */
#include "../steps/steps.h"

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    AuthConfig auth;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    FrameSource frameSource;
    PCHAR streamName, sampleDir;
    UINT64 streamingDuration = SAMPLE_STREAM_DURATION;

    MEMSET(&auth, 0x00, SIZEOF(AuthConfig));
    MEMSET(&frameSource, 0x00, SIZEOF(FrameSource));

    CHK_ERR(argc >= 2, STATUS_INVALID_ARG,
            "Usage: %s <stream_name> [duration_in_seconds] [frame_files_dir]\n"
            "  Required env vars: AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_STS_ROLE_ARN\n"
            "  Optional env vars: AWS_SESSION_TOKEN, AWS_STS_SESSION_NAME, AWS_DEFAULT_REGION",
            argv[0]);
    streamName = argv[1];
    if (argc >= 3) {
        CHK_STATUS(STRTOUI64(argv[2], NULL, 10, &streamingDuration));
        streamingDuration *= HUNDREDS_OF_NANOS_IN_A_SECOND;
    }
    sampleDir = (argc >= 4) ? argv[3] : (PCHAR) "../samples";

    // Media source stand-in: pre-encoded H.264 + AAC frames from disk.
    CHK_STATUS(loadAudioVideoFrames(sampleDir, &frameSource));

    // Step 1: device info. Content store size, log level.
    LOG_STEP("1: Configuring device info");
    CHK_STATUS(configureDefaultDeviceInfo(&pDeviceInfo));

    // Step 2: stream info. Realtime multi-track H.264 + AAC; audio CPD is
    // generated and attached inside the step, before stream creation.
    LOG_STEP("2: Configuring stream info (realtime, H.264 + AAC multi-track)");
    CHK_STATUS(configureAudioVideoStreamInfo(streamName, &pStreamInfo));

    // Step 3: auth. Rotating STS credentials through a custom credential
    // provider. The provider refreshes ahead of expiry inside
    // getCredentialsFn — never pass rotating credentials to the static path.
    LOG_STEP("3: Configuring auth (STS AssumeRole rotating credentials)");
    CHK_STATUS(configureStsAssumeRoleAuth(&auth));

    // Step 4: stream callbacks. Default continuous-retry orchestrator.
    LOG_STEP("4: Attaching stream callbacks (continuous retry)");
    CHK_STATUS(attachContinuousRetryCallbacks(auth.pClientCallbacks));

    // Step 5: create client and stream.
    LOG_STEP("5: Creating client and stream");
    CHK_STATUS(createKinesisVideoClient(pDeviceInfo, auth.pClientCallbacks, &clientHandle));
    CHK_STATUS(createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    // Step 6: frame loop. One thread per track; audio waits for the first
    // video frame (the video track cuts the fragments).
    LOG_STEP("6: Running audio+video frame loop");
    CHK_STATUS(runAudioVideoFrameLoop(streamHandle, &frameSource, streamingDuration));

    // Step 7: teardown. The sync stop flushes the final fragment before the
    // handles are freed — skipping it loses the tail of the stream.
    LOG_STEP("7: Stopping stream and freeing resources");
    CHK_STATUS(stopKinesisVideoStreamSync(streamHandle));

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        printf("Failed with status 0x%08x\n", retStatus);
    }

    freeKinesisVideoStream(&streamHandle);
    freeKinesisVideoClient(&clientHandle);
    freeAuth(&auth); // Frees the callbacks provider, then the STS credential provider
    freeStreamInfoProvider(&pStreamInfo);
    freeDeviceInfo(&pDeviceInfo);
    freeFrameSource(&frameSource);

    return (INT32) retStatus;
}
