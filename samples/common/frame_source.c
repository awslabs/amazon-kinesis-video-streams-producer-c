/**
 * frame_source.c — Media pipeline stand-in for the step-based samples.
 *
 * Loads pre-encoded frame files from disk so the samples can stream without a
 * camera or encoder. This file is NOT a teaching concept: in your application,
 * frames come from your capture/encode pipeline and this file disappears.
 * Everything that IS a teaching concept (auth, callbacks, stream config, the
 * putFrame loop) lives in steps/.
 */
#include "sample_common.h"

static STATUS loadFrameFiles(PCHAR pathFormat, PCHAR sampleDir, PFrameData frames, UINT32 frameCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT64 fileSize = 0;
    UINT32 i;

    for (i = 0; i < frameCount; ++i) {
        SNPRINTF(filePath, MAX_PATH_LEN, pathFormat, sampleDir, i + 1);
        CHK_STATUS(readFile(filePath, TRUE, NULL, &fileSize));
        frames[i].buffer = (PBYTE) MEMALLOC(fileSize);
        CHK(frames[i].buffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
        frames[i].size = (UINT32) fileSize;
        CHK_STATUS(readFile(filePath, TRUE, frames[i].buffer, &fileSize));
    }

CleanUp:
    return retStatus;
}

STATUS loadVideoFrames(PCHAR sampleDir, PFrameSource pFrameSource)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(sampleDir != NULL && pFrameSource != NULL, STATUS_NULL_ARG);

    printf("Loading video frames...\n");
    pFrameSource->videoFrameCount = SAMPLE_NUMBER_OF_VIDEO_FRAME_FILES;
    CHK_STATUS(loadFrameFiles((PCHAR) "%s/h264SampleFrames/frame-%03d.h264", sampleDir, pFrameSource->videoFrames,
                              pFrameSource->videoFrameCount));
    printf("Done loading video frames.\n");

CleanUp:
    return retStatus;
}

STATUS loadAudioVideoFrames(PCHAR sampleDir, PFrameSource pFrameSource)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_STATUS(loadVideoFrames(sampleDir, pFrameSource));

    printf("Loading audio frames...\n");
    pFrameSource->audioFrameCount = SAMPLE_NUMBER_OF_AUDIO_FRAME_FILES;
    CHK_STATUS(loadFrameFiles((PCHAR) "%s/aacSampleFrames/sample-%03d.aac", sampleDir, pFrameSource->audioFrames,
                              pFrameSource->audioFrameCount));
    printf("Done loading audio frames.\n");

CleanUp:
    return retStatus;
}

VOID freeFrameSource(PFrameSource pFrameSource)
{
    UINT32 i;

    if (pFrameSource == NULL) {
        return;
    }

    for (i = 0; i < SAMPLE_NUMBER_OF_VIDEO_FRAME_FILES; ++i) {
        SAFE_MEMFREE(pFrameSource->videoFrames[i].buffer);
    }
    for (i = 0; i < SAMPLE_NUMBER_OF_AUDIO_FRAME_FILES; ++i) {
        SAFE_MEMFREE(pFrameSource->audioFrames[i].buffer);
    }
    pFrameSource->videoFrameCount = 0;
    pFrameSource->audioFrameCount = 0;
}
