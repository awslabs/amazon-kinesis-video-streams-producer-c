#include "Samples.h"

#define DEFAULT_RETENTION_PERIOD          2 * HUNDREDS_OF_NANOS_IN_AN_HOUR
#define DEFAULT_BUFFER_DURATION           120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_CALLBACK_CHAIN_COUNT      5
// Every frame is flagged as a key frame, so at DEFAULT_FPS_VALUE = 25 (40ms frame duration) this
// produces one fragment every 40ms => 25 fragments/second, well above the 5 fragments/second
// service limit. Used to reproduce service-side rate limiting. Override with the
// KEY_FRAME_INTERVAL env var to change the fragment rate without changing the byte rate - e.g. 45
// restores ~1.8s fragments (0.5 fragments/second) as a control.
#define DEFAULT_KEY_FRAME_INTERVAL        1
#define KEY_FRAME_INTERVAL_ENV_VAR        ((PCHAR) "KEY_FRAME_INTERVAL")
#define DEFAULT_FPS_VALUE                 25
#define DEFAULT_STREAM_DURATION           20 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_STORAGE_SIZE              20 * 1024 * 1024
#define RECORDED_FRAME_AVG_BITRATE_BIT_PS 3800000
#define VIDEO_CODEC_NAME_H264             "h264"
#define VIDEO_CODEC_NAME_H265             "h265"
#define VIDEO_CODEC_NAME_MAX_LENGTH       5
#define METADATA_MAX_KEY_LENGTH           128
#define METADATA_MAX_VALUE_LENGTH         256
#define MAX_METADATA_PER_FRAGMENT         10

#define NUMBER_OF_FRAME_FILES 403

// #define IOT_CORE_ENABLE_CREDENTIALS 1

STATUS readFrameData(PFrame pFrame, PCHAR frameFilePath, PCHAR videoCodec)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 index;
    UINT64 size;

    CHK(pFrame != NULL, STATUS_NULL_ARG);

    index = pFrame->index % NUMBER_OF_FRAME_FILES + 1;
    SNPRINTF(filePath, MAX_PATH_LEN, "%s/%sSampleFrames/frame-%03d.%s", frameFilePath, videoCodec, index, videoCodec);
    size = pFrame->size;

    // Get the size and read into frame
    CHK_STATUS(readFile(filePath, TRUE, NULL, &size));
    CHK_STATUS(readFile(filePath, TRUE, pFrame->frameData, &size));

    pFrame->size = (UINT32) size;

    if (pFrame->flags == FRAME_FLAG_KEY_FRAME) {
        DLOGD("Key frame file %s, size %" PRIu64, filePath, pFrame->size);
    }

CleanUp:

    return retStatus;
}

// Forward declaration of the default thread sleep function
VOID defaultThreadSleep(UINT64);

#define STATS_REPORT_INTERVAL (1 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Diagnostic counters used to separate "the device cannot keep up" from "the content is not draining
// to the service". The ACK-side fields are updated from the curl network thread while the put-side
// fields are updated from the main thread; the counters are unsynchronized and indicative only.
typedef struct {
    // Put path, reset every report interval
    UINT64 framesPut;
    UINT64 readTimeSum;
    UINT64 readTimeMax;
    UINT64 putTimeSum;
    UINT64 putTimeMax;

    // ACK path, reset every report interval
    UINT64 acksReceived;
    UINT64 acksPersisted;
    UINT64 acksError;
    UINT64 ackLagSumMs;
    UINT64 ackLagMaxMs;
} SampleStats, *PSampleStats;

STATUS sampleFragmentAckHandler(UINT64 customData, STREAM_HANDLE streamHandle, UPLOAD_HANDLE uploadHandle, PFragmentAck pFragmentAck)
{
    PSampleStats pStats = (PSampleStats) customData;
    UINT64 nowMs, lagMs;

    UNUSED_PARAM(streamHandle);
    UNUSED_PARAM(uploadHandle);

    if (pStats == NULL || pFragmentAck == NULL) {
        return STATUS_SUCCESS;
    }

    switch (pFragmentAck->ackType) {
        case FRAGMENT_ACK_TYPE_RECEIVED:
            pStats->acksReceived++;
            break;
        case FRAGMENT_ACK_TYPE_PERSISTED:
            pStats->acksPersisted++;
            break;
        case FRAGMENT_ACK_TYPE_ERROR:
            pStats->acksError++;
            break;
        default:
            // BUFFERING/IDLE ACKs carry no fragment we can age
            return STATUS_SUCCESS;
    }

    // This stream is created with absoluteFragmentTimes, so the ACK timecode is an absolute epoch
    // timestamp in milliseconds. The gap between it and now is the end-to-end age of the fragment at
    // the moment its ACK arrived - i.e. the drift.
    nowMs = GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    lagMs = nowMs > pFragmentAck->timestamp ? nowMs - pFragmentAck->timestamp : 0;
    pStats->ackLagSumMs += lagMs;
    if (lagMs > pStats->ackLagMaxMs) {
        pStats->ackLagMaxMs = lagMs;
    }

    return STATUS_SUCCESS;
}

VOID reportSampleStats(STREAM_HANDLE streamHandle, PSampleStats pStats)
{
    StreamMetrics streamMetrics;
    UINT64 acks = pStats->acksReceived + pStats->acksPersisted + pStats->acksError;

    // Put path: if the device were the bottleneck, putRate would be well under the target frame rate
    // and/or putAvg would be large. putKinesisVideoFrame is non-blocking in realtime mode.
    DLOGI("PUT  frames: %" PRIu64 " (%.1f/s) | readFrameData avg/max: %.2f/%.2f ms | putKinesisVideoFrame avg/max: %.2f/%.2f ms",
          pStats->framesPut, (DOUBLE) pStats->framesPut * HUNDREDS_OF_NANOS_IN_A_SECOND / (DOUBLE) STATS_REPORT_INTERVAL,
          pStats->framesPut == 0 ? 0.0 : (DOUBLE) pStats->readTimeSum / (DOUBLE) pStats->framesPut / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          (DOUBLE) pStats->readTimeMax / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          pStats->framesPut == 0 ? 0.0 : (DOUBLE) pStats->putTimeSum / (DOUBLE) pStats->framesPut / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          (DOUBLE) pStats->putTimeMax / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // ACK path: ackLag is the age of each fragment when its ACK landed. Growing lag with a healthy
    // put rate means the content is queueing locally, not that the device is slow.
    DLOGI("ACK  received: %" PRIu64 " persisted: %" PRIu64 " error: %" PRIu64 " (%.1f acked/s) | fragment age at ACK avg/max: %" PRIu64 "/%" PRIu64 " ms",
          pStats->acksReceived, pStats->acksPersisted, pStats->acksError,
          (DOUBLE) pStats->acksReceived * HUNDREDS_OF_NANOS_IN_A_SECOND / (DOUBLE) STATS_REPORT_INTERVAL, acks == 0 ? 0 : pStats->ackLagSumMs / acks,
          pStats->ackLagMaxMs);

    streamMetrics.version = STREAM_METRICS_CURRENT_VERSION;
    if (STATUS_SUCCEEDED(getKinesisVideoStreamMetrics(streamHandle, &streamMetrics))) {
        // overallViewDuration/Size is tail-to-head: the content still retained on this device, i.e.
        // the real backlog. currentViewDuration/Size is only current-to-head - what the uploader has
        // not been handed yet - and sits at one frame whenever the uploader is keeping up, so it must
        // not be read as "nothing is buffered". currentTransferRate is what actually leaves the box.
        // currentFrameRate is measured off the wall clock between putFrame calls, elementaryFrameRate
        // off the frame presentation timestamps. currentFrameRate < elementaryFrameRate is the device
        // failing to produce frames as fast as the media timeline claims.
        DLOGI("BUF  retained: %" PRIu64 " ms / %" PRIu64 " KB | unsent: %" PRIu64 " ms / %" PRIu64 " KB | transferRate: %" PRIu64
              " Kbps | frameRate wall/media: %.1f/%.1f fps | transferred: %" PRIu64 " KB",
              streamMetrics.overallViewDuration / HUNDREDS_OF_NANOS_IN_A_MILLISECOND, streamMetrics.overallViewSize / 1024,
              streamMetrics.currentViewDuration / HUNDREDS_OF_NANOS_IN_A_MILLISECOND, streamMetrics.currentViewSize / 1024,
              streamMetrics.currentTransferRate * 8 / 1024, streamMetrics.currentFrameRate, streamMetrics.elementaryFrameRate,
              streamMetrics.transferredBytes / 1024);
        DLOGI("PRS  dropped frames: %" PRIu64 " | storage: %" PRIu64 " latency: %" PRIu64 " buffer: %" PRIu64 " pressures | stale: %" PRIu64
              " | putFrame errors: %" PRIu64,
              streamMetrics.droppedFrames, streamMetrics.storagePressures, streamMetrics.latencyPressures, streamMetrics.bufferPressures,
              streamMetrics.staleEvents, streamMetrics.putFrameErrors);
    }

    // Reset the per-interval accumulators, keeping the cumulative view in the PIC metrics above
    pStats->framesPut = 0;
    pStats->readTimeSum = 0;
    pStats->readTimeMax = 0;
    pStats->putTimeSum = 0;
    pStats->putTimeMax = 0;
    pStats->acksReceived = 0;
    pStats->acksPersisted = 0;
    pStats->acksError = 0;
    pStats->ackLagSumMs = 0;
    pStats->ackLagMaxMs = 0;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
    StreamCallbacks statsStreamCallbacks;
    SampleStats stats;
    UINT64 nextStatsReportTime, opStartTime, opDuration;
    UINT32 keyFrameInterval = DEFAULT_KEY_FRAME_INTERVAL;
    PCHAR pKeyFrameInterval = NULL;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR accessKey = NULL, secretKey = NULL, sessionToken = NULL, streamName = NULL, region = NULL, cacertPath = NULL;
    CHAR frameFilePath[MAX_PATH_LEN + 1], metadataKey[METADATA_MAX_KEY_LENGTH + 1], metadataValue[METADATA_MAX_VALUE_LENGTH + 1];
    Frame frame, eofr = EOFR_FRAME_INITIALIZER;
    BYTE frameBuffer[200000]; // Assuming this is enough
    UINT32 frameSize = SIZEOF(frameBuffer), frameIndex = 0, fileIndex = 0, n = 0, numMetadata = 9;
    UINT64 streamStopTime, streamingDuration = DEFAULT_STREAM_DURATION;
    DOUBLE startUpLatency;
    BOOL firstFrame = TRUE;
    UINT64 startTime;
    CHAR videoCodec[VIDEO_CODEC_NAME_MAX_LENGTH];
    SNPRINTF(videoCodec, SIZEOF(videoCodec), "%s", VIDEO_CODEC_NAME_H264); // h264 video by default
    VIDEO_CODEC_ID videoCodecID = VIDEO_CODEC_ID_H264;
    CHAR endpointOverride[MAX_URI_CHAR_LEN];

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    PCHAR pIotCoreCredentialEndpoint, pIotCoreCert, pIotCorePrivateKey, pIotCoreRoleAlias, pIotCoreThingName;
    CHK_ERR((pIotCoreCredentialEndpoint = GETENV(IOT_CORE_CREDENTIAL_ENDPOINT)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_CREDENTIAL_ENDPOINT must be set");
    CHK_ERR((pIotCoreCert = GETENV(IOT_CORE_CERT)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_CERT must be set");
    CHK_ERR((pIotCorePrivateKey = GETENV(IOT_CORE_PRIVATE_KEY)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_PRIVATE_KEY must be set");
    CHK_ERR((pIotCoreRoleAlias = GETENV(IOT_CORE_ROLE_ALIAS)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_ROLE_ALIAS must be set");
    CHK_ERR((pIotCoreRoleAlias = GETENV(IOT_CORE_ROLE_ALIAS)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_ROLE_ALIAS must be set");
    CHK_ERR((pIotCoreThingName = GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set");
#else
    if (argc < 2) {
        DLOGE("Usage: AWS_ACCESS_KEY_ID=SAMPLEKEY AWS_SECRET_ACCESS_KEY=SAMPLESECRET %s <stream_name>"
              "<codec> <duration_in_seconds> <frame_files_path> [num_metadata = 9]\n",
              argv[0]);
        CHK(FALSE, STATUS_INVALID_ARG);
    }
    if ((accessKey = GETENV(ACCESS_KEY_ENV_VAR)) == NULL || (secretKey = GETENV(SECRET_KEY_ENV_VAR)) == NULL) {
        DLOGE("Error missing credentials");
        CHK(FALSE, STATUS_INVALID_ARG);
    }
    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
#endif

    cacertPath = GETENV(CACERT_PATH_ENV_VAR);
#ifdef IOT_CORE_ENABLE_CREDENTIALS
    streamName = pIotCoreThingName;
#else
    streamName = argv[1];
#endif
    if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    if (argc >= 3 && !IS_EMPTY_STRING(argv[2])) {
        if (!STRCMP(argv[2], VIDEO_CODEC_NAME_H265)) {
            SNPRINTF(videoCodec, SIZEOF(videoCodec), "%s", VIDEO_CODEC_NAME_H265);
            videoCodecID = VIDEO_CODEC_ID_H265;
        }
    }

    if (argc >= 4 && !IS_EMPTY_STRING(argv[3])) {
        // Get the duration and convert to an integer
        CHK_STATUS(STRTOUI64(argv[3], NULL, 10, &streamingDuration));
        streamingDuration *= HUNDREDS_OF_NANOS_IN_A_SECOND;
    }

    MEMSET(frameFilePath, 0x00, MAX_PATH_LEN + 1);
    if (argc >= 5 && !IS_EMPTY_STRING(argv[4])) {
        STRNCPY(frameFilePath, argv[4], MAX_PATH_LEN);
    } else {
        STRCPY(frameFilePath, (PCHAR) "../samples/");
    }

    if (argc >= 6 && !IS_EMPTY_STRING(argv[5])) {
        numMetadata = STRTOUL(argv[5], NULL, 10);
        DLOGD("numMetadata: %d\n", numMetadata);
        CHK(numMetadata <= MAX_METADATA_PER_FRAGMENT - 1, STATUS_INVALID_ARG);
    }

    if ((pKeyFrameInterval = GETENV(KEY_FRAME_INTERVAL_ENV_VAR)) != NULL && !IS_EMPTY_STRING(pKeyFrameInterval)) {
        CHK_STATUS(STRTOUI32(pKeyFrameInterval, NULL, 10, &keyFrameInterval));
        CHK(keyFrameInterval > 0, STATUS_INVALID_ARG);
    }
    DLOGI("Key frame interval: %u frame(s) => %.2f fragments/second at %u fps", keyFrameInterval,
          (DOUBLE) DEFAULT_FPS_VALUE / (DOUBLE) keyFrameInterval, DEFAULT_FPS_VALUE);

    streamStopTime = GETTIME() + streamingDuration;

    // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    // adjust members of pDeviceInfo here if needed
    pDeviceInfo->clientInfo.loggerLogLevel = getSampleLogLevel();
    pDeviceInfo->storageInfo.storageSize = DEFAULT_STORAGE_SIZE;

    CHK_STATUS(
        createRealtimeVideoStreamInfoProviderWithCodecs(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION, videoCodecID, &pStreamInfo));
    CHK_STATUS(setStreamInfoBasedOnStorageSize(DEFAULT_STORAGE_SIZE, RECORDED_FRAME_AVG_BITRATE_BIT_PS, 1, pStreamInfo));
    // adjust members of pStreamInfo here if needed

    startTime = GETTIME();

    getEndpointOverride(endpointOverride, SIZEOF(endpointOverride));
#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_STATUS(createDefaultCallbacksProviderWithIotCertificateAndEndpointOverride(pIotCoreCredentialEndpoint, pIotCoreCert, pIotCorePrivateKey,
                                                                                   cacertPath, pIotCoreRoleAlias, pIotCoreThingName, region, NULL,
                                                                                   NULL, endpointOverride, &pClientCallbacks));
#else
    CHK_STATUS(createDefaultCallbacksProviderWithAwsCredentialsAndEndpointOverride(accessKey, secretKey, sessionToken, MAX_UINT64, region, cacertPath,
                                                                                   NULL, NULL, endpointOverride, &pClientCallbacks));
#endif

    if (NULL != GETENV(ENABLE_FILE_LOGGING)) {
        if ((retStatus = addFileLoggerPlatformCallbacksProvider(pClientCallbacks, FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES,
                                                                (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH, TRUE) != STATUS_SUCCESS)) {
            printf("File logging enable option failed with 0x%08x error code\n", retStatus);
        }
    }

    CHK_STATUS(createStreamCallbacks(&pStreamCallbacks));
    CHK_STATUS(addStreamCallbacks(pClientCallbacks, pStreamCallbacks));

    // Register a second set of stream callbacks that only tallies ACKs for the diagnostic report.
    // All registered stream callbacks are invoked, so this does not displace the default ones.
    MEMSET(&stats, 0x00, SIZEOF(stats));
    MEMSET(&statsStreamCallbacks, 0x00, SIZEOF(statsStreamCallbacks));
    statsStreamCallbacks.version = STREAM_CALLBACKS_CURRENT_VERSION;
    statsStreamCallbacks.customData = (UINT64) &stats;
    statsStreamCallbacks.fragmentAckReceivedFn = sampleFragmentAckHandler;
    CHK_STATUS(addStreamCallbacks(pClientCallbacks, &statsStreamCallbacks));

    CHK_STATUS(createKinesisVideoClient(pDeviceInfo, pClientCallbacks, &clientHandle));
    CHK_STATUS(createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    // setup dummy frame
    MEMSET(frameBuffer, 0x00, frameSize);
    frame.frameData = frameBuffer;
    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE;
    frame.decodingTs = GETTIME(); // current time
    frame.presentationTs = frame.decodingTs;

    nextStatsReportTime = GETTIME() + STATS_REPORT_INTERVAL;

    while (GETTIME() < streamStopTime) {
        frame.index = frameIndex;
        frame.flags = fileIndex % keyFrameInterval == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        frame.size = SIZEOF(frameBuffer);

        opStartTime = GETTIME();
        CHK_STATUS(readFrameData(&frame, frameFilePath, videoCodec));
        opDuration = GETTIME() - opStartTime;
        stats.readTimeSum += opDuration;
        if (opDuration > stats.readTimeMax) {
            stats.readTimeMax = opDuration;
        }

        if (frame.flags == FRAME_FLAG_KEY_FRAME && !firstFrame) {
            putKinesisVideoFrame(streamHandle, &eofr);
        }

        opStartTime = GETTIME();
        CHK_STATUS(putKinesisVideoFrame(streamHandle, &frame));
        opDuration = GETTIME() - opStartTime;
        stats.putTimeSum += opDuration;
        if (opDuration > stats.putTimeMax) {
            stats.putTimeMax = opDuration;
        }
        stats.framesPut++;

        if (GETTIME() >= nextStatsReportTime) {
            reportSampleStats(streamHandle, &stats);
            nextStatsReportTime = GETTIME() + STATS_REPORT_INTERVAL;
        }

        if (firstFrame) {
            startUpLatency = (DOUBLE) (GETTIME() - startTime) / (DOUBLE) HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
            DLOGD("Start up latency: %lf ms", startUpLatency);
            firstFrame = FALSE;
        }
        defaultThreadSleep(frame.duration);

        // Add the fragment metadata key-value pairs
        // For limits, refer to https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/limits.html#limits-streaming-metadata
        if (frame.flags == FRAME_FLAG_KEY_FRAME) {
            for (n = 1; n <= numMetadata; n++) {
                SNPRINTF(metadataKey, METADATA_MAX_KEY_LENGTH, "TEST_KEY_%d", n);
                SNPRINTF(metadataValue, METADATA_MAX_VALUE_LENGTH, "TEST_VALUE_%d", frame.index + n);
                CHK_STATUS(putKinesisVideoFragmentMetadata(streamHandle, metadataKey, metadataValue, FALSE));
            }
        }

        frame.decodingTs += frame.duration;
        frame.presentationTs = frame.decodingTs;
        frameIndex++;
        fileIndex++;
        fileIndex = fileIndex % NUMBER_OF_FRAME_FILES;
    }

    putKinesisVideoFrame(streamHandle, &eofr);

    CHK_STATUS(stopKinesisVideoStreamSync(streamHandle));
    CHK_STATUS(freeKinesisVideoStream(&streamHandle));
    CHK_STATUS(freeKinesisVideoClient(&clientHandle));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed with status 0x%08x", retStatus);
    }

    if (pDeviceInfo != NULL) {
        freeDeviceInfo(&pDeviceInfo);
    }

    if (pStreamInfo != NULL) {
        freeStreamInfoProvider(&pStreamInfo);
    }

    if (IS_VALID_STREAM_HANDLE(streamHandle)) {
        freeKinesisVideoStream(&streamHandle);
    }

    if (IS_VALID_CLIENT_HANDLE(clientHandle)) {
        freeKinesisVideoClient(&clientHandle);
    }

    if (pClientCallbacks != NULL) {
        freeCallbacksProvider(&pClientCallbacks);
    }
    return (INT32) retStatus;
}
