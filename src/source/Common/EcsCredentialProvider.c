/**
 * Kinesis Video Producer ECS based Credential Provider
 */
#define LOG_CLASS "EcsCredentialProvider"
#include "Include_i.h"

STATUS createEcsCredentialProviderWithTime(PCHAR ecsCredentialFullUri, PCHAR token, GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                           BlockingServiceCallFunc serviceCallFn, PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PEcsCredentialProvider pEcsCredentialProvider = NULL;
    PCHAR pStart, pEnd;
    UINT32 len;
    UINT32 port = 0;
    UINT32 fullUriLen = 0;

    CHK(ppCredentialProvider != NULL && ecsCredentialFullUri != NULL && token != NULL && serviceCallFn != NULL, STATUS_NULL_ARG);

    pEcsCredentialProvider = (PEcsCredentialProvider) MEMCALLOC(1, SIZEOF(EcsCredentialProvider));
    CHK(pEcsCredentialProvider != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pEcsCredentialProvider->credentialProvider.getCredentialsFn = getEcsCredentials;

    // Store the time functionality and specify default if NULL
    pEcsCredentialProvider->getCurrentTimeFn = (getCurrentTimeFn == NULL) ? commonDefaultGetCurrentTimeFunc : getCurrentTimeFn;
    pEcsCredentialProvider->customData = customData;

    fullUriLen = STRNLEN(ecsCredentialFullUri, MAX_URI_CHAR_LEN + 1);

    CHK(fullUriLen <= MAX_URI_CHAR_LEN, MAX_URI_CHAR_LEN);
    CHK_STATUS(getRequestHost(ecsCredentialFullUri, &pStart, &pEnd));
    len = (UINT32)(pEnd - ecsCredentialFullUri);
    STRNCPY(pEcsCredentialProvider->ecsGetCredentialEndpoint, ecsCredentialFullUri, len);
    pEcsCredentialProvider->ecsGetCredentialEndpoint[len] = '\0';

    CHK_STATUS(getHostPort(ecsCredentialFullUri + len, &pStart, &pEnd));
    CHK_STATUS(STRTOUI32(pStart, pEnd, 10, &port));
    pEcsCredentialProvider->port = port;
    len = (ecsCredentialFullUri + fullUriLen) - pEnd;
    STRNCPY(pEcsCredentialProvider->ecsGetCredentialResource, pEnd + 1, len);

    CHK(STRNLEN(token, MAX_ECS_TOKEN_LEN + 1) <= MAX_ECS_TOKEN_LEN, STATUS_MAX_ECS_TOKEN_LENGTH);
    STRNCPY(pEcsCredentialProvider->token, token, MAX_ECS_TOKEN_LEN);

    pEcsCredentialProvider->serviceCallFn = serviceCallFn;

    CHK_STATUS(ecsCurlHandler(pEcsCredentialProvider));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeEcsCredentialProvider((PAwsCredentialProvider*) &pEcsCredentialProvider);
        pEcsCredentialProvider = NULL;
    }

    // Set the return value if it's not NULL
    if (ppCredentialProvider != NULL) {
        *ppCredentialProvider = (PAwsCredentialProvider) pEcsCredentialProvider;
    }

    LEAVES();
    return retStatus;
}

STATUS freeEcsCredentialProvider(PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PEcsCredentialProvider pEcsCredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);

    pEcsCredentialProvider = (PEcsCredentialProvider) *ppCredentialProvider;

    // Call is idempotent
    CHK(pEcsCredentialProvider != NULL, retStatus);

    // Release the underlying AWS credentials object
    freeAwsCredentials(&pEcsCredentialProvider->pAwsCredentials);

    // Release the object
    MEMFREE(pEcsCredentialProvider);

    // Set the pointer to NULL
    *ppCredentialProvider = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS getEcsCredentials(PAwsCredentialProvider pCredentialProvider, PAwsCredentials* ppAwsCredentials)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;

    PEcsCredentialProvider pEcsCredentialProvider = (PEcsCredentialProvider) pCredentialProvider;

    CHK(pEcsCredentialProvider != NULL && ppAwsCredentials != NULL, STATUS_NULL_ARG);

    // Fill the credentials
    CHK_STATUS(ecsCurlHandler(pEcsCredentialProvider));

    *ppAwsCredentials = pEcsCredentialProvider->pAwsCredentials;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS parseEcsResponse(PEcsCredentialProvider pEcsCredentialProvider, PCallInfo pCallInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    UINT32 i, resultLen, accessKeyIdLen = 0, secretKeyLen = 0, sessionTokenLen = 0, expirationTimestampLen = 0;
    INT32 tokenCount;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    PCHAR accessKeyId = NULL, secretKey = NULL, sessionToken = NULL, expirationTimestamp = NULL, pResponseStr = NULL;
    UINT64 expiration, currentTime;
    CHAR expirationTimestampStr[MAX_EXPIRATION_LEN + 1];

    CHK(pEcsCredentialProvider != NULL && pCallInfo != NULL, STATUS_NULL_ARG);

    resultLen = pCallInfo->responseDataLen;
    pResponseStr = pCallInfo->responseData;
    CHK(resultLen > 0, STATUS_ECS_AUTH_FAILED);

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));

    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    for (i = 1; i < (UINT32) tokenCount; i++) {
        if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "AccessKeyId")) {
            accessKeyIdLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(accessKeyIdLen <= MAX_ACCESS_KEY_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            accessKeyId = pResponseStr + tokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "SecretAccessKey")) {
            secretKeyLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(secretKeyLen <= MAX_SECRET_KEY_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            secretKey = pResponseStr + tokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Token")) {
            sessionTokenLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(sessionTokenLen <= MAX_SESSION_TOKEN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            sessionToken = pResponseStr + tokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, "Expiration")) {
            expirationTimestampLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(expirationTimestampLen <= MAX_EXPIRATION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            expirationTimestamp = pResponseStr + tokens[i + 1].start;
            MEMCPY(expirationTimestampStr, expirationTimestamp, expirationTimestampLen);
            expirationTimestampStr[expirationTimestampLen] = '\0';
            i++;
        }
    }

    CHK(accessKeyId != NULL && secretKey != NULL && sessionToken != NULL, STATUS_ECS_AUTH_RSP_FAILED);

    currentTime = pEcsCredentialProvider->getCurrentTimeFn(pEcsCredentialProvider->customData);
    CHK_STATUS(convertTimestampToEpoch(expirationTimestampStr, currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration));
    DLOGD("Ecs credential expiration time %" PRIu64, expiration / HUNDREDS_OF_NANOS_IN_A_SECOND);

    if (pEcsCredentialProvider->pAwsCredentials != NULL) {
        freeAwsCredentials(&pEcsCredentialProvider->pAwsCredentials);
        pEcsCredentialProvider->pAwsCredentials = NULL;
    }

    // Fix-up the expiration to be no more than max enforced token rotation to avoid extra token rotations
    // as we are caching the returned value which is likely to be an hour but we are enforcing max
    // rotation to be more frequent.
    expiration = MIN(expiration, currentTime + MAX_ENFORCED_TOKEN_EXPIRATION_DURATION);

    CHK_STATUS(createAwsCredentials(accessKeyId, accessKeyIdLen, secretKey, secretKeyLen, sessionToken, sessionTokenLen, expiration,
                                    &pEcsCredentialProvider->pAwsCredentials));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS ecsCurlHandler(PEcsCredentialProvider pEcsCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currentTime;
    UINT32 formatLen = 0;
    CHAR serviceUrl[MAX_URI_CHAR_LEN + 1];
    PRequestInfo pRequestInfo = NULL;
    CallInfo callInfo;

    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));

    // Refresh the credentials
    currentTime = pEcsCredentialProvider->getCurrentTimeFn(pEcsCredentialProvider->customData);

    CHK(pEcsCredentialProvider->pAwsCredentials == NULL ||
            currentTime + ECS_CREDENTIAL_FETCH_GRACE_PERIOD > pEcsCredentialProvider->pAwsCredentials->expiration,
        retStatus);

    formatLen = SNPRINTF(serviceUrl, MAX_URI_CHAR_LEN, "%s/%s", pEcsCredentialProvider->ecsGetCredentialEndpoint,
                         pEcsCredentialProvider->ecsGetCredentialResource);
    CHK(formatLen > 0 && formatLen < MAX_URI_CHAR_LEN, STATUS_ECS_AUTH_URI_FAILED);

    // Form a new request info based on the params
    CHK_STATUS(createRequestInfo(serviceUrl, NULL, pEcsCredentialProvider->port, DEFAULT_AWS_REGION, NULL, NULL, NULL, SSL_CERTIFICATE_TYPE_PEM,
                                 DEFAULT_USER_AGENT_NAME, ECS_REQUEST_CONNECTION_TIMEOUT, ECS_REQUEST_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT,
                                 DEFAULT_LOW_SPEED_TIME_LIMIT, pEcsCredentialProvider->pAwsCredentials, &pRequestInfo));

    callInfo.pRequestInfo = pRequestInfo;

    // Append the Ecs header
    CHK_STATUS(setRequestHeader(pRequestInfo, ECS_AUTH_TOKEN_HEADER, 0, pEcsCredentialProvider->token, 0));

    // Perform a blocking call
    CHK_STATUS(pEcsCredentialProvider->serviceCallFn(pRequestInfo, &callInfo));

    // Parse the response and get the credentials
    CHK_STATUS(parseEcsResponse(pEcsCredentialProvider, &callInfo));

CleanUp:

    if (pRequestInfo != NULL) {
        freeRequestInfo(&pRequestInfo);
    }

    releaseCallInfo(&callInfo);

    return retStatus;
}

STATUS getHostPort(PCHAR pUrl, PCHAR* ppStart, PCHAR* ppEnd)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pStart = NULL, pEnd = NULL, pCurPtr;
    UINT32 urlLen;
    BOOL iterate = TRUE;

    CHK(pUrl != NULL && ppStart != NULL && ppEnd != NULL, STATUS_NULL_ARG);

    // We know for sure url is NULL terminated
    urlLen = (UINT32) STRNLEN(pUrl, MAX_URI_CHAR_LEN + 1);

    // Start from the schema delimiter
    pStart = STRSTR(pUrl, PORT_DELIMITER_STRING);
    CHK(pStart != NULL, STATUS_INVALID_ARG);

    // Advance the pStart past the delimiter
    pStart += (ARRAY_SIZE(PORT_DELIMITER_STRING) - 1);

    // Ensure we are not past the string
    CHK(pUrl + urlLen > pStart, STATUS_INVALID_ARG);

    // Set the end first
    pEnd = pUrl + urlLen;

    // Find the delimiter which would indicate end of the host - either one of "/:?"
    pCurPtr = pStart;
    while (iterate && pCurPtr <= pEnd) {
        switch (*pCurPtr) {
            case '/':
            case ':':
            case '?':
                iterate = FALSE;

                // Set the new end value
                pEnd = pCurPtr;
            default:
                pCurPtr++;
        }
    }

CleanUp:

    if (ppStart != NULL) {
        *ppStart = pStart;
    }

    if (ppEnd != NULL) {
        *ppEnd = pEnd;
    }

    return retStatus;
}
