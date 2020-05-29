#include <com/amazonaws/kinesis/video/cproducer/Include.h>

#define DEFAULT_RETENTION_PERIOD            2 * HUNDREDS_OF_NANOS_IN_AN_HOUR
#define DEFAULT_BUFFER_DURATION             120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_CALLBACK_CHAIN_COUNT        5
#define DEFAULT_KEY_FRAME_INTERVAL          45
#define DEFAULT_FPS_VALUE                   25
#define DEFAULT_STREAM_DURATION             20 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_STORAGE_SIZE                20 * 1024 * 1024
#define RECORDED_FRAME_AVG_BITRATE_BIT_PS   3800000

#define NUMBER_OF_FRAME_FILES               403

STATUS readFrameData(PFrame pFrame, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 index;
    UINT64 size;
    CHK(pFrame != NULL, STATUS_NULL_ARG);

    index = pFrame->index % NUMBER_OF_FRAME_FILES + 1;
    SNPRINTF(filePath, MAX_PATH_LEN, "%s/frame-%03d.h264", frameFilePath, index);
    size = pFrame->size;

    // Get the size and read into frame
    CHK_STATUS(readFile(filePath, TRUE, NULL, &size));
    CHK_STATUS(readFile(filePath, TRUE, pFrame->frameData, &size));

    pFrame->size = (UINT32) size;

    if (pFrame->flags == FRAME_FLAG_KEY_FRAME) {
        defaultLogPrint(LOG_LEVEL_DEBUG, "", "Key frame file %s, size %" PRIu64, filePath, pFrame->size);
    }

CleanUp:

    return retStatus;
}

void helper() {
    printf("List of available options:\n" \
           "1. --stream_name      : Set name of stream. If not selected, default stream name is set\n" \
           "2. --file_log         : No args needed. If set, file logging is enabled\n" \
           "3. --file_log_path    : Ensure --file_log option is provided. Set file path\n" \
           "4. --duration         : Streaming duration in seconds\n" \
           "5. --frame_file_path  : Frame file path\n " \
           "6. --help             : Look at the list of available args\n");
}
// Forward declaration of the default thread sleep function
VOID defaultThreadSleep(UINT64);

INT32 main(INT32 argc, CHAR *argv[])
{
    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR accessKey = NULL, secretKey = NULL, sessionToken = NULL, streamName = NULL, region = NULL, cacertPath = NULL;
    CHAR frameFilePath[MAX_PATH_LEN + 1];
    Frame frame;
    BYTE frameBuffer[200000]; // Assuming this is enough
    UINT32 frameSize = SIZEOF(frameBuffer), frameIndex = 0, fileIndex = 0;
    UINT64 streamStopTime, streamingDuration = DEFAULT_STREAM_DURATION;
    INT32 opt_result;
    CHAR opt[256];
    PCHAR fileLogPath = (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH;
    BOOL fileLogEnable = FALSE;
    STRCPY(frameFilePath, (PCHAR) "../samples/h264SampleFrames");

    if (argc < 2) {
        helper();
        goto CleanUp;
    }
    while((opt_result = parseCommandLineOptions(argc, argv, opt)) != -1) {
        if(STRCMP(opt, "help") == 0) {
            helper();
            goto CleanUp;
        }

        else if(STRCMP(opt, "stream_name") == 0) {
            if(opt_result != 0) {
                streamName = argv[opt_result];
            }
            else {
                printf("Please set the stream name\n");
                CHK(FALSE, STATUS_INVALID_ARG);
            }
        }

        else if(STRCMP(opt, "file_log") == 0) {
            fileLogEnable = TRUE;
        }

        else if(STRCMP(opt, "file_log_path") == 0) {
            if(opt_result != 0) {
                fileLogPath = (PCHAR) argv[opt_result];
            }
            else {
                printf("Please set the file log path\n");
                CHK(FALSE, STATUS_INVALID_ARG);
            }
        }
        else if(STRCMP(opt, "duration") == 0) {
            if(opt_result != 0) {
                CHK_STATUS(STRTOUI64(argv[opt_result], NULL, 10, &streamingDuration));
                streamingDuration *= HUNDREDS_OF_NANOS_IN_A_SECOND;
            }
            else {
                printf("Please set the required streaming duration in seconds\n");
                CHK(FALSE, STATUS_INVALID_ARG);
            }
        }
        else if(STRCMP(opt, "frame_file_path") == 0) {
            if(opt_result != 0) {
                STRNCPY(frameFilePath, argv[opt_result], STRLEN(argv[opt_result]) + 1);
            }
            else {
                printf("Please set a valid file path\n");
                CHK(FALSE, STATUS_INVALID_ARG);
            }
        }
        MEMSET(opt, '\0', SIZEOF(opt));
    }

    if(fileLogEnable) {
        if(createFileLogger(FILE_LOGGER_STRING_BUFFER_SIZE, FILE_LOGGER_LOG_FILE_COUNT, fileLogPath, TRUE, TRUE, NULL) != STATUS_SUCCESS) {
            printf("Unable to set file logger....no logging will be available\n");
        }
    }

    if ((accessKey = getenv(ACCESS_KEY_ENV_VAR)) == NULL || (secretKey = getenv(SECRET_KEY_ENV_VAR)) == NULL) {
        defaultLogPrint(LOG_LEVEL_ERROR, "", "Error missing credentials");
        CHK(FALSE, STATUS_INVALID_ARG);
    }

    cacertPath = getenv(CACERT_PATH_ENV_VAR);
    sessionToken = getenv(SESSION_TOKEN_ENV_VAR);

    if ((region = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    streamStopTime = defaultGetTime() + streamingDuration;

    // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    // adjust members of pDeviceInfo here if needed
    pDeviceInfo->clientInfo.loggerLogLevel = LOG_LEVEL_DEBUG;
    pDeviceInfo->storageInfo.storageSize = DEFAULT_STORAGE_SIZE;

    CHK_STATUS(createRealtimeVideoStreamInfoProvider(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION, &pStreamInfo));
    CHK_STATUS(setStreamInfoBasedOnStorageSize(DEFAULT_STORAGE_SIZE, RECORDED_FRAME_AVG_BITRATE_BIT_PS, 1, pStreamInfo));
    // adjust members of pStreamInfo here if needed

    CHK_STATUS(createDefaultCallbacksProviderWithAwsCredentials(accessKey,
                                                                secretKey,
                                                                sessionToken,
                                                                MAX_UINT64,
                                                                region,
                                                                cacertPath,
                                                                NULL,
                                                                NULL,
                                                                &pClientCallbacks));
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
    frame.decodingTs = defaultGetTime(); // current time
    frame.presentationTs = frame.decodingTs;

    while(defaultGetTime() < streamStopTime) {
        frame.index = frameIndex;
        frame.flags = fileIndex % DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        frame.size = SIZEOF(frameBuffer);

        CHK_STATUS(readFrameData(&frame, frameFilePath));

        CHK_STATUS(putKinesisVideoFrame(streamHandle, &frame));
        defaultThreadSleep(frame.duration);

        frame.decodingTs += frame.duration;
        frame.presentationTs = frame.decodingTs;
        frameIndex++;
        fileIndex++;
        fileIndex = fileIndex % NUMBER_OF_FRAME_FILES;
    }

    CHK_STATUS(stopKinesisVideoStreamSync(streamHandle));
    CHK_STATUS(freeKinesisVideoStream(&streamHandle));
    CHK_STATUS(freeKinesisVideoClient(&clientHandle));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        defaultLogPrint(LOG_LEVEL_ERROR, "", "Failed with status 0x%08x\n", retStatus);
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
