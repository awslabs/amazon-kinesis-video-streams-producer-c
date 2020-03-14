#include <com/amazonaws/kinesis/video/cproducer/Include.h>
#include <com/amazonaws/kinesis/video/utils/Include.h>
#include "CanaryStreamCallbacks.h"

volatile BOOL sigCaptureInterrupt = FALSE;
UINT32 updateCrc32(UINT32, PBYTE, UINT32);

#define DEFAULT_RETENTION_PERIOD            (2 * HUNDREDS_OF_NANOS_IN_AN_HOUR)
#define DEFAULT_BUFFER_DURATION             (120 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define DEFAULT_CALLBACK_CHAIN_COUNT        5
#define DEFAULT_KEY_FRAME_INTERVAL          45
#define DEFAULT_FPS_VALUE                   25
#define DEFAULT_STREAM_DURATION             (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define NUMBER_OF_FRAME_FILES               403
#define CANARY_METADATA_SIZE                (SIZEOF(INT64) + SIZEOF(UINT32) + SIZEOF(UINT32) + SIZEOF(UINT64))

VOID sigintHandler(INT32 sigNum)
{
    sigCaptureInterrupt = TRUE;
}

// add frame pts, frame index, original frame size, CRC to beginning of buffer
VOID addCanaryMetadataToFrameData(PFrame pFrame) {
    putInt64((PINT64)pFrame->frameData, pFrame->presentationTs / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    putInt32((PINT32) (pFrame->frameData + SIZEOF(UINT64)), pFrame->index);
    putInt32((PINT32) (pFrame->frameData + SIZEOF(UINT64) + SIZEOF(UINT32)), pFrame->size);
    MEMSET(pFrame->frameData + SIZEOF(UINT64) + SIZEOF(UINT32) + SIZEOF(UINT32), 0x00, SIZEOF(UINT64));
    putInt64((PINT64) (pFrame->frameData + SIZEOF(UINT64) + SIZEOF(UINT32) + SIZEOF(UINT32)), updateCrc32(0, pFrame->frameData, pFrame->size));
}

VOID createCanaryFrameData(PFrame pFrame) {
    UINT32 i;

    for (i = CANARY_METADATA_SIZE; i < pFrame->size; i++) {
        pFrame->frameData[i] = RAND();
    }
    addCanaryMetadataToFrameData(pFrame);
}

PCHAR getCanaryStr(UINT32 canaryType) {
    switch (canaryType) {
        case 0:
            return (PCHAR)"realtime";
        case 1:
            return (PCHAR)"offline";
        default:
            return (PCHAR)"";
    }
}

VOID adjustStreamInfoToCanaryType(PStreamInfo pStreamInfo, UINT32 canaryType) {
    switch (canaryType) {
        case 0:
            pStreamInfo->streamCaps.streamingType = STREAMING_TYPE_REALTIME;
            break;
        case 1:
            pStreamInfo->streamCaps.streamingType = STREAMING_TYPE_OFFLINE;
            break;
        default:
            break;
    }

}

INT32 main(INT32 argc, CHAR *argv[])
{

    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR accessKey = NULL, secretKey = NULL, sessionToken = NULL, streamNamePrefix = NULL, canaryTypeStr = NULL, region = NULL, cacertPath = NULL;
    CHAR streamName[MAX_STREAM_NAME_LEN + 1];
    Frame frame;
    UINT32 frameIndex = 0, fileIndex = 0, canaryType = 0;
    UINT64 fragmentSizeInByte = 0;
    UINT64 lastKeyFrameTimestamp = 0;
    PCanaryStreamCallbacks pCanaryStreamCallbacks = NULL;
    pCanaryStreamCallbacks = (PCanaryStreamCallbacks) MEMCALLOC(1, SIZEOF(CanaryStreamCallbacks));

    Aws::SDKOptions options;
    Aws::InitAPI(options);
    {
        frame.frameData = NULL;

        if (argc < 3) {
            defaultLogPrint(LOG_LEVEL_ERROR, (PCHAR) "", (PCHAR) "Usage: AWS_ACCESS_KEY_ID=SAMPLEKEY AWS_SECRET_ACCESS_KEY=SAMPLESECRET %s <stream_name_prefix> <canary_type> <bandwidth>\n", argv[0]);
            CHK(FALSE, STATUS_INVALID_ARG);
        }

        if ((accessKey = getenv(ACCESS_KEY_ENV_VAR)) == NULL || (secretKey = getenv(SECRET_KEY_ENV_VAR)) == NULL) {
            defaultLogPrint(LOG_LEVEL_ERROR, (PCHAR) "", (PCHAR) "Error missing credentials");
            CHK(FALSE, STATUS_INVALID_ARG);
        }

        if (argc < 4) {
            fragmentSizeInByte = 1024 * 1024; // default to 1 MB
        } else {
            STRTOUI64(argv[3], NULL, 10, &fragmentSizeInByte);
        }

        cacertPath = getenv(CACERT_PATH_ENV_VAR);
        sessionToken = getenv(SESSION_TOKEN_ENV_VAR);
        streamNamePrefix = argv[1];
        STRTOUI32(argv[2], NULL, 10, &canaryType);;
        canaryTypeStr = getCanaryStr(canaryType);
        CHK(STRCMP(canaryTypeStr, "") != 0, STATUS_INVALID_ARG);
        SNPRINTF(streamName, MAX_STREAM_NAME_LEN, "%s-canary-%s-%s", streamNamePrefix, canaryTypeStr, argv[3]);
        if ((region = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
            region = (PCHAR) DEFAULT_AWS_REGION;
        }

        Aws::Client::ClientConfiguration clientConfiguration;
        clientConfiguration.region = region;
        Aws::CloudWatch::CloudWatchClient cw(clientConfiguration);

        // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
        CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
        // adjust members of pDeviceInfo here if needed
        pDeviceInfo->clientInfo.loggerLogLevel = LOG_LEVEL_DEBUG;

        CHK_STATUS(createRealtimeVideoStreamInfoProvider(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION, &pStreamInfo));
        adjustStreamInfoToCanaryType(pStreamInfo, canaryType);
        // adjust members of pStreamInfo here if needed
        pStreamInfo->streamCaps.nalAdaptationFlags = NAL_ADAPTATION_FLAG_NONE;

        CHK_STATUS(createDefaultCallbacksProviderWithAwsCredentials(accessKey,
                                                                    secretKey,
                                                                    sessionToken,
                                                                    MAX_UINT64,
                                                                    region,
                                                                    cacertPath,
                                                                    NULL,
                                                                    NULL,
                                                                    &pClientCallbacks));

        CHK_STATUS(createCanaryStreamCallbacks(&cw, streamName, &pStreamCallbacks, pCanaryStreamCallbacks));
        CHK_STATUS(addStreamCallbacks(pClientCallbacks, pStreamCallbacks));
        CHK_STATUS(createKinesisVideoClient(pDeviceInfo, pClientCallbacks, &clientHandle));
        CHK_STATUS(createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

        // setup dummy frame
        frame.size = CANARY_METADATA_SIZE + fragmentSizeInByte / DEFAULT_FPS_VALUE;
        frame.frameData = (PBYTE) MEMALLOC(frame.size);
        frame.version = FRAME_CURRENT_VERSION;
        frame.trackId = DEFAULT_VIDEO_TRACK_ID;
        frame.duration = 0;
        frame.decodingTs = defaultGetTime(); // current time
        frame.presentationTs = frame.decodingTs;

        while (sigCaptureInterrupt != TRUE) {
            if (frameIndex < 0) {
                frameIndex = 0;
            }
            frame.index = frameIndex;
            frame.flags = frameIndex % DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;

            createCanaryFrameData(&frame);

            if (frame.flags == FRAME_FLAG_KEY_FRAME) {
                if (lastKeyFrameTimestamp != 0) {
                    CanaryStreamRecordFragmentEndSendTime(pStreamCallbacks, lastKeyFrameTimestamp, frame.presentationTs);
                    ComputeStreamMetricsFromCanary(streamHandle, pCanaryStreamCallbacks);
                    ComputeClientMetricsFromCanary(clientHandle, pCanaryStreamCallbacks);
                }
                lastKeyFrameTimestamp = frame.presentationTs;
            }
            CHK_STATUS(putKinesisVideoFrame(streamHandle, &frame));

            THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE);

            frame.decodingTs = defaultGetTime(); // current time
            frame.presentationTs = frame.decodingTs;
            frameIndex++;
        }

    }
CleanUp:

    Aws::ShutdownAPI(options);

    if (STATUS_FAILED(retStatus)) {
        defaultLogPrint(LOG_LEVEL_ERROR, "", "Failed with status 0x%08x\n", retStatus);
    }

    if (frame.frameData != NULL) {
        MEMFREE(frame.frameData);
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
