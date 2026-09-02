/**
 * Kinesis Video Producer EC2 IMDS based Credential Provider
 */
#define LOG_CLASS "Ec2CredentialProvider"
#include "Include_i.h"

STATUS createEc2CredentialProviderWithTime(UINT64 connectionTimeout, UINT64 completionTimeout, GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                           BlockingServiceCallFunc serviceCallFn, PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PEc2CredentialProvider pEc2CredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL && serviceCallFn != NULL, STATUS_NULL_ARG);

    pEc2CredentialProvider = (PEc2CredentialProvider) MEMCALLOC(1, SIZEOF(Ec2CredentialProvider));
    CHK(pEc2CredentialProvider != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pEc2CredentialProvider->credentialProvider.getCredentialsFn = getEc2Credentials;

    pEc2CredentialProvider->getCurrentTimeFn = (getCurrentTimeFn == NULL) ? commonDefaultGetCurrentTimeFunc : getCurrentTimeFn;
    pEc2CredentialProvider->customData = customData;

    if (completionTimeout < connectionTimeout) {
        connectionTimeout = IMDS_REQUEST_CONNECTION_TIMEOUT;
        completionTimeout = IMDS_REQUEST_COMPLETION_TIMEOUT;
    }
    if (connectionTimeout == 0) {
        connectionTimeout = IMDS_REQUEST_CONNECTION_TIMEOUT;
    }
    if (completionTimeout == 0) {
        completionTimeout = IMDS_REQUEST_COMPLETION_TIMEOUT;
    }
    pEc2CredentialProvider->connectionTimeout = connectionTimeout;
    pEc2CredentialProvider->completionTimeout = completionTimeout;

    pEc2CredentialProvider->serviceCallFn = serviceCallFn;
    pEc2CredentialProvider->imdsTokenExpiration = 0;

    CHK_STATUS(ec2CredentialHandler(pEc2CredentialProvider));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeEc2CredentialProvider((PAwsCredentialProvider*) &pEc2CredentialProvider);
        pEc2CredentialProvider = NULL;
    }

    if (ppCredentialProvider != NULL) {
        *ppCredentialProvider = (PAwsCredentialProvider) pEc2CredentialProvider;
    }

    LEAVES();
    return retStatus;
}

STATUS freeEc2CredentialProvider(PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PEc2CredentialProvider pEc2CredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);

    pEc2CredentialProvider = (PEc2CredentialProvider) *ppCredentialProvider;

    // Call is idempotent
    CHK(pEc2CredentialProvider != NULL, retStatus);

    // Release the underlying AWS credentials object
    freeAwsCredentials(&pEc2CredentialProvider->pAwsCredentials);

    // Release the object
    MEMFREE(pEc2CredentialProvider);

    // Set the pointer to NULL
    *ppCredentialProvider = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS getEc2Credentials(PAwsCredentialProvider pCredentialProvider, PAwsCredentials* ppAwsCredentials)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PEc2CredentialProvider pEc2CredentialProvider = (PEc2CredentialProvider) pCredentialProvider;

    CHK(pEc2CredentialProvider != NULL && ppAwsCredentials != NULL, STATUS_NULL_ARG);

    CHK_STATUS(ec2CredentialHandler(pEc2CredentialProvider));

    *ppAwsCredentials = pEc2CredentialProvider->pAwsCredentials;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS ec2FetchImdsToken(PEc2CredentialProvider pEc2CredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currentTime;
    CHAR tokenUrl[MAX_URI_CHAR_LEN + 1];
    PRequestInfo pRequestInfo = NULL;
    CallInfo callInfo;

    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));

    CHK(pEc2CredentialProvider != NULL, STATUS_NULL_ARG);

    currentTime = pEc2CredentialProvider->getCurrentTimeFn(pEc2CredentialProvider->customData);

    // Check if existing token is still valid
    CHK(pEc2CredentialProvider->imdsTokenExpiration == 0 || currentTime >= pEc2CredentialProvider->imdsTokenExpiration, retStatus);

    SNPRINTF(tokenUrl, MAX_URI_CHAR_LEN, "http://%s%s", IMDS_DEFAULT_ENDPOINT, IMDS_TOKEN_PATH);

    CHK_STATUS(createRequestInfo(tokenUrl, NULL, DEFAULT_AWS_REGION, NULL, NULL, NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, DEFAULT_USER_AGENT_NAME,
                                 pEc2CredentialProvider->connectionTimeout, pEc2CredentialProvider->completionTimeout, DEFAULT_LOW_SPEED_LIMIT,
                                 DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, &pRequestInfo));

    pRequestInfo->verb = HTTP_REQUEST_VERB_PUT;

    CHK_STATUS(setRequestHeader(pRequestInfo, (PCHAR) IMDS_TOKEN_TTL_HEADER, 0, (PCHAR) IMDS_TOKEN_TTL_SECONDS, 0));

    callInfo.pRequestInfo = pRequestInfo;

    CHK_STATUS(pEc2CredentialProvider->serviceCallFn(pRequestInfo, &callInfo));

    CHK_ERR(callInfo.responseDataLen > 0 && callInfo.responseDataLen <= IMDS_TOKEN_LEN, STATUS_IMDS_TOKEN_FETCH_FAILED,
            "Failed to fetch IMDSv2 session token from %s. Verify that the instance has IMDSv2 enabled and the metadata service is reachable.",
            tokenUrl);

    MEMCPY(pEc2CredentialProvider->imdsToken, callInfo.responseData, callInfo.responseDataLen);
    pEc2CredentialProvider->imdsToken[callInfo.responseDataLen] = '\0';

    pEc2CredentialProvider->imdsTokenExpiration = currentTime + IMDS_TOKEN_EXPIRATION_DURATION;

CleanUp:

    if (pRequestInfo != NULL) {
        freeRequestInfo(&pRequestInfo);
    }

    releaseCallInfo(&callInfo);

    LEAVES();
    return retStatus;
}

STATUS ec2CredentialHandler(PEc2CredentialProvider pEc2CredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currentTime;
    CHAR roleUrl[MAX_URI_CHAR_LEN + 1];
    CHAR credUrl[MAX_URI_CHAR_LEN + 1];
    CHAR roleName[IMDS_ROLE_NAME_LEN + 1];
    PRequestInfo pRequestInfo = NULL;
    CallInfo callInfo;

    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));

    CHK(pEc2CredentialProvider != NULL, STATUS_NULL_ARG);

    // Check if cached credentials are still valid
    currentTime = pEc2CredentialProvider->getCurrentTimeFn(pEc2CredentialProvider->customData);
    CHK(pEc2CredentialProvider->pAwsCredentials == NULL ||
            currentTime + IMDS_CREDENTIAL_FETCH_GRACE_PERIOD > pEc2CredentialProvider->pAwsCredentials->expiration,
        retStatus);

    DLOGI("Attempting to fetch EC2 IMDS credentials");

    // Ensure we have a valid IMDSv2 token
    CHK_STATUS(ec2FetchImdsToken(pEc2CredentialProvider));

    // Step 1: Get the IAM role name
    SNPRINTF(roleUrl, MAX_URI_CHAR_LEN, "http://%s%s", IMDS_DEFAULT_ENDPOINT, IMDS_ROLE_PATH);

    CHK_STATUS(createRequestInfo(roleUrl, NULL, DEFAULT_AWS_REGION, NULL, NULL, NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, DEFAULT_USER_AGENT_NAME,
                                 pEc2CredentialProvider->connectionTimeout, pEc2CredentialProvider->completionTimeout, DEFAULT_LOW_SPEED_LIMIT,
                                 DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, &pRequestInfo));

    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    CHK_STATUS(setRequestHeader(pRequestInfo, (PCHAR) IMDS_TOKEN_HEADER, 0, pEc2CredentialProvider->imdsToken, 0));

    callInfo.pRequestInfo = pRequestInfo;

    CHK_STATUS(pEc2CredentialProvider->serviceCallFn(pRequestInfo, &callInfo));

    CHK_ERR(callInfo.responseDataLen > 0 && callInfo.responseDataLen <= IMDS_ROLE_NAME_LEN, STATUS_IMDS_ROLE_FETCH_FAILED,
            "Failed to fetch IAM role name from IMDS. Verify that an IAM role is attached to the EC2 instance.");

    MEMCPY(roleName, callInfo.responseData, callInfo.responseDataLen);
    roleName[callInfo.responseDataLen] = '\0';

    freeRequestInfo(&pRequestInfo);
    pRequestInfo = NULL;
    releaseCallInfo(&callInfo);
    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));

    // Step 2: Get credentials for the role
    SNPRINTF(credUrl, MAX_URI_CHAR_LEN, "http://%s%s%s", IMDS_DEFAULT_ENDPOINT, IMDS_ROLE_PATH, roleName);

    CHK_STATUS(createRequestInfo(credUrl, NULL, DEFAULT_AWS_REGION, NULL, NULL, NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, DEFAULT_USER_AGENT_NAME,
                                 pEc2CredentialProvider->connectionTimeout, pEc2CredentialProvider->completionTimeout, DEFAULT_LOW_SPEED_LIMIT,
                                 DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, &pRequestInfo));

    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    CHK_STATUS(setRequestHeader(pRequestInfo, (PCHAR) IMDS_TOKEN_HEADER, 0, pEc2CredentialProvider->imdsToken, 0));

    callInfo.pRequestInfo = pRequestInfo;

    CHK_STATUS(pEc2CredentialProvider->serviceCallFn(pRequestInfo, &callInfo));

    CHK_STATUS(parseEc2Response(pEc2CredentialProvider, &callInfo));

    DLOGI("Successfully fetched EC2 credentials for role %s", roleName);

CleanUp:

    if (STATUS_FAILED(retStatus) && pEc2CredentialProvider != NULL) {
        pEc2CredentialProvider->imdsTokenExpiration = 0;
    }

    if (pRequestInfo != NULL) {
        freeRequestInfo(&pRequestInfo);
    }

    releaseCallInfo(&callInfo);

    return retStatus;
}

STATUS parseEc2Response(PEc2CredentialProvider pEc2CredentialProvider, PCallInfo pCallInfo)
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

    CHK(pEc2CredentialProvider != NULL && pCallInfo != NULL, STATUS_NULL_ARG);

    resultLen = pCallInfo->responseDataLen;
    CHK_ERR(resultLen > 0, STATUS_IMDS_INVALID_RESPONSE_LENGTH, "IMDS response has a length of 0");
    pResponseStr = pCallInfo->responseData;

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));

    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    for (i = 1; i < (UINT32) tokenCount; i++) {
        if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "AccessKeyId")) {
            accessKeyIdLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(accessKeyIdLen <= MAX_ACCESS_KEY_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            accessKeyId = pResponseStr + tokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "SecretAccessKey")) {
            secretKeyLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(secretKeyLen <= MAX_SECRET_KEY_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            secretKey = pResponseStr + tokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Token")) {
            sessionTokenLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(sessionTokenLen <= MAX_SESSION_TOKEN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            sessionToken = pResponseStr + tokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Expiration")) {
            expirationTimestampLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(expirationTimestampLen <= MAX_EXPIRATION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            expirationTimestamp = pResponseStr + tokens[i + 1].start;
            MEMCPY(expirationTimestampStr, expirationTimestamp, expirationTimestampLen);
            expirationTimestampStr[expirationTimestampLen] = '\0';
            i++;
        }
    }

    CHK(accessKeyId != NULL && secretKey != NULL && sessionToken != NULL, STATUS_IMDS_NULL_AWS_CREDS);

    currentTime = pEc2CredentialProvider->getCurrentTimeFn(pEc2CredentialProvider->customData);
    CHK_STATUS(convertTimestampToEpoch(expirationTimestampStr, currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration));
    DLOGD("EC2 IMDS credential expiration time %" PRIu64, expiration / HUNDREDS_OF_NANOS_IN_A_SECOND);

    if (pEc2CredentialProvider->pAwsCredentials != NULL) {
        freeAwsCredentials(&pEc2CredentialProvider->pAwsCredentials);
        pEc2CredentialProvider->pAwsCredentials = NULL;
    }

    CHK_STATUS(createAwsCredentials(accessKeyId, accessKeyIdLen, secretKey, secretKeyLen, sessionToken, sessionTokenLen, expiration,
                                    &pEc2CredentialProvider->pAwsCredentials));

CleanUp:

    LEAVES();
    return retStatus;
}
