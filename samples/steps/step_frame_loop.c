/**
 * step_frame_loop.c — Step 6: Frame loop.
 *
 * The putFrame loop is where the application's media pipeline meets the SDK.
 * In realtime mode, fragment timestamps derive from the frame PTS the
 * application generates, so the loop must pace putFrame calls against the
 * wall clock to match the declared frame durations.
 *
 * What these loops teach:
 *   - Timestamp generation: PTS/DTS advance by exactly one frame duration per
 *     frame, and the loop sleeps until the wall clock catches up. Bursting
 *     frames faster than realtime distorts fragment timing.
 *   - Keyframe cadence: the video track cuts fragments — a frame flagged
 *     FRAME_FLAG_KEY_FRAME starts a new fragment. Audio frames never carry
 *     the keyframe flag.
 *   - Multi-track ordering: no audio frame may be put before the first video
 *     frame; the first fragment boundary is defined by the first video
 *     keyframe. The audio thread spins on an atomic flag until the video
 *     thread has put its first frame.
 *   - putFrame error handling: an individual putFrame failure is logged and
 *     skipped, NOT treated as fatal. Recovery from stream-level errors is the
 *     job of the stream callbacks (step 4), not the frame loop.
 */
#include "steps.h"

typedef struct {
    volatile ATOMIC_BOOL firstVideoFramePut;
    STREAM_HANDLE streamHandle;
    PFrameSource pFrameSource;
    UINT64 streamStartTime;
    UINT64 streamStopTime;
} FrameLoopContext, *PFrameLoopContext;

static PVOID putVideoFrameRoutine(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PFrameLoopContext pCtx = (PFrameLoopContext) args;
    Frame frame;
    UINT32 fileIndex = 0;
    STATUS status;
    UINT64 runningTime;

    CHK(pCtx != NULL, STATUS_NULL_ARG);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;
    frame.decodingTs = 0;     // Relative time mode: timestamps start at 0
    frame.presentationTs = 0;
    frame.index = 0;
    frame.frameData = pCtx->pFrameSource->videoFrames[fileIndex].buffer;
    frame.size = pCtx->pFrameSource->videoFrames[fileIndex].size;
    // The video track cuts fragments: every FRAME_FLAG_KEY_FRAME starts a new one.
    frame.flags = fileIndex % SAMPLE_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;

    while (GETTIME() < pCtx->streamStopTime) {
        status = putKinesisVideoFrame(pCtx->streamHandle, &frame);

        // Unblocks the audio thread (see putAudioFrameRoutine).
        ATOMIC_STORE_BOOL(&pCtx->firstVideoFramePut, TRUE);

        if (STATUS_FAILED(status)) {
            // Log and continue. Stream-level recovery belongs to the stream
            // callbacks attached at step 4, not here.
            printf("putKinesisVideoFrame for video failed with 0x%08x\n", status);
            status = STATUS_SUCCESS;
        }

        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;
        frame.decodingTs = frame.presentationTs;
        frame.index++;

        fileIndex = (fileIndex + 1) % pCtx->pFrameSource->videoFrameCount;
        frame.flags = fileIndex % SAMPLE_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        frame.frameData = pCtx->pFrameSource->videoFrames[fileIndex].buffer;
        frame.size = pCtx->pFrameSource->videoFrames[fileIndex].size;

        // Pace against the wall clock so PTS tracks real time.
        runningTime = GETTIME() - pCtx->streamStartTime;
        if (runningTime < frame.presentationTs) {
            THREAD_SLEEP(frame.presentationTs - runningTime);
        }
    }

CleanUp:
    if (retStatus != STATUS_SUCCESS) {
        printf("putVideoFrameRoutine failed with 0x%08x\n", retStatus);
    }
    return (PVOID) (ULONG_PTR) retStatus;
}

static PVOID putAudioFrameRoutine(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PFrameLoopContext pCtx = (PFrameLoopContext) args;
    Frame frame;
    UINT32 fileIndex = 0;
    STATUS status;
    UINT64 runningTime;

    CHK(pCtx != NULL, STATUS_NULL_ARG);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.decodingTs = 0;
    frame.presentationTs = 0;
    frame.index = 0;
    frame.frameData = pCtx->pFrameSource->audioFrames[fileIndex].buffer;
    frame.size = pCtx->pFrameSource->audioFrames[fileIndex].size;
    frame.flags = FRAME_FLAG_NONE; // The audio track never cuts fragments

    while (GETTIME() < pCtx->streamStopTime) {
        // Multi-track barrier: no audio until the first video frame is in.
        if (ATOMIC_LOAD_BOOL(&pCtx->firstVideoFramePut)) {
            status = putKinesisVideoFrame(pCtx->streamHandle, &frame);
            if (STATUS_FAILED(status)) {
                printf("putKinesisVideoFrame for audio failed with 0x%08x\n", status);
                status = STATUS_SUCCESS;
            }

            frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;
            frame.decodingTs = frame.presentationTs;
            frame.index++;

            fileIndex = (fileIndex + 1) % pCtx->pFrameSource->audioFrameCount;
            frame.frameData = pCtx->pFrameSource->audioFrames[fileIndex].buffer;
            frame.size = pCtx->pFrameSource->audioFrames[fileIndex].size;

            runningTime = GETTIME() - pCtx->streamStartTime;
            if (runningTime < frame.presentationTs) {
                THREAD_SLEEP(frame.presentationTs - runningTime);
            }
        }
    }

CleanUp:
    if (retStatus != STATUS_SUCCESS) {
        printf("putAudioFrameRoutine failed with 0x%08x\n", retStatus);
    }
    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS runRealtimeVideoFrameLoop(STREAM_HANDLE streamHandle, PFrameSource pFrameSource, UINT64 streamingDuration)
{
    STATUS retStatus = STATUS_SUCCESS;
    FrameLoopContext ctx;
    TID videoSendTid;

    CHK(IS_VALID_STREAM_HANDLE(streamHandle) && pFrameSource != NULL && pFrameSource->videoFrameCount > 0, STATUS_INVALID_ARG);

    MEMSET(&ctx, 0x00, SIZEOF(FrameLoopContext));
    ctx.streamHandle = streamHandle;
    ctx.pFrameSource = pFrameSource;
    ctx.streamStartTime = GETTIME();
    ctx.streamStopTime = ctx.streamStartTime + streamingDuration;

    CHK_STATUS(THREAD_CREATE(&videoSendTid, putVideoFrameRoutine, (PVOID) &ctx));
    CHK_STATUS(THREAD_JOIN(videoSendTid, NULL));

CleanUp:
    return retStatus;
}

STATUS runAudioVideoFrameLoop(STREAM_HANDLE streamHandle, PFrameSource pFrameSource, UINT64 streamingDuration)
{
    STATUS retStatus = STATUS_SUCCESS;
    FrameLoopContext ctx;
    TID videoSendTid, audioSendTid;

    CHK(IS_VALID_STREAM_HANDLE(streamHandle) && pFrameSource != NULL && pFrameSource->videoFrameCount > 0 && pFrameSource->audioFrameCount > 0,
        STATUS_INVALID_ARG);

    MEMSET(&ctx, 0x00, SIZEOF(FrameLoopContext));
    ctx.streamHandle = streamHandle;
    ctx.pFrameSource = pFrameSource;
    ctx.streamStartTime = GETTIME();
    ctx.streamStopTime = ctx.streamStartTime + streamingDuration;
    ATOMIC_STORE_BOOL(&ctx.firstVideoFramePut, FALSE);

    // One thread per track; both share the pacing clock in ctx.
    CHK_STATUS(THREAD_CREATE(&videoSendTid, putVideoFrameRoutine, (PVOID) &ctx));
    CHK_STATUS(THREAD_CREATE(&audioSendTid, putAudioFrameRoutine, (PVOID) &ctx));

    CHK_STATUS(THREAD_JOIN(videoSendTid, NULL));
    CHK_STATUS(THREAD_JOIN(audioSendTid, NULL));

CleanUp:
    return retStatus;
}
