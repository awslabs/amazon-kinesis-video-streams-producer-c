#ifndef __KINESIS_VIDEO_EC2_AUTH_CALLBACKS_INCLUDE_I__
#define __KINESIS_VIDEO_EC2_AUTH_CALLBACKS_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward declarations
 */
struct __CallbacksProvider;

typedef struct __Ec2AuthCallbacks Ec2AuthCallbacks;

struct __Ec2AuthCallbacks {
    // First member should be the Auth callbacks
    AuthCallbacks authCallbacks;

    // Pointer to EC2 Credential Provider
    PAwsCredentialProvider pCredentialProvider;

    // Back pointer to the callback provider object
    PCallbacksProvider pCallbacksProvider;
};

typedef struct __Ec2AuthCallbacks* PEc2AuthCallbacks;

////////////////////////////////////////////////////////////////////////
// Callback function implementations
////////////////////////////////////////////////////////////////////////

// The callback functions
STATUS getStreamingTokenEc2Func(UINT64, PCHAR, STREAM_ACCESS_MODE, PServiceCallContext);
STATUS getSecurityTokenEc2Func(UINT64, PBYTE*, PUINT32, PUINT64);
STATUS freeEc2AuthCallbacksFunc(PUINT64);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_EC2_AUTH_CALLBACKS_INCLUDE_I__ */
