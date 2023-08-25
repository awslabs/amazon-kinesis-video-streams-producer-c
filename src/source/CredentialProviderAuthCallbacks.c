/**
 * Kinesis Video Producer Static Auth Callbacks
 */
#define LOG_CLASS "CredentialProviderAuthCallbacks"
#include "Include_i.h"

/**
 * Creates Credential Provider Auth callbacks
 */
STATUS createCredentialProviderAuthCallbacks(PClientCallbacks pCallbacksProvider, PAwsCredentialProvider pCredentialProvider,
                                             PAuthCallbacks* ppCredentialProviderAuthCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCredentialProviderAuthCallbacks pCredentialProviderAuthCallbacks = NULL;

    CHK(pCallbacksProvider != NULL && pCredentialProvider != NULL && ppCredentialProviderAuthCallbacks != NULL, STATUS_NULL_ARG);

    // Allocate the entire structure
    pCredentialProviderAuthCallbacks = (PCredentialProviderAuthCallbacks) MEMCALLOC(1, SIZEOF(CredentialProviderAuthCallbacks));
    CHK(pCredentialProviderAuthCallbacks != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Set the version, self
    pCredentialProviderAuthCallbacks->authCallbacks.version = AUTH_CALLBACKS_CURRENT_VERSION;
    pCredentialProviderAuthCallbacks->authCallbacks.customData = (UINT64) pCredentialProviderAuthCallbacks;

    // Store the back pointer as we will be using the other callbacks
    pCredentialProviderAuthCallbacks->pCallbacksProvider = (PCallbacksProvider) pCallbacksProvider;

    // Set the callbacks
    pCredentialProviderAuthCallbacks->authCallbacks.getStreamingTokenFn = getStreamingTokenCredentialProviderFunc;
    pCredentialProviderAuthCallbacks->authCallbacks.getSecurityTokenFn = getSecurityTokenCredentialProviderFunc;
    pCredentialProviderAuthCallbacks->authCallbacks.freeAuthCallbacksFn = freeCredentialProviderAuthCallbacksFunc;
    pCredentialProviderAuthCallbacks->authCallbacks.getDeviceCertificateFn = NULL;
    pCredentialProviderAuthCallbacks->authCallbacks.deviceCertToTokenFn = NULL;
    pCredentialProviderAuthCallbacks->authCallbacks.getDeviceFingerprintFn = NULL;

    // Set the provider
    pCredentialProviderAuthCallbacks->pCredentialProvider = pCredentialProvider;

    // Append to the auth chain
    CHK_STATUS(addAuthCallbacks(pCallbacksProvider, (PAuthCallbacks) pCredentialProviderAuthCallbacks));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeCredentialProviderAuthCallbacks((PAuthCallbacks*) &pCredentialProviderAuthCallbacks);
        pCredentialProviderAuthCallbacks = NULL;
    }

    // Set the return value if it's not NULL
    if (ppCredentialProviderAuthCallbacks != NULL) {
        *ppCredentialProviderAuthCallbacks = (PAuthCallbacks) pCredentialProviderAuthCallbacks;
    }

    LEAVES();
    return retStatus;
}

/**
 * Frees the Credential Provider based Auth callbacks object
 *
 * NOTE: The caller should have passed a pointer which was previously created by the corresponding function
 * NOTE: The underlying credential provider object will not be freed
 */
STATUS freeCredentialProviderAuthCallbacks(PAuthCallbacks* ppCredentialProviderAuthCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCredentialProviderAuthCallbacks pCredentialProviderAuthCallbacks = NULL;

    CHK(ppCredentialProviderAuthCallbacks != NULL, STATUS_NULL_ARG);

    pCredentialProviderAuthCallbacks = (PCredentialProviderAuthCallbacks) *ppCredentialProviderAuthCallbacks;

    // Call is idempotent
    CHK(pCredentialProviderAuthCallbacks != NULL, retStatus);

    // Release the object
    MEMFREE(pCredentialProviderAuthCallbacks);

    // Set the pointer to NULL
    *ppCredentialProviderAuthCallbacks = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS freeCredentialProviderAuthCallbacksFunc(PUINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCredentialProviderAuthCallbacks pAuthCallbacks;

    CHK(customData != NULL, STATUS_NULL_ARG);
    pAuthCallbacks = (PCredentialProviderAuthCallbacks) *customData;
    CHK_STATUS(freeCredentialProviderAuthCallbacks((PAuthCallbacks*) &pAuthCallbacks));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS getStreamingTokenCredentialProviderFunc(UINT64 customData, PCHAR streamName, STREAM_ACCESS_MODE accessMode,
                                               PServiceCallContext pServiceCallContext)
{
    UNUSED_PARAM(streamName);
    UNUSED_PARAM(accessMode);
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials;
    PAwsCredentialProvider pCredentialProvider;

    PCredentialProviderAuthCallbacks pCredentialProviderAuthCallbacks = (PCredentialProviderAuthCallbacks) customData;

    CHK(pCredentialProviderAuthCallbacks != NULL, STATUS_NULL_ARG);

    pCredentialProvider = (PAwsCredentialProvider) pCredentialProviderAuthCallbacks->pCredentialProvider;
    CHK_STATUS(pCredentialProvider->getCredentialsFn(pCredentialProvider, &pAwsCredentials));

    retStatus = getStreamingTokenResultEvent(pServiceCallContext->customData, SERVICE_CALL_RESULT_OK, (PBYTE) pAwsCredentials, pAwsCredentials->size,
                                             pAwsCredentials->expiration);

    PCallbacksProvider pCallbacksProvider = pCredentialProviderAuthCallbacks->pCallbacksProvider;

    notifyCallResult(pCallbacksProvider, retStatus, customData);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS getSecurityTokenCredentialProviderFunc(UINT64 customData, PBYTE* ppBuffer, PUINT32 pSize, PUINT64 pExpiration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCredentialProviderAuthCallbacks pCredentialProviderAuthCallbacks = (PCredentialProviderAuthCallbacks) customData;
    PAwsCredentials pAwsCredentials;
    PAwsCredentialProvider pCredentialProvider;

    CHK(pCredentialProviderAuthCallbacks != NULL && ppBuffer != NULL && pSize != NULL && pExpiration != NULL, STATUS_NULL_ARG);

    pCredentialProvider = (PAwsCredentialProvider) pCredentialProviderAuthCallbacks->pCredentialProvider;
    CHK_STATUS(pCredentialProvider->getCredentialsFn(pCredentialProvider, &pAwsCredentials));

    *pExpiration = pAwsCredentials->expiration;
    *pSize = pAwsCredentials->size;
    *ppBuffer = (PBYTE) pAwsCredentials;

CleanUp:

    LEAVES();
    return retStatus;
}
