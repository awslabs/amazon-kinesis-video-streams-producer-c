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