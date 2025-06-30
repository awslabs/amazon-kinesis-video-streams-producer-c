#include "Samples.h"

#define DEFAULT_RETENTION_PERIOD          2 * HUNDREDS_OF_NANOS_IN_AN_HOUR
#define DEFAULT_BUFFER_DURATION           120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_CALLBACK_CHAIN_COUNT      5
#define DEFAULT_KEY_FRAME_INTERVAL        45
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

INT32 main(INT32 argc, CHAR* argv[])
{
    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
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

    streamStopTime = GETTIME() + streamingDuration;

    // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    // adjust members of pDeviceInfo here if needed
    pDeviceInfo->clientInfo.loggerLogLevel = LOG_LEVEL_DEBUG;
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

    while (GETTIME() < streamStopTime) {
        frame.index = frameIndex;
        frame.flags = fileIndex % DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        frame.size = SIZEOF(frameBuffer);

        CHK_STATUS(readFrameData(&frame, frameFilePath, videoCodec));

        if (frame.flags == FRAME_FLAG_KEY_FRAME && !firstFrame) {
            putKinesisVideoFrame(streamHandle, &eofr);
        }

        CHK_STATUS(putKinesisVideoFrame(streamHandle, &frame));
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
