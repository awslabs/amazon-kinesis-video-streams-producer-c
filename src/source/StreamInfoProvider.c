/*
 *  Kinesis Video C Producer Default Stream Info Provider
 */
#define LOG_CLASS "StreamInfoProvider"
#include "Include_i.h"

// Creates video stream info for real time streaming mode
STATUS createRealtimeVideoStreamInfoProvider(PCHAR streamName, UINT64 retention,
                                             UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{

    return createVideoStreamInfo(STREAMING_TYPE_REALTIME, streamName,
                                 retention, bufferDuration, ppStreamInfo);
}

// Creates video stream info for offline streaming mode
STATUS createOfflineVideoStreamInfoProvider(PCHAR streamName, UINT64 retention,
                                            UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{

    return createVideoStreamInfo(STREAMING_TYPE_OFFLINE, streamName,
                                 retention, bufferDuration, ppStreamInfo);
}
// Creates audio video stream info for real time streaming mode
STATUS createRealtimeAudioVideoStreamInfoProvider(PCHAR streamName, UINT64 retention,
                                                  UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{

    return createAudioVideoStreamInfo(STREAMING_TYPE_REALTIME, streamName,
                                      retention, bufferDuration, ppStreamInfo);
}

// Creates audio video stream info for offline streaming mode
STATUS createOfflineAudioVideoStreamInfoProvider(PCHAR streamName, UINT64 retention,
                                                 UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{

    return createAudioVideoStreamInfo(STREAMING_TYPE_OFFLINE, streamName,
                                      retention, bufferDuration, ppStreamInfo);
}

// Frees the stream info
STATUS freeStreamInfoProvider(PStreamInfo* ppStreamInfo)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PStreamInfo pStreamInfo;

    CHK(ppStreamInfo != NULL, STATUS_NULL_ARG);
    CHK(*ppStreamInfo != NULL, retStatus);

    pStreamInfo = *ppStreamInfo;

    MEMFREE(pStreamInfo);

    *ppStreamInfo = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

// Creates H264 track info for video
STATUS createH264VideoTrackInfo(PTrackInfo pTrackInfo)
{
    STATUS retStatus = STATUS_SUCCESS;

    pTrackInfo->trackId = DEFAULT_VIDEO_TRACK_ID;
    pTrackInfo->codecPrivateData = NULL;
    pTrackInfo->codecPrivateDataSize = 0;
    STRCPY(pTrackInfo->codecId, MKV_H264_AVC_CODEC_ID);
    STRCPY(pTrackInfo->trackName, DEFAULT_VIDEO_TRACK_NAME);
    pTrackInfo->trackType = MKV_TRACK_INFO_TYPE_VIDEO;

    return retStatus;
}

// Creates AAC track info for audio
STATUS createAacAudioTrackInfo(PTrackInfo pTrackInfo)
{
    STATUS retStatus = STATUS_SUCCESS;

    pTrackInfo->trackId = DEFAULT_AUDIO_TRACK_ID;
    pTrackInfo->codecPrivateData = NULL;
    pTrackInfo->codecPrivateDataSize = 0;
    STRCPY(pTrackInfo->codecId, MKV_AAC_CODEC_ID);
    STRCPY(pTrackInfo->trackName, DEFAULT_AUDIO_TRACK_NAME);
    pTrackInfo->trackType = MKV_TRACK_INFO_TYPE_AUDIO;

    return retStatus;
}

STATUS setStreamInfoDefaults(STREAMING_TYPE streamingType, UINT64 retention, UINT64 bufferDuration, UINT32 trackCount,
                             PStreamInfo pStreamInfo, PTrackInfo pTrackInfo)
{
    STATUS retStatus = STATUS_SUCCESS;

    pStreamInfo->streamCaps.absoluteFragmentTimes = TRUE;
    pStreamInfo->streamCaps.adaptive = TRUE;
    pStreamInfo->streamCaps.avgBandwidthBps = DEFAULT_AVG_BANDWIDTH;
    pStreamInfo->streamCaps.bufferDuration = bufferDuration;
    pStreamInfo->streamCaps.connectionStalenessDuration =
    pStreamInfo->streamCaps.connectionStalenessDuration = STREAM_INFO_DEFAULT_CONNECTION_STALE_DURATION;
    pStreamInfo->streamCaps.frameRate = DEFAULT_VIDEO_AUDIO_FRAME_RATE;
    pStreamInfo->streamCaps.fragmentDuration = STREAM_INFO_DEFAULT_FRAGMENT_DURATION;
    pStreamInfo->streamCaps.fragmentAcks = TRUE;
    pStreamInfo->streamCaps.frameTimecodes = TRUE;
    pStreamInfo->streamCaps.frameOrderingMode =
            trackCount == 1 ? FRAME_ORDER_MODE_PASS_THROUGH : FRAME_ORDERING_MODE_MULTI_TRACK_AV_COMPARE_PTS_ONE_MS_COMPENSATE;
    pStreamInfo->streamCaps.keyFrameFragmentation = TRUE;
    pStreamInfo->streamCaps.maxLatency =
            (UINT64) (LATENCY_PRESSURE_FACTOR * ((DOUBLE) bufferDuration));
    pStreamInfo->streamCaps.nalAdaptationFlags = NAL_ADAPTATION_ANNEXB_CPD_NALS | NAL_ADAPTATION_ANNEXB_NALS;
    pStreamInfo->streamCaps.recalculateMetrics = TRUE;
    pStreamInfo->streamCaps.recoverOnError = TRUE;
    pStreamInfo->streamCaps.replayDuration =
            (UINT64) (REPLAY_DURATION_FACTOR * ((DOUBLE) bufferDuration));
    pStreamInfo->streamCaps.streamingType = streamingType;
    pStreamInfo->streamCaps.segmentUuid = NULL;
    pStreamInfo->streamCaps.timecodeScale = STREAM_INFO_DEFAULT_TIMESCALE;
    pStreamInfo->streamCaps.trackInfoCount = trackCount;
    pStreamInfo->streamCaps.trackInfoList = pTrackInfo;
    // when putFrame could cause OOM error from buffer, drop tail fragment
    pStreamInfo->streamCaps.storePressurePolicy = CONTENT_STORE_PRESSURE_POLICY_DROP_TAIL_ITEM;
    // when content view is full, drop tail fragment
    pStreamInfo->streamCaps.viewOverflowPolicy = CONTENT_VIEW_OVERFLOW_POLICY_DROP_UNTIL_FRAGMENT_START;

    pStreamInfo->kmsKeyId[0] = '\0';
    pStreamInfo->retention = retention;
    pStreamInfo->tagCount = 0;
    pStreamInfo->tags = NULL;
    pStreamInfo->version = STREAM_INFO_CURRENT_VERSION;

    return retStatus;
}

STATUS createVideoStreamInfo(STREAMING_TYPE streamingType, PCHAR streamName,
                             UINT64 retention, UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PStreamInfo pStreamInfo = NULL;

    CHK(ppStreamInfo != NULL && streamName != NULL, STATUS_NULL_ARG);
    CHK(bufferDuration != 0, STATUS_INVALID_ARG);

    CHK(streamingType != STREAMING_TYPE_OFFLINE || retention != 0, STATUS_INVALID_ARG);

    // Allocate the entire structure
    pStreamInfo = (PStreamInfo) MEMCALLOC(1, SIZEOF(StreamInfo) + VIDEO_ONLY_TRACK_COUNT * SIZEOF(TrackInfo));
    CHK(pStreamInfo != NULL, STATUS_NOT_ENOUGH_MEMORY);

    PTrackInfo pTrackInfo = NULL;
    pTrackInfo = (PTrackInfo) (pStreamInfo + 1);

    CHK_STATUS(createH264VideoTrackInfo(pTrackInfo));

    STRCPY(pStreamInfo->name, streamName);
    STRCPY(pStreamInfo->streamCaps.contentType, MKV_H264_CONTENT_TYPE);
    CHK_STATUS(setStreamInfoDefaults(streamingType, retention, bufferDuration, VIDEO_ONLY_TRACK_COUNT,
                                     pStreamInfo, pTrackInfo));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeStreamInfoProvider(&pStreamInfo);
        pStreamInfo = NULL;
    }

    if (pStreamInfo != NULL) {
        *ppStreamInfo = pStreamInfo;
    }

    LEAVES();
    return retStatus;
}

STATUS createStreamInfoFromConfigFile(PStreamInfo pCreateStreamInfoFromConfigFile, PCHAR filePath) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pCreateStreamInfoFromConfigFile != NULL && filePath != NULL, STATUS_NULL_ARG);
    UINT64 fileSize = 0;
    BYTE params[1024];
    CHAR frameFilePath[MAX_PATH_LEN + 1];
    jsmn_parser parser;
    jsmn_init(&parser);
    jsmntok_t tokens[256];
    INT64 tokenCount;
    CHAR finalAttr[256];

    MEMSET(frameFilePath, 0x00, MAX_PATH_LEN + 1);
    STRCPY(frameFilePath, filePath);

    CHK_STATUS(readFile(frameFilePath, TRUE, NULL, &fileSize));
    CHK_STATUS(readFile(frameFilePath, TRUE, params, &fileSize));

    tokenCount = jsmn_parse(&parser, (PCHAR)params, fileSize, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    if(tokenCount < 0) {
        DLOGE("JSMN Parse failed with error %d", tokenCount);
        retStatus = STATUS_INTERNAL_ERROR;
    }
    else {
        for (UINT32 i = 1; i < (UINT32) tokenCount; i++) {
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_RETENTION_PERIOD_HOURS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr);
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->retention));
                pCreateStreamInfoFromConfigFile->retention = pCreateStreamInfoFromConfigFile->retention * HUNDREDS_OF_NANOS_IN_AN_HOUR;
                DLOGD("Retention Period (hours) parsed: %lu hours",pCreateStreamInfoFromConfigFile->retention);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_TIMECODE_SCALE_MILLISECONDS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.timecodeScale);
                pCreateStreamInfoFromConfigFile->streamCaps.timecodeScale = pCreateStreamInfoFromConfigFile->streamCaps.timecodeScale * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
                DLOGD("Timecode scale (milliseconds) parsed: %lu milliseconds", pCreateStreamInfoFromConfigFile->streamCaps.timecodeScale);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_AVG_BANDWIDTH_BPS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI32(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.avgBandwidthBps);
                DLOGD("Average bandwidth parsed: %lu bps", pCreateStreamInfoFromConfigFile->streamCaps.avgBandwidthBps);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_FRAGMENT_DURATION_MILLISECONDS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.fragmentDuration);
                pCreateStreamInfoFromConfigFile->streamCaps.fragmentDuration = pCreateStreamInfoFromConfigFile->streamCaps.fragmentDuration * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
                DLOGD("Fragment duration (milliseconds) parsed: %lu milliseconds", pCreateStreamInfoFromConfigFile->streamCaps.fragmentDuration);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_STREAM_FRAMERATE")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI32(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.frameRate);
                DLOGD("Stream frame rate parsed: %lu fps", pCreateStreamInfoFromConfigFile->streamCaps.frameRate);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_MAX_LATENCY_SECONDS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.maxLatency);
                pCreateStreamInfoFromConfigFile->streamCaps.maxLatency = pCreateStreamInfoFromConfigFile->streamCaps.maxLatency * HUNDREDS_OF_NANOS_IN_A_SECOND;
                DLOGD("Max latency (seconds) parsed: %lu seconds", pCreateStreamInfoFromConfigFile->streamCaps.maxLatency);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_BUFFER_DURATION_SECONDS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.bufferDuration);
                pCreateStreamInfoFromConfigFile->streamCaps.bufferDuration = pCreateStreamInfoFromConfigFile->streamCaps.bufferDuration * HUNDREDS_OF_NANOS_IN_A_SECOND;
                DLOGD("Buffer Duration (seconds) parsed: %lu seconds", pCreateStreamInfoFromConfigFile->streamCaps.bufferDuration);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_REPLAY_DURATION_SECONDS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.replayDuration);
                pCreateStreamInfoFromConfigFile->streamCaps.replayDuration = pCreateStreamInfoFromConfigFile->streamCaps.replayDuration * HUNDREDS_OF_NANOS_IN_A_SECOND;
                DLOGD("Replay Duration (seconds) parsed: %lu seconds", pCreateStreamInfoFromConfigFile->streamCaps.replayDuration);
                i++;
            }
            if (compareJsonString((PCHAR)params, &tokens[i], JSMN_STRING, (PCHAR) "DEFAULT_CONNECTION_STALENESS_SECONDS")) {
                CHK_STATUS(getParsedValue(params, tokens[i+1], finalAttr));
                STRTOUI64(finalAttr, NULL, 10, &pCreateStreamInfoFromConfigFile->streamCaps.connectionStalenessDuration);
                pCreateStreamInfoFromConfigFile->streamCaps.connectionStalenessDuration = pCreateStreamInfoFromConfigFile->streamCaps.connectionStalenessDuration * HUNDREDS_OF_NANOS_IN_A_SECOND;
                DLOGD("Connection stale (seconds) parsed: %lu seconds", pCreateStreamInfoFromConfigFile->streamCaps.connectionStalenessDuration);
                i++;
            }
        }
    }
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS createAudioVideoStreamInfo(STREAMING_TYPE streamingType, PCHAR streamName,
                                  UINT64 retention, UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PStreamInfo pStreamInfo = NULL;
    PTrackInfo pTrackInfo = NULL;

    CHK(ppStreamInfo != NULL && streamName != NULL, STATUS_NULL_ARG);
    CHK(bufferDuration != 0, STATUS_INVALID_ARG);

    CHK(streamingType != STREAMING_TYPE_OFFLINE || retention != 0, STATUS_INVALID_ARG);

    // Allocate the entire structure
    pStreamInfo = (PStreamInfo) MEMCALLOC(1, SIZEOF(StreamInfo) + VIDEO_WITH_AUDIO_TRACK_COUNT * SIZEOF(TrackInfo));
    CHK(pStreamInfo != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pTrackInfo = (PTrackInfo) (pStreamInfo + 1);

    CHK_STATUS(createH264VideoTrackInfo(pTrackInfo));
    CHK_STATUS(createAacAudioTrackInfo(pTrackInfo + 1));

    STRCPY(pStreamInfo->name, streamName);
    STRCPY(pStreamInfo->streamCaps.contentType, MKV_H264_AAC_MULTI_CONTENT_TYPE);
    CHK_STATUS(setStreamInfoDefaults(streamingType, retention, bufferDuration, VIDEO_WITH_AUDIO_TRACK_COUNT,
                                     pStreamInfo, pTrackInfo));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeStreamInfoProvider(&pStreamInfo);
        pStreamInfo = NULL;
    }

    if (pStreamInfo != NULL) {
        *ppStreamInfo = pStreamInfo;
    }
    LEAVES();
    return retStatus;
}

STATUS setStreamInfoBasedOnStorageSize(UINT32 storageSize, UINT64 avgBitrate, UINT32 totalStreamCount, PStreamInfo pStreamInfo)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pStreamInfo != NULL, STATUS_NULL_ARG);
    CHK(storageSize > 0 && avgBitrate > 0 && totalStreamCount > 0, STATUS_INVALID_ARG);

    pStreamInfo->streamCaps.bufferDuration = (UINT64) ((DOUBLE) storageSize * 8 / avgBitrate / totalStreamCount * PRODUCER_DEFRAGMENTATION_FACTOR) * HUNDREDS_OF_NANOS_IN_A_SECOND;
    pStreamInfo->streamCaps.replayDuration = (UINT64) (REPLAY_DURATION_FACTOR * ((DOUBLE) pStreamInfo->streamCaps.bufferDuration));
    pStreamInfo->streamCaps.maxLatency = (UINT64) (LATENCY_PRESSURE_FACTOR * ((DOUBLE) pStreamInfo->streamCaps.bufferDuration));

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

