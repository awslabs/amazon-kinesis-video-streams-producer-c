/**
 * Kinesis Video Producer EC2 IMDS based Credential Provider for libWebSockets
 */
#define LOG_CLASS "LwsEc2CredentialProvider"
#include "../Include_i.h"

STATUS createLwsEc2CredentialProvider(PAwsCredentialProvider* ppCredentialProvider)
{
    return createLwsEc2CredentialProviderWithTime(commonDefaultGetCurrentTimeFunc, 0, ppCredentialProvider);
}

STATUS createLwsEc2CredentialProviderWithTime(GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData, PAwsCredentialProvider* ppCredentialProvider)
{
    return createEc2CredentialProviderWithTime(IMDS_REQUEST_CONNECTION_TIMEOUT, IMDS_REQUEST_COMPLETION_TIMEOUT, getCurrentTimeFn, customData,
                                               blockingLwsCall, ppCredentialProvider);
}
