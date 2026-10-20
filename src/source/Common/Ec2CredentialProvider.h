
#ifndef __KINESIS_VIDEO_EC2_CREDENTIAL_PROVIDER_INCLUDE_I__
#define __KINESIS_VIDEO_EC2_CREDENTIAL_PROVIDER_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define IMDS_REQUEST_CONNECTION_TIMEOUT (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define IMDS_REQUEST_COMPLETION_TIMEOUT (4 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define IMDS_DEFAULT_ENDPOINT  "169.254.169.254"
#define IMDS_TOKEN_PATH        "/latest/api/token"
#define IMDS_ROLE_PATH         "/latest/meta-data/iam/security-credentials/"
#define IMDS_TOKEN_TTL_HEADER  "X-aws-ec2-metadata-token-ttl-seconds"
#define IMDS_TOKEN_TTL_SECONDS "21600"
#define IMDS_TOKEN_HEADER      "X-aws-ec2-metadata-token"

#define IMDS_TOKEN_LEN     (2048)
#define IMDS_ROLE_NAME_LEN (256)

/**
 * Grace period for refreshing credentials before they expire
 */
#define IMDS_CREDENTIAL_FETCH_GRACE_PERIOD                                                                                                           \
    (5 * HUNDREDS_OF_NANOS_IN_A_SECOND + MIN_STREAMING_TOKEN_EXPIRATION_DURATION + STREAMING_TOKEN_EXPIRATION_GRACE_PERIOD)

/**
 * IMDSv2 token validity: 6 hours in 100ns units
 */
#define IMDS_TOKEN_EXPIRATION_DURATION (6 * 3600 * HUNDREDS_OF_NANOS_IN_A_SECOND)

typedef struct __Ec2CredentialProvider Ec2CredentialProvider;
struct __Ec2CredentialProvider {
    // First member should be the abstract credential provider
    AwsCredentialProvider credentialProvider;

    // Current time functionality - optional
    GetCurrentTimeFunc getCurrentTimeFn;

    // Custom data supplied to time function
    UINT64 customData;

    // IMDSv2 session token
    CHAR imdsToken[IMDS_TOKEN_LEN + 1];

    // When the IMDSv2 session token expires (100ns absolute time)
    UINT64 imdsTokenExpiration;

    UINT64 connectionTimeout;

    UINT64 completionTimeout;

    // Cached AWS credentials
    PAwsCredentials pAwsCredentials;

    // Service call functionality
    BlockingServiceCallFunc serviceCallFn;
};
typedef struct __Ec2CredentialProvider* PEc2CredentialProvider;

////////////////////////////////////////////////////////////////////////
// Callback function implementations
////////////////////////////////////////////////////////////////////////
STATUS createEc2CredentialProviderWithTime(UINT64, UINT64, GetCurrentTimeFunc, UINT64, BlockingServiceCallFunc, PAwsCredentialProvider*);
STATUS freeEc2CredentialProvider(PAwsCredentialProvider*);
STATUS getEc2Credentials(PAwsCredentialProvider, PAwsCredentials*);

// Internal functions
STATUS ec2CredentialHandler(PEc2CredentialProvider);
STATUS ec2FetchImdsToken(PEc2CredentialProvider);
STATUS parseEc2Response(PEc2CredentialProvider, PCallInfo);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_EC2_CREDENTIAL_PROVIDER_INCLUDE_I__ */
