/**
 * sample_common.h — Shared helpers for the step-based Producer-C samples.
 *
 * This header provides the pieces every entry file needs but that are NOT
 * teaching concepts themselves:
 *   - LOG_STEP: numbered-step logging so the console output mirrors the
 *     numbered steps in the entry file and the documentation.
 *   - Shared constants: retention, buffer duration, frame cadence. These are
 *     the "committed configuration point" shared by all samples; per-sample
 *     tuning belongs in the relevant step function, not here.
 *   - FrameSource: a stand-in for the customer's media pipeline. It loads
 *     pre-encoded frame files from disk so the samples run with no camera or
 *     encoder. In a real application, replace FrameSource with your capture
 *     and encode pipeline; everything else stays the same.
 *
 * Teaching concepts (auth, callbacks, stream info, frame loops) live in
 * steps/, one function per concept. See steps/steps.h.
 */
#ifndef __KVS_SAMPLE_COMMON_INCLUDE__
#define __KVS_SAMPLE_COMMON_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../Samples.h"

// ----------------------------------------------------------------------------
// Step logging. Every entry file numbers its steps 1-7; LOG_STEP makes the
// runtime output line up with the source and the docs.
// ----------------------------------------------------------------------------
#define LOG_STEP(...)                                                                                                                                \
    do {                                                                                                                                             \
        printf("\n========== Step ");                                                                                                                \
        printf(__VA_ARGS__);                                                                                                                         \
        printf(" ==========\n");                                                                                                                     \
    } while (FALSE)

// ----------------------------------------------------------------------------
// Shared constants — the committed configuration point for all step samples.
// ----------------------------------------------------------------------------
#define SAMPLE_RETENTION_PERIOD 2 * HUNDREDS_OF_NANOS_IN_AN_HOUR    // 2h retention on the stream
#define SAMPLE_BUFFER_DURATION  120 * HUNDREDS_OF_NANOS_IN_A_SECOND // 120s of content buffered in the producer
#define SAMPLE_STREAM_DURATION  20 * HUNDREDS_OF_NANOS_IN_A_SECOND  // default runtime when no duration argument is given

// Frame cadence of the bundled sample frame files. The H.264 files are encoded
// at 25 fps with a keyframe every 45 frames; the AAC files are 20 ms samples.
#define SAMPLE_FPS_VALUE           25
#define SAMPLE_KEY_FRAME_INTERVAL  45
#define SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / SAMPLE_FPS_VALUE)
#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// AAC sample frame parameters, used to generate the audio codec private data.
#define SAMPLE_AAC_SAMPLING_RATE  48000
#define SAMPLE_AAC_CHANNEL_CONFIG 2

// Bundled frame file counts (samples/h264SampleFrames, samples/aacSampleFrames).
#define SAMPLE_NUMBER_OF_VIDEO_FRAME_FILES 403
#define SAMPLE_NUMBER_OF_AUDIO_FRAME_FILES 582

// ----------------------------------------------------------------------------
// FrameSource — the media pipeline stand-in.
// ----------------------------------------------------------------------------
typedef struct {
    PBYTE buffer;
    UINT32 size;
} FrameData, *PFrameData;

typedef struct {
    FrameData videoFrames[SAMPLE_NUMBER_OF_VIDEO_FRAME_FILES];
    UINT32 videoFrameCount;
    FrameData audioFrames[SAMPLE_NUMBER_OF_AUDIO_FRAME_FILES];
    UINT32 audioFrameCount;
} FrameSource, *PFrameSource;

/**
 * Loads the bundled H.264 frame files into memory.
 *
 * @param sampleDir - Directory containing h264SampleFrames/ (usually ../samples)
 * @param pFrameSource - Caller-owned FrameSource to populate
 */
STATUS loadVideoFrames(PCHAR sampleDir, PFrameSource pFrameSource);

/**
 * Loads the bundled H.264 and AAC frame files into memory.
 *
 * @param sampleDir - Directory containing h264SampleFrames/ and aacSampleFrames/
 * @param pFrameSource - Caller-owned FrameSource to populate
 */
STATUS loadAudioVideoFrames(PCHAR sampleDir, PFrameSource pFrameSource);

/**
 * Frees all frame buffers held by the FrameSource. Safe to call on a
 * partially loaded or zeroed FrameSource.
 */
VOID freeFrameSource(PFrameSource pFrameSource);

#ifdef __cplusplus
}
#endif
#endif /* __KVS_SAMPLE_COMMON_INCLUDE__ */
