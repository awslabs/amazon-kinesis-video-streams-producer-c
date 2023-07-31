/*******************************************
Auth internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_VERSION_INCLUDE_I__
#define __KINESIS_VIDEO_VERSION_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef VERSION_STRING
#define AWS_SDK_KVS_PRODUCER_VERSION_STRING (PCHAR) VERSION_STRING
#else
#define AWS_SDK_KVS_PRODUCER_VERSION_STRING (PCHAR) "UNKNOWN"
#endif

/**
 * Default user agent string
 */
#define USER_AGENT_NAME (PCHAR) "AWS-PRODUCER-SDK-KVS"

////////////////////////////////////////////////////
// Function definitions
////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_VERSION_INCLUDE_I__ */
