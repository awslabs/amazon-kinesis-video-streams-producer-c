#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class InfoProviderApiTest : public ProducerClientTestBase {
};

TEST_F(InfoProviderApiTest, CreateDefaultDeviceInfo_Returns_Success)
{
    PDeviceInfo pDeviceInfo;
    EXPECT_EQ(STATUS_SUCCESS, createDefaultDeviceInfo(&pDeviceInfo));
    EXPECT_EQ(STATUS_SUCCESS, setDeviceInfoStorageSize(pDeviceInfo, MAX_STORAGE_ALLOCATION_SIZE));
    EXPECT_EQ(STATUS_INVALID_STORAGE_SIZE, setDeviceInfoStorageSize(pDeviceInfo, MAX_STORAGE_ALLOCATION_SIZE + 1));
    EXPECT_TRUE(pDeviceInfo->clientInfo.loggerLogLevel == this->loggerLogLevel);
    // 500 Kbps with 2 sec duration
    EXPECT_EQ(STATUS_SUCCESS,setDeviceInfoStorageSizeBasedOnBitrateAndBufferDuration(pDeviceInfo, 500 * 1000 * 8, 2 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    // 500 Kbps with 10 sec duration
    EXPECT_EQ(STATUS_SUCCESS, setDeviceInfoStorageSizeBasedOnBitrateAndBufferDuration(pDeviceInfo, 500 * 1000 * 8, 10 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    // 1000 Kbps with 10 sec duration
    EXPECT_EQ(STATUS_SUCCESS, setDeviceInfoStorageSizeBasedOnBitrateAndBufferDuration(pDeviceInfo, 1000 * 1000 * 8, 10 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    EXPECT_EQ(STATUS_INVALID_ARG, setDeviceInfoStorageSizeBasedOnBitrateAndBufferDuration(pDeviceInfo, 0, 0 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    EXPECT_EQ(STATUS_NULL_ARG, setDeviceInfoStorageSizeBasedOnBitrateAndBufferDuration(NULL, 0, 0 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    EXPECT_EQ(STATUS_NULL_ARG, createDefaultDeviceInfo(NULL));
    EXPECT_EQ(STATUS_NULL_ARG, setDeviceInfoStorageSize(NULL, MAX_STORAGE_ALLOCATION_SIZE));
    EXPECT_EQ(STATUS_SUCCESS, freeDeviceInfo(&pDeviceInfo));
    EXPECT_EQ(NULL, pDeviceInfo);
    EXPECT_EQ(STATUS_SUCCESS, freeDeviceInfo(&pDeviceInfo));
    EXPECT_EQ(STATUS_NULL_ARG, freeDeviceInfo(NULL));
}

TEST_F(InfoProviderApiTest, CreateOfflineVideoStreamInfoProvider_Returns_ValidVideoStreamInfo)
{
    PStreamInfo pStreamInfo;
    EXPECT_EQ(STATUS_SUCCESS, createOfflineVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(FRAME_ORDER_MODE_PASS_THROUGH, pStreamInfo->streamCaps.frameOrderingMode);
    EXPECT_EQ(MKV_TRACK_INFO_TYPE_VIDEO, pStreamInfo->streamCaps.trackInfoList->trackType);
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_NE(0, pStreamInfo->retention);
    EXPECT_EQ(1, pStreamInfo->streamCaps.trackInfoCount);
    EXPECT_EQ(TRUE, pStreamInfo->streamCaps.absoluteFragmentTimes);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineVideoStreamInfoProvider(NULL, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, createOfflineVideoStreamInfoProvider(EMPTY_STRING, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_INVALID_ARG, createOfflineVideoStreamInfoProvider(TEST_STREAM_NAME, 0, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_INVALID_ARG, createOfflineVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, 0, &pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(NULL, pStreamInfo);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_NULL_ARG, freeStreamInfoProvider(NULL));
}

TEST_F(InfoProviderApiTest, CreateRealTimeVideoStreamInfoProvider_Returns_ValidVideoStreamInfo)
{
    PStreamInfo pStreamInfo;

    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(FRAME_ORDER_MODE_PASS_THROUGH, pStreamInfo->streamCaps.frameOrderingMode);
    EXPECT_EQ(1, pStreamInfo->streamCaps.trackInfoCount);
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_EQ(MKV_TRACK_INFO_TYPE_VIDEO, pStreamInfo->streamCaps.trackInfoList[0].trackType);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeVideoStreamInfoProvider(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProvider(EMPTY_STRING, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProvider(TEST_STREAM_NAME, 0, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(NULL, pStreamInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_NULL_ARG, freeStreamInfoProvider(NULL));

}

TEST_F(InfoProviderApiTest, CreateRealTimeAudioVideoStreamInfoProvider_Returns_ValidVideoStreamInfo)
{
    PStreamInfo pStreamInfo;

    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(FRAME_ORDERING_MODE_MULTI_TRACK_AV_COMPARE_PTS_ONE_MS_COMPENSATE_EOFR, pStreamInfo->streamCaps.frameOrderingMode);
    EXPECT_EQ(2, pStreamInfo->streamCaps.trackInfoCount);
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_EQ(MKV_TRACK_INFO_TYPE_VIDEO, pStreamInfo->streamCaps.trackInfoList[0].trackType);
    EXPECT_EQ(MKV_TRACK_INFO_TYPE_AUDIO, pStreamInfo->streamCaps.trackInfoList[1].trackType);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, freeStreamInfoProvider(NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeAudioVideoStreamInfoProvider(NULL, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProvider(EMPTY_STRING, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProvider(TEST_STREAM_NAME, 0, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeAudioVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_NULL_ARG, freeStreamInfoProvider(NULL));
}

TEST_F(InfoProviderApiTest, setStreamInfoBasedOnStorageSizeApiTest)
{
    PStreamInfo pStreamInfo;

    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_INVALID_ARG, setStreamInfoBasedOnStorageSize(0, 1000000, 1, pStreamInfo));
    EXPECT_EQ(STATUS_INVALID_ARG, setStreamInfoBasedOnStorageSize(2 * 1024 * 1024, 0, 1, pStreamInfo));
    EXPECT_EQ(STATUS_INVALID_ARG, setStreamInfoBasedOnStorageSize(2 * 1024 * 1024, 1000000, 0, pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, setStreamInfoBasedOnStorageSize(2 * 1024 * 1024, 1000000, 1, NULL));

    EXPECT_EQ(STATUS_SUCCESS, setStreamInfoBasedOnStorageSize(2 * 1024 * 1024, 1000000, 1, pStreamInfo));
    EXPECT_TRUE(pStreamInfo->streamCaps.bufferDuration != TEST_STREAM_BUFFER_DURATION);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
}

TEST_F(InfoProviderApiTest, CreateOfflineAudioVideoStreamInfoProvider_Returns_ValidVideoStreamInfo)
{
    PStreamInfo pStreamInfo;

    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(FRAME_ORDERING_MODE_MULTI_TRACK_AV_COMPARE_PTS_ONE_MS_COMPENSATE_EOFR, pStreamInfo->streamCaps.frameOrderingMode);
    EXPECT_EQ(2, pStreamInfo->streamCaps.trackInfoCount);
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_EQ(MKV_TRACK_INFO_TYPE_VIDEO, pStreamInfo->streamCaps.trackInfoList[0].trackType);
    EXPECT_EQ(MKV_TRACK_INFO_TYPE_AUDIO, pStreamInfo->streamCaps.trackInfoList[1].trackType);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_NULL_ARG, createOfflineAudioVideoStreamInfoProvider(NULL, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProvider(EMPTY_STRING, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_INVALID_ARG, createOfflineAudioVideoStreamInfoProvider(TEST_STREAM_NAME, 0, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(STATUS_NULL_ARG, createOfflineAudioVideoStreamInfoProvider(TEST_STREAM_NAME, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));

    EXPECT_EQ(NULL, pStreamInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_NULL_ARG, freeStreamInfoProvider(NULL));
}

TEST_F(InfoProviderApiTest, CreateStream_Returns_Success)
{
    PStreamCallbacks pStreamCallbacks;
    EXPECT_EQ(STATUS_SUCCESS, createStreamCallbacks(&pStreamCallbacks));
    EXPECT_EQ(STATUS_SUCCESS, freeStreamCallbacks(&pStreamCallbacks));
    EXPECT_EQ(NULL, pStreamCallbacks);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamCallbacks(&pStreamCallbacks));
    EXPECT_EQ(STATUS_NULL_ARG, createStreamCallbacks(NULL));
    }

    TEST_F(InfoProviderApiTest, createRealtimeVideoStreamInfoProviderWithCodecsApiTest)
    {
    PStreamInfo pStreamInfo;
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeVideoStreamInfoProviderWithCodecs(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, NULL));

    // H264
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_H264_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // H265
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_H265_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // default
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, (VIDEO_CODEC_ID) 0xffff, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_H264_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    }

    TEST_F(InfoProviderApiTest, createOfflineVideoStreamInfoProviderWithCodecsApiTest)
    {
    PStreamInfo pStreamInfo;
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineVideoStreamInfoProviderWithCodecs(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, NULL));

    // H264
    EXPECT_EQ(STATUS_SUCCESS, createOfflineVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_H264_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // H265
    EXPECT_EQ(STATUS_SUCCESS, createOfflineVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_H265_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // default
    EXPECT_EQ(STATUS_SUCCESS, createOfflineVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, (VIDEO_CODEC_ID) 0xffff, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_H264_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
}

TEST_F(InfoProviderApiTest, createRealtimeAudioVideoStreamInfoProviderWithCodecsApiTest)
{
    PStreamInfo pStreamInfo;
    CHAR expectedContentType[MAX_CONTENT_TYPE_LEN + 1] = {"\0"};
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeAudioVideoStreamInfoProviderWithCodecs(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, NULL));

    // H264, AAC
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_AAC_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H264, PCM ALAW
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_ALAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H264, MULAW
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_PCM_MULAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_MULAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);


    // H265, AAC
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, AUDIO_CODEC_ID_AAC, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H265_CONTENT_TYPE, MKV_AAC_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H265, PCM ALAW
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H265_CONTENT_TYPE, MKV_ALAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H265, MULAW
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, AUDIO_CODEC_ID_PCM_MULAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H265_CONTENT_TYPE, MKV_MULAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // Default
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, (VIDEO_CODEC_ID) 0xffff, (AUDIO_CODEC_ID) 0xffff, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_AAC_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

}

TEST_F(InfoProviderApiTest, createOfflineAudioVideoStreamInfoProviderWithCodecsApiTest)
{
    PStreamInfo pStreamInfo;
    CHAR expectedContentType[MAX_CONTENT_TYPE_LEN + 1] = {"\0"};
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineAudioVideoStreamInfoProviderWithCodecs(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, NULL));

    // H264, AAC
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_AAC, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_AAC_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H264, PCM ALAW
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_ALAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H264, MULAW
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H264, AUDIO_CODEC_ID_PCM_MULAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_MULAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);


    // H265, AAC
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, AUDIO_CODEC_ID_AAC, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H265_CONTENT_TYPE, MKV_AAC_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H265, PCM ALAW
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H265_CONTENT_TYPE, MKV_ALAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
    MEMSET(expectedContentType, '\0', SIZEOF(expectedContentType));

    // H265, MULAW
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, VIDEO_CODEC_ID_H265, AUDIO_CODEC_ID_PCM_MULAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H265_CONTENT_TYPE, MKV_MULAW_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // Default
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioVideoStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, (VIDEO_CODEC_ID) 0xffff, (AUDIO_CODEC_ID) 0xffff, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    SNPRINTF(expectedContentType, SIZEOF(expectedContentType), "%s,%s", MKV_H264_CONTENT_TYPE, MKV_AAC_CONTENT_TYPE);
    EXPECT_STREQ(expectedContentType, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
}

TEST_F(InfoProviderApiTest, createRealtimeAudioStreamInfoProviderWithCodecsApiTest)
{
    PStreamInfo pStreamInfo;
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_ALAW, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRealtimeAudioStreamInfoProviderWithCodecs(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_ALAW, NULL));

    // AAC
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_AAC, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_AAC_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // PCM A LAW
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_ALAW_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // PCM MULAW
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_MULAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_MULAW_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // default
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, (AUDIO_CODEC_ID) 0xffff, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_REALTIME, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_AAC_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
}

TEST_F(InfoProviderApiTest, createOfflineAudioStreamInfoProviderWithCodecsApiTest)
{
    PStreamInfo pStreamInfo;
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_ALAW, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createOfflineAudioStreamInfoProviderWithCodecs(NULL, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_ALAW, NULL));

    // AAC
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_AAC, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_AAC_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // PCM A LAW
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_ALAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_ALAW_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // PCM MULAW
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, AUDIO_CODEC_ID_PCM_MULAW, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_MULAW_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);

    // default
    EXPECT_EQ(STATUS_SUCCESS, createOfflineAudioStreamInfoProviderWithCodecs(TEST_STREAM_NAME, TEST_RETENTION_PERIOD,TEST_STREAM_BUFFER_DURATION, (AUDIO_CODEC_ID) 0xffff, &pStreamInfo));
    EXPECT_EQ(STREAMING_TYPE_OFFLINE, pStreamInfo->streamCaps.streamingType);
    EXPECT_STREQ(MKV_AAC_CONTENT_TYPE, pStreamInfo->streamCaps.contentType);
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(NULL, pStreamInfo);
}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;
