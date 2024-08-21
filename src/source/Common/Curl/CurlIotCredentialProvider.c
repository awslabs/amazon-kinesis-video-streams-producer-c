/**
 * Kinesis Video Producer IoT based Credential Provider for libCurl
 */
#define LOG_CLASS "CurlIotCredentialProvider"
#include "../Include_i.h"

STATUS createCurlIotCredentialProvider(PCHAR iotGetCredentialEndpoint, PCHAR certPath, PCHAR privateKeyPath, PCHAR caCertPath, PCHAR roleAlias,
                                       PCHAR thingName, PAwsCredentialProvider* ppCredentialProvider)
{
    return createCurlIotCredentialProviderWithTime(iotGetCredentialEndpoint, certPath, privateKeyPath, caCertPath, roleAlias, thingName,
                                                   commonDefaultGetCurrentTimeFunc, 0, ppCredentialProvider);
}

STATUS createCurlIotCredentialProviderWithTime(PCHAR iotGetCredentialEndpoint, PCHAR certPath, PCHAR privateKeyPath, PCHAR caCertPath,
                                               PCHAR roleAlias, PCHAR thingName, GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                               PAwsCredentialProvider* ppCredentialProvider)
{
    return createIotCredentialProviderWithTimeAndIPVersion(iotGetCredentialEndpoint, certPath, privateKeyPath, caCertPath, roleAlias, thingName,
                                               IOT_REQUEST_CONNECTION_TIMEOUT, IOT_REQUEST_COMPLETION_TIMEOUT,  IPv4_ONLY, getCurrentTimeFn, customData,
                                               blockingCurlCall, ppCredentialProvider);
}

STATUS createCurlIotCredentialProviderWithTimeAndTimeout(PCHAR iotGetCredentialEndpoint, PCHAR certPath, PCHAR privateKeyPath, PCHAR caCertPath,
                                                         PCHAR roleAlias, PCHAR thingName, UINT64 connectionTimeout, UINT64 completionTimeout,
                                                         GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                                         PAwsCredentialProvider* ppCredentialProvider)
{
    return createIotCredentialProviderWithTimeAndIPVersion(iotGetCredentialEndpoint, certPath, privateKeyPath, caCertPath, roleAlias, thingName,
                                               connectionTimeout, completionTimeout, IPv4_ONLY, getCurrentTimeFn, customData, blockingCurlCall,
                                               ppCredentialProvider);
}

STATUS createCurlIotCredentialProviderWithTimeAndTimeoutAndIPVersion(PCHAR iotGetCredentialEndpoint, PCHAR certPath, PCHAR privateKeyPath, PCHAR caCertPath,
                                                         PCHAR roleAlias, PCHAR thingName, UINT64 connectionTimeout, UINT64 completionTimeout, IP_VERSION ipVersion, 
                                                         GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                                         PAwsCredentialProvider* ppCredentialProvider)
{
    return createIotCredentialProviderWithTimeAndIPVersion(iotGetCredentialEndpoint, certPath, privateKeyPath, caCertPath, roleAlias, thingName,
                                               connectionTimeout, completionTimeout, ipVersion, getCurrentTimeFn, customData, blockingCurlCall,
                                               ppCredentialProvider);
}
