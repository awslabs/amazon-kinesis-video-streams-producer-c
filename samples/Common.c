#include "Samples.h"

VOID getEndpointOverride(PCHAR outUrl, SIZE_T maxLen)
{
    const char* envValue = GETENV(CONTROL_PLANE_URI_ENV_VAR);

    if (IS_NULL_OR_EMPTY_STRING(envValue)) {
        outUrl[0] = '\0';
        return;
    }

    if (STRNCMP(envValue, "https://", 8) != 0) {
        SNPRINTF(outUrl, maxLen, "https://%s", envValue);
        return;
    }

    SNPRINTF(outUrl, maxLen, "%s", envValue);
}

UINT32 getSampleLogLevel()
{
    UINT32 userLogLevel;
    const char* envValue = GETENV(DEBUG_LOG_LEVEL_ENV_VAR);

    // Default to debug
    if (IS_NULL_OR_EMPTY_STRING(envValue)) {
        return LOG_LEVEL_DEBUG;
    }

    if (STATUS_FAILED(STRTOUI32((PCHAR) envValue, NULL, 10, &userLogLevel))) {
        printf("failed to parse %s, set to debug\n", DEBUG_LOG_LEVEL_ENV_VAR);
        userLogLevel = LOG_LEVEL_DEBUG;
    }

    return userLogLevel;
}

STATUS createSampleCallbacksProvider(PCHAR region, PCHAR caCertPath, PCHAR userAgentPostfix, PCHAR customUserAgent,
                                     PClientCallbacks* ppClientCallbacks)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR accessKey = NULL, secretKey = NULL, sessionToken = NULL;
    PAuthCallbacks pAuthCallbacks = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
    CHAR endpointOverride[MAX_URI_CHAR_LEN];

    CHK(ppClientCallbacks != NULL, STATUS_NULL_ARG);

    getEndpointOverride(endpointOverride, SIZEOF(endpointOverride));

    accessKey = GETENV(ACCESS_KEY_ENV_VAR);
    secretKey = GETENV(SECRET_KEY_ENV_VAR);
    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);

    if (accessKey != NULL && secretKey != NULL) {
        DLOGI("Using environment variable credentials");
        CHK_STATUS(createDefaultCallbacksProviderWithAwsCredentialsAndEndpointOverride(accessKey, secretKey, sessionToken, MAX_UINT64, region,
                                                                                       caCertPath, userAgentPostfix, customUserAgent,
                                                                                       endpointOverride, ppClientCallbacks));
    } else {
        DLOGI("Environment variable credentials not found, using EC2 IMDS credential provider");
        CHK_STATUS(createAbstractDefaultCallbacksProvider(DEFAULT_CALLBACK_CHAIN_COUNT, API_CALL_CACHE_TYPE_ALL,
                                                          ENDPOINT_UPDATE_PERIOD_SENTINEL_VALUE, region, endpointOverride, caCertPath,
                                                          userAgentPostfix, customUserAgent, ppClientCallbacks));
        CHK_STATUS(createEc2AuthCallbacks(*ppClientCallbacks, &pAuthCallbacks));
        CHK_STATUS(createContinuousRetryStreamCallbacks(*ppClientCallbacks, &pStreamCallbacks));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && ppClientCallbacks != NULL && *ppClientCallbacks != NULL) {
        freeCallbacksProvider(ppClientCallbacks);
    }

    return retStatus;
}