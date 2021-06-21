/**
 * Kinesis Video Producer ECS based Credential Provider for libWebSockets
 */
#define LOG_CLASS "LwsEcsCredentialProvider"
#include "../Include_i.h"

STATUS createLwsEcsCredentialProvider(PCHAR ecsCredentialFullUri, PCHAR token, PAwsCredentialProvider* ppCredentialProvider)
{
    return createLwsEcsCredentialProviderWithTime(ecsCredentialFullUri, token, commonDefaultGetCurrentTimeFunc, 0, ppCredentialProvider);
}

STATUS createLwsEcsCredentialProviderWithTime(PCHAR ecsCredentialFullUri, PCHAR token, GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                              PAwsCredentialProvider* ppCredentialProvider)
{
    return createEcsCredentialProviderWithTime(ecsCredentialFullUri, token, getCurrentTimeFn, customData, blockingLwsHttpCall, ppCredentialProvider);
}
