#include "Samples.h"

#define DEFAULT_RETENTION_PERIOD        2 * HUNDREDS_OF_NANOS_IN_AN_HOUR
#define DEFAULT_BUFFER_DURATION         120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_KEY_FRAME_INTERVAL      100
#define DEFAULT_STREAM_DURATION         20 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define SAMPLE_AUDIO_FRAME_DURATION     (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define AAC_AUDIO_TRACK_SAMPLING_RATE   48000
#define ALAW_AUDIO_TRACK_SAMPLING_RATE  8000
#define AAC_AUDIO_TRACK_CHANNEL_CONFIG  2
#define ALAW_AUDIO_TRACK_CHANNEL_CONFIG 1
#define AUDIO_CODEC_NAME_MAX_LENGTH     5
#define AUDIO_CODEC_NAME_ALAW           "alaw"
#define AUDIO_CODEC_NAME_AAC            "aac"

#define NUMBER_OF_AUDIO_FRAME_FILES 582

// #define IOT_CORE_ENABLE_CREDENTIALS 1

typedef struct {
    PBYTE buffer;
    UINT32 size;
} FrameData, *PFrameData;

typedef struct {
    UINT64 streamStopTime;
    UINT64 streamStartTime;
    STREAM_HANDLE streamHandle;
    CHAR sampleDir[MAX_PATH_LEN + 1];
    FrameData audioFrames[NUMBER_OF_AUDIO_FRAME_FILES];
    BOOL firstFrame;
    UINT64 startTime;
} SampleCustomData, *PSampleCustomData;

PVOID putAudioFrameRoutine(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleCustomData data = (PSampleCustomData) args;
    Frame frame;
    UINT32 fileIndex = 0;
    STATUS status;
    UINT64 runningTime;

    CHK(data != NULL, STATUS_NULL_ARG);

    frame.frameData = data->audioFrames[fileIndex].buffer;
    frame.size = data->audioFrames[fileIndex].size;
    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_AUDIO_ONLY_TRACK_ID;
    frame.duration = 0;
    frame.decodingTs = 0;     // relative time mode
    frame.presentationTs = 0; // relative time mode
    frame.index = 0;
    frame.flags = fileIndex % DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;

    while (GETTIME() < data->streamStopTime) {
        status = putKinesisVideoFrame(data->streamHandle, &frame);
        if (STATUS_FAILED(status)) {
            printf("putKinesisVideoFrame for audio failed with 0x%08x\n", status);
            status = STATUS_SUCCESS;
        }

        frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;
        frame.decodingTs = frame.presentationTs;
        frame.index++;

        fileIndex = (fileIndex + 1) % NUMBER_OF_AUDIO_FRAME_FILES;
        frame.flags = fileIndex % DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        frame.frameData = data->audioFrames[fileIndex].buffer;
        frame.size = data->audioFrames[fileIndex].size;

        // synchronize putKinesisVideoFrame to running time
        runningTime = GETTIME() - data->streamStartTime;
        if (runningTime < frame.presentationTs) {
            THREAD_SLEEP(frame.presentationTs - runningTime);
        }
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("putAudioFrameRoutine failed with 0x%08x", retStatus);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

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
    UINT64 streamStopTime, streamingDuration = DEFAULT_STREAM_DURATION, fileSize = 0;
    TID audioSendTid;
    SampleCustomData data;
    UINT32 i;
    CHAR filePath[MAX_PATH_LEN + 1];
    PTrackInfo pAudioTrack = NULL;
    CHAR audioCodec[AUDIO_CODEC_NAME_MAX_LENGTH];
    BYTE aacAudioCpd[KVS_AAC_CPD_SIZE_BYTE];
    BYTE alawAudioCpd[KVS_PCM_CPD_SIZE_BYTE];
    CHAR endpointOverride[MAX_URI_CHAR_LEN];

    MEMSET(&data, 0x00, SIZEOF(SampleCustomData));

    SNPRINTF(audioCodec, SIZEOF(audioCodec), "%s", AUDIO_CODEC_NAME_AAC); // aac audio by default

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    PCHAR pIotCoreCredentialEndpoint, pIotCoreCert, pIotCorePrivateKey, pIotCoreRoleAlias, pIotCoreThingName;
    CHK_ERR((pIotCoreCredentialEndpoint = GETENV(IOT_CORE_CREDENTIAL_ENDPOINT)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_CREDENTIAL_ENDPOINT must be set");
    CHK_ERR((pIotCoreCert = GETENV(IOT_CORE_CERT)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_CERT must be set");
    CHK_ERR((pIotCorePrivateKey = GETENV(IOT_CORE_PRIVATE_KEY)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_PRIVATE_KEY must be set");
    CHK_ERR((pIotCoreRoleAlias = GETENV(IOT_CORE_ROLE_ALIAS)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_ROLE_ALIAS must be set");
    CHK_ERR((pIotCoreThingName = GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set");
#else
    if (argc < 2) {
        printf("Usage: AWS_ACCESS_KEY_ID=SAMPLEKEY AWS_SECRET_ACCESS_KEY=SAMPLESECRET %s <stream_name> <duration_in_seconds> <frame_files_path>\n",
               argv[0]);
        CHK(FALSE, STATUS_INVALID_ARG);
    }
    if ((accessKey = GETENV(ACCESS_KEY_ENV_VAR)) == NULL || (secretKey = GETENV(SECRET_KEY_ENV_VAR)) == NULL) {
        printf("Error missing credentials\n");
        CHK(FALSE, STATUS_INVALID_ARG);
    }
    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
#endif

    if (argc >= 5) {
        if (!STRCMP(argv[2], AUDIO_CODEC_NAME_ALAW)) {
            SNPRINTF(audioCodec, SIZEOF(audioCodec), "%s", AUDIO_CODEC_NAME_ALAW);
        }
    }

    MEMSET(data.sampleDir, 0x00, MAX_PATH_LEN + 1);
    if (argc < 4) {
        STRCPY(data.sampleDir, (PCHAR) "../samples");
    } else {
        STRNCPY(data.sampleDir, argv[3], MAX_PATH_LEN);
        if (data.sampleDir[STRLEN(data.sampleDir) - 1] == '/') {
            data.sampleDir[STRLEN(data.sampleDir) - 1] = '\0';
        }
    }

    printf("Loading audio frames...\n");
    for (i = 0; i < NUMBER_OF_AUDIO_FRAME_FILES; ++i) {
        SNPRINTF(filePath, MAX_PATH_LEN, "%s/%sSampleFrames/sample-%03d.%s", data.sampleDir, audioCodec, i + 1, audioCodec);
        CHK_STATUS(readFile(filePath, TRUE, NULL, &fileSize));
        data.audioFrames[i].buffer = (PBYTE) MEMALLOC(fileSize);
        data.audioFrames[i].size = fileSize;
        CHK_STATUS(readFile(filePath, TRUE, data.audioFrames[i].buffer, &fileSize));
    }
    printf("Done loading audio frames.\n");

    cacertPath = GETENV(CACERT_PATH_ENV_VAR);
    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    streamName = pIotCoreThingName;
#else
    streamName = argv[1];
#endif

    if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    if (argc >= 3) {
        // Get the duration and convert to an integer
        CHK_STATUS(STRTOUI64(argv[2], NULL, 10, &streamingDuration));
        printf("streaming for %" PRIu64 " seconds\n", streamingDuration);
        streamingDuration *= HUNDREDS_OF_NANOS_IN_A_SECOND;
    }

    streamStopTime = GETTIME() + streamingDuration;

    // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    // adjust members of pDeviceInfo here if needed
    pDeviceInfo->clientInfo.loggerLogLevel = LOG_LEVEL_DEBUG;

    // generate audio cpd
    if (!STRCMP(audioCodec, AUDIO_CODEC_NAME_ALAW)) {
        CHK_STATUS(createRealtimeAudioStreamInfoProviderWithCodecs(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION,
                                                                   AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));

        // adjust members of pStreamInfo here if needed

        // set up audio cpd.
        pAudioTrack = pStreamInfo->streamCaps.trackInfoList[0].trackId == 1 ? &pStreamInfo->streamCaps.trackInfoList[0]
                                                                            : &pStreamInfo->streamCaps.trackInfoList[1];
        pAudioTrack->codecPrivateData = alawAudioCpd;
        pAudioTrack->codecPrivateDataSize = KVS_PCM_CPD_SIZE_BYTE;
        CHK_STATUS(mkvgenGeneratePcmCpd(KVS_PCM_FORMAT_CODE_ALAW, ALAW_AUDIO_TRACK_SAMPLING_RATE, ALAW_AUDIO_TRACK_CHANNEL_CONFIG,
                                        pAudioTrack->codecPrivateData, pAudioTrack->codecPrivateDataSize));
    } else {
        CHK_STATUS(createRealtimeAudioStreamInfoProviderWithCodecs(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION, AUDIO_CODEC_ID_AAC,
                                                                   &pStreamInfo));

        // To specify PCM/G.711 use createRealtimeAudioStreamInfoProviderWithCodecs
        // adjust members of pStreamInfo here if needed

        // set up audio cpd.
        pAudioTrack = pStreamInfo->streamCaps.trackInfoList[0].trackId == 1 ? &pStreamInfo->streamCaps.trackInfoList[0]
                                                                            : &pStreamInfo->streamCaps.trackInfoList[1];
        pAudioTrack->codecPrivateData = aacAudioCpd;
        pAudioTrack->codecPrivateDataSize = KVS_AAC_CPD_SIZE_BYTE;
        CHK_STATUS(mkvgenGenerateAacCpd(AAC_LC, AAC_AUDIO_TRACK_SAMPLING_RATE, AAC_AUDIO_TRACK_CHANNEL_CONFIG, pAudioTrack->codecPrivateData,
                                        pAudioTrack->codecPrivateDataSize));
    }

    // use relative time mode. Buffer timestamps start from 0
    pStreamInfo->streamCaps.absoluteFragmentTimes = FALSE;

    data.startTime = GETTIME();
    data.firstFrame = TRUE;

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

    data.streamStopTime = streamStopTime;
    data.streamHandle = streamHandle;
    data.streamStartTime = GETTIME();

    THREAD_CREATE(&audioSendTid, putAudioFrameRoutine, (PVOID) &data);

    THREAD_JOIN(audioSendTid, NULL);

    CHK_STATUS(stopKinesisVideoStreamSync(streamHandle));
    CHK_STATUS(freeKinesisVideoStream(&streamHandle));
    CHK_STATUS(freeKinesisVideoClient(&clientHandle));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        printf("Failed with status 0x%08x\n", retStatus);
    }

    for (i = 0; i < NUMBER_OF_AUDIO_FRAME_FILES; ++i) {
        SAFE_MEMFREE(data.audioFrames[i].buffer);
    }

    freeDeviceInfo(&pDeviceInfo);
    freeStreamInfoProvider(&pStreamInfo);
    freeKinesisVideoStream(&streamHandle);
    freeKinesisVideoClient(&clientHandle);
    freeCallbacksProvider(&pClientCallbacks);

    return (INT32) retStatus;
}