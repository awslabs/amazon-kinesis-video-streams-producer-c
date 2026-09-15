/**
 * step_stream_info.c — Step 2: Stream info.
 *
 * The StreamInfo commits the stream to a streaming type, codec(s), retention,
 * and buffering behavior. The two silent-failure traps this step guards
 * against:
 *
 *   - Streaming type: these functions configure STREAMING_TYPE_REALTIME, where
 *     fragment timestamps derive from the frame PTS the application generates
 *     against the wall clock. Uploading pre-recorded files with a realtime
 *     stream drops frames silently — that variant belongs in a separate
 *     configureOfflineStreamInfo().
 *
 *   - Codec private data (CPD): for multi-track streams, every track's CPD
 *     must be attached to the StreamInfo (or delivered in-band) before frames
 *     flow, otherwise the stream fails with an invalid-stream-state error.
 *     The audio CPD is generated here so the entry file never has to think
 *     about it. The CPD buffer must outlive stream creation, which is why it
 *     is static here rather than on the stack.
 */
#include "steps.h"

// CPD storage. The StreamInfo only holds a pointer to the CPD; the SDK copies
// it during stream creation. Static storage keeps the buffer valid without
// heap management. (One buffer per process — these samples create one stream.)
static BYTE gAacAudioCpd[KVS_AAC_CPD_SIZE_BYTE];

STATUS configureRealtimeVideoStreamInfo(PCHAR streamName, PStreamInfo* ppStreamInfo)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStreamInfo pStreamInfo = NULL;

    CHK(streamName != NULL && ppStreamInfo != NULL, STATUS_NULL_ARG);

    // Realtime, video-only, H.264. H.264 CPD (SPS/PPS) is delivered in-band
    // with the first keyframe, so nothing to attach here.
    CHK_STATUS(createRealtimeVideoStreamInfoProviderWithCodecs(streamName, SAMPLE_RETENTION_PERIOD, SAMPLE_BUFFER_DURATION, VIDEO_CODEC_ID_H264,
                                                               &pStreamInfo));

    // Relative time mode: frame timestamps start at 0 and the service assigns
    // ingest wall-clock times to fragments. Set TRUE if your application
    // generates epoch timestamps and playback must reflect capture time.
    pStreamInfo->streamCaps.absoluteFragmentTimes = FALSE;

    *ppStreamInfo = pStreamInfo;
    pStreamInfo = NULL;

CleanUp:
    if (pStreamInfo != NULL) {
        freeStreamInfoProvider(&pStreamInfo);
    }
    return retStatus;
}

STATUS configureAudioVideoStreamInfo(PCHAR streamName, PStreamInfo* ppStreamInfo)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStreamInfo pStreamInfo = NULL;
    PTrackInfo pAudioTrack = NULL;

    CHK(streamName != NULL && ppStreamInfo != NULL, STATUS_NULL_ARG);

    // Realtime, multi-track: H.264 video + AAC audio.
    CHK_STATUS(createRealtimeAudioVideoStreamInfoProviderWithCodecs(streamName, SAMPLE_RETENTION_PERIOD, SAMPLE_BUFFER_DURATION, VIDEO_CODEC_ID_H264,
                                                                    AUDIO_CODEC_ID_AAC, &pStreamInfo));

    // Attach the AAC codec private data. Track order in trackInfoList is not
    // guaranteed, so locate the audio track by its track ID.
    pAudioTrack = pStreamInfo->streamCaps.trackInfoList[0].trackId == DEFAULT_AUDIO_TRACK_ID ? &pStreamInfo->streamCaps.trackInfoList[0]
                                                                                             : &pStreamInfo->streamCaps.trackInfoList[1];
    pAudioTrack->codecPrivateData = gAacAudioCpd;
    pAudioTrack->codecPrivateDataSize = KVS_AAC_CPD_SIZE_BYTE;
    CHK_STATUS(mkvgenGenerateAacCpd(AAC_LC, SAMPLE_AAC_SAMPLING_RATE, SAMPLE_AAC_CHANNEL_CONFIG, pAudioTrack->codecPrivateData,
                                    pAudioTrack->codecPrivateDataSize));

    // Relative time mode — see configureRealtimeVideoStreamInfo above.
    pStreamInfo->streamCaps.absoluteFragmentTimes = FALSE;

    *ppStreamInfo = pStreamInfo;
    pStreamInfo = NULL;

CleanUp:
    if (pStreamInfo != NULL) {
        freeStreamInfoProvider(&pStreamInfo);
    }
    return retStatus;
}
