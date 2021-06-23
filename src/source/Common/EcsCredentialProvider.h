
#ifndef __KINESIS_VIDEO_ECS_CREDENTIAL_PROVIDER_INCLUDE_I__
#define __KINESIS_VIDEO_ECS_CREDENTIAL_PROVIDER_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_REQUEST_CONNECTION_TIMEOUT (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define ECS_REQUEST_COMPLETION_TIMEOUT (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define ECS_AUTH_TOKEN_HEADER          "authorization"

/**
 * Service call callback functionality
 */
typedef STATUS (*BlockingServiceCallFunc)(PRequestInfo, PCallInfo);

/**
 * Grace period which is added to the current time to determine whether the extracted credentials are still valid
 */
#define ECS_CREDENTIAL_FETCH_GRACE_PERIOD                                                                                                            \
    (5 * HUNDREDS_OF_NANOS_IN_A_SECOND + MIN_STREAMING_TOKEN_EXPIRATION_DURATION + STREAMING_TOKEN_EXPIRATION_GRACE_PERIOD)

typedef struct __EcsCredentialProvider EcsCredentialProvider;
struct __EcsCredentialProvider {
    // First member should be the abstract credential provider
    AwsCredentialProvider credentialProvider;

    // Current time functionality - optional
    GetCurrentTimeFunc getCurrentTimeFn;

    // Custom data supplied to time function
    UINT64 customData;

    // Ecs credential endpoint
    CHAR ecsGetCredentialEndpoint[MAX_URI_CHAR_LEN + 1];
    CHAR ecsGetCredentialResource[MAX_URI_CHAR_LEN + 1];
    CHAR token[MAX_ECS_TOKEN_LEN + 1];
    UINT32 port;

    // Static Aws Credentials structure with the pointer following the main allocation
    PAwsCredentials pAwsCredentials;

    // Service call functionality
    BlockingServiceCallFunc serviceCallFn;
};
typedef struct __EcsCredentialProvider* PEcsCredentialProvider;

////////////////////////////////////////////////////////////////////////
// Callback function implementations
////////////////////////////////////////////////////////////////////////
STATUS createEcsCredentialProviderWithTime(PCHAR, PCHAR, GetCurrentTimeFunc, UINT64, BlockingServiceCallFunc, PAwsCredentialProvider*);
STATUS getEcsCredentials(PAwsCredentialProvider, PAwsCredentials*);

// internal functions
STATUS ecsCurlHandler(PEcsCredentialProvider);
STATUS parseEcsResponse(PEcsCredentialProvider, PCallInfo);
STATUS getHostPort(PCHAR, PCHAR*, PCHAR*);
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_ECS_CREDENTIAL_PROVIDER_INCLUDE_I__ */
