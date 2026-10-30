/**
 * Kinesis Video Producer EC2 IMDS based Credential Provider for libCurl
 */
#define LOG_CLASS "CurlEc2CredentialProvider"
#include "../Include_i.h"

STATUS createCurlEc2CredentialProvider(PAwsCredentialProvider* ppCredentialProvider)
{
    return createCurlEc2CredentialProviderWithTime(commonDefaultGetCurrentTimeFunc, 0, ppCredentialProvider);
}

STATUS createCurlEc2CredentialProviderWithTime(GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData, PAwsCredentialProvider* ppCredentialProvider)
{
    return createEc2CredentialProviderWithTime(IMDS_REQUEST_CONNECTION_TIMEOUT, IMDS_REQUEST_COMPLETION_TIMEOUT, getCurrentTimeFn, customData,
                                               blockingCurlCall, ppCredentialProvider);
}
