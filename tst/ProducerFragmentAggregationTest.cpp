#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class ProducerFragmentAggregationTest : public ProducerClientTestBase {
};

extern ProducerClientTestBase* gProducerClientTestBase;

TEST_F(ProducerFragmentAggregationTest, fragment_aggregation_recovery)
{
    UINT32 j;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    UINT32 totalFragments = 2;
    UINT32 totalFrames = totalFragments * TEST_FPS;
    UINT64 bufferDuration = 10 * 24 * HUNDREDS_OF_NANOS_IN_AN_HOUR; // 10 days
    UINT64 maxLatency = bufferDuration - 1 * 24 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    StreamMetrics streamMetrics;

    // Set the right parameters in order to cause fragment aggregation
    mDeviceInfo.storageInfo.spillRatio = 5; // 5% will be in-memory store and the rest 95% of the heap will be file backed
    mDeviceInfo.storageInfo.storageSize = MIN_STORAGE_SIZE_FOR_FRAGMENT_ACCUMULATOR + 1;
    mDeviceInfo.storageInfo.storageType = DEVICE_STORAGE_TYPE_HYBRID_FILE;

    createDefaultProducerClient();

    // don't care about the connection staleness for now
    mStreamInfo.streamCaps.connectionStalenessDuration = bufferDuration;
    EXPECT_EQ(STATUS_SUCCESS, createTestStream(0,
                                               STREAMING_TYPE_REALTIME,
                                               maxLatency,
                                               bufferDuration));
    streamHandle = mStreams[0];
    EXPECT_TRUE(streamHandle != INVALID_STREAM_HANDLE_VALUE);

    // Put some frames at regular cadence and ensure those are streamed out OK
    for (j = 0; j < totalFrames; ++j) {
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(streamHandle, &mFrame));
        updateFrame();
        THREAD_SLEEP(mFrame.duration);
    }

    // Pause to ensure the data is properly streamed out
    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Now, produce frames at fast rate to cause buffer filling and eventual fragment aggregation
    // We will choose twice the min required duration for the aggregation
    totalFrames = (2 * MIN_CONTENT_DURATION_FOR_FRAGMENT_ACCUMULATOR) / HUNDREDS_OF_NANOS_IN_A_SECOND * TEST_FPS;
    for (j = 0; j < totalFrames; ++j) {
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(streamHandle, &mFrame));
        updateFrame();
    }

    // Loop and await for the stream to catch up
    MEMSET(&streamMetrics, 0x00, SIZEOF(StreamMetrics));
    streamMetrics.version = STREAM_METRICS_CURRENT_VERSION;
    do {
        EXPECT_EQ(STATUS_SUCCESS, getKinesisVideoStreamMetrics(streamHandle, &streamMetrics));
    } while (streamMetrics.overallViewDuration > 10 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Continue producing at a regular cadence
    totalFrames = totalFragments * TEST_FPS;
    for (j = 0; j < totalFrames; ++j) {
        EXPECT_EQ(STATUS_SUCCESS, putKinesisVideoFrame(streamHandle, &mFrame));
        updateFrame();
        THREAD_SLEEP(mFrame.duration);
    }

    EXPECT_EQ(STATUS_SUCCESS, stopKinesisVideoStreamSync(streamHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&streamHandle));
    EXPECT_EQ(0, mStreamErrorFnCount);

    mStreams[0] = INVALID_STREAM_HANDLE_VALUE;
}

}
}
}
}
