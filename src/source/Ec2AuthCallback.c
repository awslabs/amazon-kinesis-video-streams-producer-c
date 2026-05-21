/**
 * Kinesis Video Producer EC2 IMDS Auth Callback
 */
#define LOG_CLASS "Ec2AuthCallbacks"
#include "Include_i.h"

STATUS createEc2AuthCallbacks(PClientCallbacks pCallbacksProvider, PAuthCallbacks* ppEc2AuthCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    PEc2AuthCallbacks pEc2AuthCallbacks = NULL;

    CHK(pCallbacksProvider != NULL && ppEc2AuthCallbacks != NULL, STATUS_NULL_ARG);

    pEc2AuthCallbacks = (PEc2AuthCallbacks) MEMCALLOC(1, SIZEOF(Ec2AuthCallbacks));
    CHK(pEc2AuthCallbacks != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pEc2AuthCallbacks->authCallbacks.version = AUTH_CALLBACKS_CURRENT_VERSION;
    pEc2AuthCallbacks->authCallbacks.customData = (UINT64) pEc2AuthCallbacks;

    pEc2AuthCallbacks->pCallbacksProvider = (PCallbacksProvider) pCallbacksProvider;

    pEc2AuthCallbacks->authCallbacks.getStreamingTokenFn = getStreamingTokenEc2Func;
    pEc2AuthCallbacks->authCallbacks.getSecurityTokenFn = getSecurityTokenEc2Func;
    pEc2AuthCallbacks->authCallbacks.freeAuthCallbacksFn = freeEc2AuthCallbacksFunc;
    pEc2AuthCallbacks->authCallbacks.getDeviceCertificateFn = NULL;
    pEc2AuthCallbacks->authCallbacks.deviceCertToTokenFn = NULL;
    pEc2AuthCallbacks->authCallbacks.getDeviceFingerprintFn = NULL;

    CHK_STATUS(createCurlEc2CredentialProviderWithTime(pEc2AuthCallbacks->pCallbacksProvider->clientCallbacks.getCurrentTimeFn,
                                                       pEc2AuthCallbacks->pCallbacksProvider->clientCallbacks.customData,
                                                       (PAwsCredentialProvider*) &pEc2AuthCallbacks->pCredentialProvider));

    CHK_STATUS(addAuthCallbacks(pCallbacksProvider, (PAuthCallbacks) pEc2AuthCallbacks));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeEc2AuthCallbacks((PAuthCallbacks*) &pEc2AuthCallbacks);
        pEc2AuthCallbacks = NULL;
    }

    if (ppEc2AuthCallbacks != NULL) {
        *ppEc2AuthCallbacks = (PAuthCallbacks) pEc2AuthCallbacks;
    }

    LEAVES();
    return retStatus;
}

STATUS freeEc2AuthCallbacks(PAuthCallbacks* ppEc2AuthCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    PEc2AuthCallbacks pEc2AuthCallbacks = NULL;

    CHK(ppEc2AuthCallbacks != NULL, STATUS_NULL_ARG);

    pEc2AuthCallbacks = (PEc2AuthCallbacks) *ppEc2AuthCallbacks;

    // Call is idempotent
    CHK(pEc2AuthCallbacks != NULL, retStatus);

    // Release the underlying AWS credentials provider object
    freeEc2CredentialProvider((PAwsCredentialProvider*) &pEc2AuthCallbacks->pCredentialProvider);

    // Release the object
    MEMFREE(pEc2AuthCallbacks);

    // Set the pointer to NULL
    *ppEc2AuthCallbacks = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS freeEc2AuthCallbacksFunc(PUINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PEc2AuthCallbacks pAuthCallbacks;

    CHK(customData != NULL, STATUS_NULL_ARG);
    pAuthCallbacks = (PEc2AuthCallbacks) *customData;
    CHK_STATUS(freeEc2AuthCallbacks((PAuthCallbacks*) &pAuthCallbacks));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS getStreamingTokenEc2Func(UINT64 customData, PCHAR streamName, STREAM_ACCESS_MODE accessMode, PServiceCallContext pServiceCallContext)
{
    UNUSED_PARAM(streamName);
    UNUSED_PARAM(accessMode);

    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials = NULL;
    PAwsCredentialProvider pCredentialProvider;
    PCallbacksProvider pCallbacksProvider = NULL;
    PEc2AuthCallbacks pEc2AuthCallbacks = (PEc2AuthCallbacks) customData;

    CHK(pEc2AuthCallbacks != NULL && pServiceCallContext != NULL, STATUS_NULL_ARG);

    pCallbacksProvider = pEc2AuthCallbacks->pCallbacksProvider;
    pCredentialProvider = (PAwsCredentialProvider) pEc2AuthCallbacks->pCredentialProvider;
    CHK_STATUS(pCredentialProvider->getCredentialsFn(pCredentialProvider, &pAwsCredentials));

    CHK_STATUS(getStreamingTokenResultEvent(pServiceCallContext->customData, SERVICE_CALL_RESULT_OK, (PBYTE) pAwsCredentials, pAwsCredentials->size,
                                            pAwsCredentials->expiration));

CleanUp:

    if (STATUS_FAILED(retStatus) && pServiceCallContext != NULL) {
        getStreamingTokenResultEvent(pServiceCallContext->customData, SERVICE_CALL_UNKNOWN, NULL, 0, 0);
        notifyCallResult(pCallbacksProvider, retStatus, pServiceCallContext->customData);
    }

    LEAVES();
    return retStatus;
}

STATUS getSecurityTokenEc2Func(UINT64 customData, PBYTE* ppBuffer, PUINT32 pSize, PUINT64 pExpiration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials;
    PAwsCredentialProvider pCredentialProvider;

    PEc2AuthCallbacks pEc2AuthCallbacks = (PEc2AuthCallbacks) customData;
    CHK(pEc2AuthCallbacks != NULL && ppBuffer != NULL && pSize != NULL && pExpiration != NULL, STATUS_NULL_ARG);

    pCredentialProvider = (PAwsCredentialProvider) pEc2AuthCallbacks->pCredentialProvider;
    CHK_STATUS(pCredentialProvider->getCredentialsFn(pCredentialProvider, &pAwsCredentials));

    *pExpiration = pAwsCredentials->expiration;
    *pSize = pAwsCredentials->size;
    *ppBuffer = (PBYTE) pAwsCredentials;

CleanUp:

    LEAVES();
    return retStatus;
}
