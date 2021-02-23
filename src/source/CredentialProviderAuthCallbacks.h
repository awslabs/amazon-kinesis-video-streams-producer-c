
#ifndef __KINESIS_VIDEO_CREDENTIAL_PROVIDER_AUTH_CALLBACKS_INCLUDE_I__
#define __KINESIS_VIDEO_CREDENTIAL_PROVIDER_AUTH_CALLBACKS_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward declarations
 */

typedef struct __CredentialProviderAuthCallbacks CredentialProviderAuthCallbacks;

struct __CredentialProviderAuthCallbacks {
    // First member should be the Auth callbacks
    AuthCallbacks authCallbacks;

    // Static credential provider
    PAwsCredentialProvider pCredentialProvider;

    // Back pointer to the callback provider object
    PCallbacksProvider pCallbacksProvider;
};

typedef struct __CredentialProviderAuthCallbacks* PCredentialProviderAuthCallbacks;

////////////////////////////////////////////////////////////////////////
// Callback function implementations
////////////////////////////////////////////////////////////////////////

// The callback functions
STATUS getStreamingTokenCredentialProviderFunc(UINT64, PCHAR, STREAM_ACCESS_MODE, PServiceCallContext);
STATUS getSecurityTokenCredentialProviderFunc(UINT64, PBYTE*, PUINT32, PUINT64);
STATUS freeCredentialProviderAuthCallbacksFunc(PUINT64);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_CREDENTIAL_PROVIDER_AUTH_CALLBACKS_INCLUDE_I__ */
