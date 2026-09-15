/**
 * step_device_info.c — Step 1: Device info.
 *
 * The DeviceInfo controls process-wide producer settings. The two that matter
 * most in production are:
 *
 *   - storageInfo.storageSize: the content store shared by ALL streams on this
 *     client. It must hold buffer-duration's worth of content for every stream
 *     at its bitrate, or the producer reports storage overflow and drops tail
 *     fragments during outages. The default is 128 MB.
 *
 *   - clientInfo.loggerLogLevel: leave verbose logging OFF in production.
 *     Metric and state logging at LOG_LEVEL_VERBOSE can emit dozens of lines
 *     per second and drown out the one line you need during an incident.
 *
 * Variants for constrained devices (smaller store, tuned timeouts) belong here
 * as additional functions, e.g. configureLowMemoryDeviceInfo().
 */
#include "steps.h"

STATUS configureDefaultDeviceInfo(PDeviceInfo* ppDeviceInfo)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDeviceInfo pDeviceInfo = NULL;

    CHK(ppDeviceInfo != NULL, STATUS_NULL_ARG);

    // Default storage size is 128 MB. Use setDeviceInfoStorageSize() after
    // create to change it — size it as sum over streams of
    // (bitrate * bufferDuration / 8), with headroom.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));

    // Log level comes from the AWS_KVS_LOG_LEVEL environment variable
    // (1=verbose .. 5=silent); defaults to DEBUG for the samples.
    pDeviceInfo->clientInfo.loggerLogLevel = getSampleLogLevel();

    *ppDeviceInfo = pDeviceInfo;
    pDeviceInfo = NULL;

CleanUp:
    if (pDeviceInfo != NULL) {
        freeDeviceInfo(&pDeviceInfo);
    }
    return retStatus;
}
