/**
 * kvs_baseline.c — Producer-C canonical reference.
 *
 * Read alongside https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/producersdk-c-write.html.
 * All teaching concepts live in steps/; this file only calls them in order.
 * Swap one step function to change one aspect at a time — for example, the
 * STS variant (kvs_audio_video_sts_sample.c) swaps steps 2, 3 and 6.
 *
 * This baseline commits to: realtime, video-only H.264, static IAM keys,
 * default continuous-retry callbacks.
 *
 * Usage:
 *   export AWS_ACCESS_KEY_ID=<key> AWS_SECRET_ACCESS_KEY=<secret>
 *   ./kvsBaselineSample <stream_name> [duration_in_seconds] [frame_files_dir]
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

    CHK_ERR(argc >= 2, STATUS_INVALID_ARG, "Usage: %s <stream_name> [duration_in_seconds] [frame_files_dir]", argv[0]);
    streamName = argv[1];
    if (argc >= 3) {
        CHK_STATUS(STRTOUI64(argv[2], NULL, 10, &streamingDuration));
        streamingDuration *= HUNDREDS_OF_NANOS_IN_A_SECOND;
    }
    sampleDir = (argc >= 4) ? argv[3] : (PCHAR) "../samples";

    // Media source stand-in: pre-encoded frames from disk. In a real
    // application this is your capture/encode pipeline.
    CHK_STATUS(loadVideoFrames(sampleDir, &frameSource));

    // Step 1: device info. Content store size, log level.
    LOG_STEP("1: Configuring device info");
    CHK_STATUS(configureDefaultDeviceInfo(&pDeviceInfo));

    // Step 2: stream info. Realtime, H.264 video-only, 2h retention.
    LOG_STEP("2: Configuring stream info (realtime, H.264 video-only)");
    CHK_STATUS(configureRealtimeVideoStreamInfo(streamName, &pStreamInfo));

    // Step 3: auth. Static keys from the environment.
    // Swap for configureStsAssumeRoleAuth() for rotating credentials.
    LOG_STEP("3: Configuring auth (static IAM keys)");
    CHK_STATUS(configureStaticAuth(&auth));

    // Step 4: stream callbacks. Default continuous-retry orchestrator.
    LOG_STEP("4: Attaching stream callbacks (continuous retry)");
    CHK_STATUS(attachContinuousRetryCallbacks(auth.pClientCallbacks));

    // Step 5: create client and stream.
    LOG_STEP("5: Creating client and stream");
    CHK_STATUS(createKinesisVideoClient(pDeviceInfo, auth.pClientCallbacks, &clientHandle));
    CHK_STATUS(createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    // Step 6: frame loop. Paced realtime putFrame until the duration elapses.
    LOG_STEP("6: Running realtime video frame loop");
    CHK_STATUS(runRealtimeVideoFrameLoop(streamHandle, &frameSource, streamingDuration));

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
    freeAuth(&auth);
    freeStreamInfoProvider(&pStreamInfo);
    freeDeviceInfo(&pDeviceInfo);
    freeFrameSource(&frameSource);

    return (INT32) retStatus;
}
