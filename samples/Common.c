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