/*
 *  Kinesis Video C Producer Default Stream Info Provider
 */
#define LOG_CLASS "StreamInfoProvider"
#include "Include_i.h"

// Creates video stream info for real time streaming mode
STATUS createRealtimeVideoStreamInfoProvider(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{
    return createVideoStreamInfo(STREAMING_TYPE_REALTIME, VIDEO_CODEC_ID_H264, streamName, retention, bufferDuration, ppStreamInfo);
}
// Creates video stream info for offline streaming mode
STATUS createOfflineVideoStreamInfoProvider(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{
    return createVideoStreamInfo(STREAMING_TYPE_OFFLINE, VIDEO_CODEC_ID_H264, streamName, retention, bufferDuration, ppStreamInfo);
}
// Creates audio video stream info for real time streaming mode
STATUS createRealtimeAudioVideoStreamInfoProvider(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{
    return createAudioVideoStreamInfo(STREAMING_TYPE_REALTIME, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, streamName, retention, bufferDuration,
                                      ppStreamInfo);
}

// Creates audio video stream info for offline streaming mode
STATUS createOfflineAudioVideoStreamInfoProvider(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, PStreamInfo* ppStreamInfo)
{
    return createAudioVideoStreamInfo(STREAMING_TYPE_OFFLINE, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, streamName, retention, bufferDuration,
                                      ppStreamInfo);
}

// Creates video stream info for real time streaming mode
STATUS createRealtimeVideoStreamInfoProviderWithCodecs(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, VIDEO_CODEC_ID videoCodecId,
                                                       PStreamInfo* ppStreamInfo)
{
    return createVideoStreamInfo(STREAMING_TYPE_REALTIME, videoCodecId, streamName, retention, bufferDuration, ppStreamInfo);
}
// Creates video stream info for offline streaming mode
STATUS createOfflineVideoStreamInfoProviderWithCodecs(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, VIDEO_CODEC_ID videoCodecId,
                                                      PStreamInfo* ppStreamInfo)
{
    return createVideoStreamInfo(STREAMING_TYPE_OFFLINE, videoCodecId, streamName, retention, bufferDuration, ppStreamInfo);
}
// Creates audio video stream info for real time streaming mode
STATUS createRealtimeAudioVideoStreamInfoProviderWithCodecs(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, VIDEO_CODEC_ID videoCodecId,
                                                            AUDIO_CODEC_ID audioCodecId, PStreamInfo* ppStreamInfo)
{
    return createAudioVideoStreamInfo(STREAMING_TYPE_REALTIME, videoCodecId, audioCodecId, streamName, retention, bufferDuration, ppStreamInfo);
}

// Creates audio video stream info for offline streaming mode
STATUS createOfflineAudioVideoStreamInfoProviderWithCodecs(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, VIDEO_CODEC_ID videoCodecId,
                                                           AUDIO_CODEC_ID audioCodecId, PStreamInfo* ppStreamInfo)
{
    return createAudioVideoStreamInfo(STREAMING_TYPE_OFFLINE, videoCodecId, audioCodecId, streamName, retention, bufferDuration, ppStreamInfo);
}

// Creates audio only stream info for real time streaming mode
STATUS createRealtimeAudioStreamInfoProviderWithCodecs(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, AUDIO_CODEC_ID audioCodecId,
                                                       PStreamInfo* ppStreamInfo)
{
    return createAudioStreamInfo(STREAMING_TYPE_REALTIME, audioCodecId, streamName, retention, bufferDuration, ppStreamInfo);
}

// Creates audio only stream info for offline streaming mode
STATUS createOfflineAudioStreamInfoProviderWithCodecs(PCHAR streamName, UINT64 retention, UINT64 bufferDuration, AUDIO_CODEC_ID audioCodecId,
                                                      PStreamInfo* ppStreamInfo)
{
    return createAudioStreamInfo(STREAMING_TYPE_OFFLINE, audioCodecId, streamName, retention, bufferDuration, ppStreamInfo);
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
// Creates track info for video with given codec and sets the content type as per the codec
STATUS createVideoTrackInfo(VIDEO_CODEC_ID videoCodecId, PCHAR contentType, PTrackInfo pTrackInfo)
{
    STATUS retStatus = STATUS_SUCCESS;

    pTrackInfo->trackId = DEFAULT_VIDEO_TRACK_ID;
    pTrackInfo->codecPrivateData = NULL;
    pTrackInfo->codecPrivateDataSize = 0;

    CHK(pTrackInfo != NULL && contentType != NULL, STATUS_NULL_ARG);

    switch (videoCodecId) {
        case VIDEO_CODEC_ID_H264:
            STRCPY(pTrackInfo->codecId, MKV_H264_AVC_CODEC_ID);
            STRCAT(contentType, MKV_H264_CONTENT_TYPE);
            break;
        case VIDEO_CODEC_ID_H265:
            STRCPY(pTrackInfo->codecId, MKV_H265_HEVC_CODEC_ID);
            STRCAT(contentType, MKV_H265_CONTENT_TYPE);
            break;
        default:
            STRCPY(pTrackInfo->codecId, MKV_H264_AVC_CODEC_ID);
            STRCAT(contentType, MKV_H265_CONTENT_TYPE);
    }
    STRCPY(pTrackInfo->trackName, DEFAULT_VIDEO_TRACK_NAME);
    pTrackInfo->trackType = MKV_TRACK_INFO_TYPE_VIDEO;

CleanUp:

    return retStatus;
}

// Creates track info for audio with given codec and sets the content type as per the codec
STATUS createAudioTrackInfo(AUDIO_CODEC_ID audioCodecId, PCHAR contentType, PTrackInfo pTrackInfo, UINT32 audioTrackId)
{
    STATUS retStatus = STATUS_SUCCESS;

    pTrackInfo->trackId = audioTrackId;
    pTrackInfo->codecPrivateData = NULL;
    pTrackInfo->codecPrivateDataSize = 0;

    CHK(pTrackInfo != NULL && contentType != NULL, STATUS_NULL_ARG);

    switch (audioCodecId) {
        case AUDIO_CODEC_ID_AAC:
            STRCPY(pTrackInfo->codecId, MKV_AAC_CODEC_ID);
            STRCAT(contentType, MKV_AAC_CONTENT_TYPE);
            break;
        case AUDIO_CODEC_ID_PCM_ALAW:
            STRCPY(pTrackInfo->codecId, MKV_PCM_CODEC_ID);
            STRCAT(contentType, MKV_ALAW_CONTENT_TYPE);
            break;
        case AUDIO_CODEC_ID_PCM_MULAW:
            STRCPY(pTrackInfo->codecId, MKV_PCM_CODEC_ID);
            STRCAT(contentType, MKV_MULAW_CONTENT_TYPE);
            break;
        default:
            STRCPY(pTrackInfo->codecId, MKV_AAC_CODEC_ID);
            STRCAT(contentType, MKV_AAC_CONTENT_TYPE);
    }
    STRCPY(pTrackInfo->trackName, DEFAULT_AUDIO_TRACK_NAME);
    pTrackInfo->trackType = MKV_TRACK_INFO_TYPE_AUDIO;

CleanUp:

    return retStatus;
}

STATUS setStreamInfoDefaults(STREAMING_TYPE streamingType, UINT64 retention, UINT64 bufferDuration, UINT32 trackCount, PStreamInfo pStreamInfo,
                             PTrackInfo pTrackInfo)
{
    STATUS retStatus = STATUS_SUCCESS;

    pStreamInfo->streamCaps.absoluteFragmentTimes = TRUE;
    pStreamInfo->streamCaps.adaptive = TRUE;
    pStreamInfo->streamCaps.avgBandwidthBps = DEFAULT_AVG_BANDWIDTH;
    pStreamInfo->streamCaps.bufferDuration = bufferDuration;
    pStreamInfo->streamCaps.connectionStalenessDuration = pStreamInfo->streamCaps.connectionStalenessDuration =
        STREAM_INFO_DEFAULT_CONNECTION_STALE_DURATION;
    pStreamInfo->streamCaps.frameRate = DEFAULT_VIDEO_AUDIO_FRAME_RATE;
    pStreamInfo->streamCaps.fragmentDuration = STREAM_INFO_DEFAULT_FRAGMENT_DURATION;
    pStreamInfo->streamCaps.fragmentAcks = TRUE;
    pStreamInfo->streamCaps.frameTimecodes = TRUE;
    pStreamInfo->streamCaps.frameOrderingMode =
        trackCount == 1 ? FRAME_ORDER_MODE_PASS_THROUGH : FRAME_ORDERING_MODE_MULTI_TRACK_AV_COMPARE_PTS_ONE_MS_COMPENSATE_EOFR;
    pStreamInfo->streamCaps.keyFrameFragmentation = TRUE;
    pStreamInfo->streamCaps.maxLatency = (UINT64) (LATENCY_PRESSURE_FACTOR * ((DOUBLE) bufferDuration));
    pStreamInfo->streamCaps.nalAdaptationFlags = NAL_ADAPTATION_ANNEXB_CPD_NALS | NAL_ADAPTATION_ANNEXB_NALS;
    pStreamInfo->streamCaps.recalculateMetrics = TRUE;
    pStreamInfo->streamCaps.recoverOnError = TRUE;
    pStreamInfo->streamCaps.replayDuration = (UINT64) (REPLAY_DURATION_FACTOR * ((DOUBLE) bufferDuration));
    pStreamInfo->streamCaps.streamingType = streamingType;
    pStreamInfo->streamCaps.segmentUuid = NULL;
    pStreamInfo->streamCaps.timecodeScale = STREAM_INFO_DEFAULT_TIMESCALE;
    pStreamInfo->streamCaps.trackInfoCount = trackCount;
    pStreamInfo->streamCaps.trackInfoList = pTrackInfo;
    pStreamInfo->streamCaps.allowStreamCreation = TRUE;
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

STATUS createVideoStreamInfo(STREAMING_TYPE streamingType, VIDEO_CODEC_ID videoCodecId, PCHAR streamName, UINT64 retention, UINT64 bufferDuration,
                             PStreamInfo* ppStreamInfo)
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

    CHK(pTrackInfo != NULL && pStreamInfo->streamCaps.contentType != NULL, STATUS_NULL_ARG);

    CHK_STATUS(createVideoTrackInfo(videoCodecId, pStreamInfo->streamCaps.contentType, pTrackInfo));

    STRCPY(pStreamInfo->name, streamName);
    CHK_STATUS(setStreamInfoDefaults(streamingType, retention, bufferDuration, VIDEO_ONLY_TRACK_COUNT, pStreamInfo, pTrackInfo));

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

STATUS createAudioVideoStreamInfo(STREAMING_TYPE streamingType, VIDEO_CODEC_ID videoCodecId, AUDIO_CODEC_ID audioCodecId, PCHAR streamName,
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

    CHK(pTrackInfo != NULL && pStreamInfo->streamCaps.contentType != NULL, STATUS_NULL_ARG);

    CHK_STATUS(createVideoTrackInfo(videoCodecId, pStreamInfo->streamCaps.contentType, pTrackInfo));
    STRCAT(pStreamInfo->streamCaps.contentType, ","); // concatenating audio content type to video content type
    CHK_STATUS(createAudioTrackInfo(audioCodecId, pStreamInfo->streamCaps.contentType, pTrackInfo + 1, DEFAULT_AUDIO_TRACK_ID));

    STRCPY(pStreamInfo->name, streamName);
    CHK_STATUS(setStreamInfoDefaults(streamingType, retention, bufferDuration, VIDEO_WITH_AUDIO_TRACK_COUNT, pStreamInfo, pTrackInfo));

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

STATUS createAudioStreamInfo(STREAMING_TYPE streamingType, AUDIO_CODEC_ID audioCodecId, PCHAR streamName, UINT64 retention, UINT64 bufferDuration,
                             PStreamInfo* ppStreamInfo)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PStreamInfo pStreamInfo = NULL;
    PTrackInfo pTrackInfo = NULL;

    CHK(ppStreamInfo != NULL && streamName != NULL, STATUS_NULL_ARG);
    CHK(bufferDuration != 0, STATUS_INVALID_ARG);

    CHK(streamingType != STREAMING_TYPE_OFFLINE || retention != 0, STATUS_INVALID_ARG);

    // Allocate the entire structure
    pStreamInfo = (PStreamInfo) MEMCALLOC(1, SIZEOF(StreamInfo) + AUDIO_ONLY_TRACK_COUNT * SIZEOF(TrackInfo));
    CHK(pStreamInfo != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pTrackInfo = (PTrackInfo) (pStreamInfo + 1);

    CHK(pTrackInfo != NULL && pStreamInfo->streamCaps.contentType != NULL, STATUS_NULL_ARG);

    CHK_STATUS(createAudioTrackInfo(audioCodecId, pStreamInfo->streamCaps.contentType, pTrackInfo, DEFAULT_AUDIO_ONLY_TRACK_ID));

    STRCPY(pStreamInfo->name, streamName);
    CHK_STATUS(setStreamInfoDefaults(streamingType, retention, bufferDuration, AUDIO_ONLY_TRACK_COUNT, pStreamInfo, pTrackInfo));

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

    pStreamInfo->streamCaps.bufferDuration =
        (UINT64) ((DOUBLE) storageSize * 8 / avgBitrate / totalStreamCount * PRODUCER_DEFRAGMENTATION_FACTOR) * HUNDREDS_OF_NANOS_IN_A_SECOND;
    pStreamInfo->streamCaps.replayDuration = (UINT64) (REPLAY_DURATION_FACTOR * ((DOUBLE) pStreamInfo->streamCaps.bufferDuration));
    pStreamInfo->streamCaps.maxLatency = (UINT64) (LATENCY_PRESSURE_FACTOR * ((DOUBLE) pStreamInfo->streamCaps.bufferDuration));

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}
