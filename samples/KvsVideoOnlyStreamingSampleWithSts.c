/*
 * KVS Video Only Streaming Sample with STS AssumeRole Rotating Credentials
 *
 * This sample demonstrates how to use rotating STS credentials with the
 * KVS Producer C SDK. It calls STS AssumeRole via libcurl with SigV4 signing
 * to obtain temporary credentials, and refreshes them on demand before expiry.
 *
 * Environment variables:
 *   AWS_ACCESS_KEY_ID       - Base credentials (long-term or from instance profile)
 *   AWS_SECRET_ACCESS_KEY   - Base secret key
 *   AWS_SESSION_TOKEN       - (Optional) session token for base credentials
 *   AWS_STS_ROLE_ARN        - ARN of the role to assume
 *   AWS_STS_SESSION_NAME    - (Optional) session name, defaults to "kvs-producer-session"
 *   AWS_DEFAULT_REGION      - (Optional) defaults to us-west-2
 *
 * Usage:
 *   ./kvsVideoOnlyStreamingSampleWithSts <stream_name> [duration_in_seconds]
 */

#include "Samples.h"
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#define DEFAULT_RETENTION_PERIOD          2 * HUNDREDS_OF_NANOS_IN_AN_HOUR
#define DEFAULT_BUFFER_DURATION           120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_KEY_FRAME_INTERVAL        45
#define DEFAULT_FPS_VALUE                 25
#define DEFAULT_STREAM_DURATION           120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_STORAGE_SIZE              20 * 1024 * 1024
#define RECORDED_FRAME_AVG_BITRATE_BIT_PS 3800000
#define NUMBER_OF_FRAME_FILES             403
#define STS_REFRESH_AHEAD                 840 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define STS_ASSUME_ROLE_DURATION_SECONDS  900
// Override credential expiration to force rotation every ~60 seconds for testing
#define STS_TEST_EXPIRATION_OVERRIDE_SECONDS 60
#define STS_MAX_RESPONSE_SIZE             16384
#define STS_CONNECTION_TIMEOUT            5 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define STS_COMPLETION_TIMEOUT            10 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_STS_SESSION_NAME          "kvs-producer-session"
#define STS_DATETIME_STRING_LEN           17 // YYYYMMDDTHHMMSSz + null

// ============================================================================
// STS AssumeRole context
// ============================================================================
typedef struct {
    CHAR baseAccessKeyId[MAX_ACCESS_KEY_LEN + 1];
    CHAR baseSecretKey[MAX_SECRET_KEY_LEN + 1];
    CHAR baseSessionToken[MAX_SESSION_TOKEN_LEN + 1];
    CHAR roleArn[MAX_URI_CHAR_LEN + 1];
    CHAR sessionName[256];
    CHAR region[MAX_REGION_NAME_LEN + 1];
    CHAR cacertPath[MAX_PATH_LEN + 1];
    // Buffers for returned credentials
    CHAR accessKeyId[MAX_ACCESS_KEY_LEN + 1];
    CHAR secretKey[MAX_SECRET_KEY_LEN + 1];
    CHAR sessionToken[MAX_SESSION_TOKEN_LEN + 1];
    UINT64 expirationEpochSeconds;
} StsAssumeRoleContext, *PStsAssumeRoleContext;

// ============================================================================
// STS credential provider
// ============================================================================
typedef STATUS (*StsFetchFunc)(UINT64 customData, PCHAR* ppAccessKeyId, PCHAR* ppSecretKey, PCHAR* ppSessionToken, PUINT64 pExpirationEpochSeconds);

typedef struct __StsCredentialProvider {
    AwsCredentialProvider credentialProvider;
    MUTEX lock;
    PAwsCredentials pAwsCredentials;
    UINT64 refreshAhead;
    StsFetchFunc stsFetchFn;
    UINT64 customData;
} StsCredentialProvider, *PStsCredentialProvider;

// ============================================================================
// Curl write callback for STS response
// ============================================================================
typedef struct {
    PCHAR pBuffer;
    UINT32 size;
    UINT32 capacity;
} CurlResponseBuffer, *PCurlResponseBuffer;

static SIZE_T stsWriteCallback(PCHAR pData, SIZE_T size, SIZE_T nmemb, PVOID userData)
{
    PCurlResponseBuffer pBuf = (PCurlResponseBuffer) userData;
    SIZE_T dataSize = size * nmemb;

    if (pBuf->size + dataSize >= pBuf->capacity) {
        return 0; // overflow protection
    }

    MEMCPY(pBuf->pBuffer + pBuf->size, pData, dataSize);
    pBuf->size += (UINT32) dataSize;
    pBuf->pBuffer[pBuf->size] = '\0';

    return dataSize;
}

// ============================================================================
// Simple XML tag value extractor
// ============================================================================
static PCHAR extractXmlTagValue(PCHAR xml, PCHAR tagName, PCHAR outBuffer, UINT32 outBufferSize)
{
    CHAR openTag[256], closeTag[256];
    PCHAR start, end;
    UINT32 len;

    SNPRINTF(openTag, SIZEOF(openTag), "<%s>", tagName);
    SNPRINTF(closeTag, SIZEOF(closeTag), "</%s>", tagName);

    start = STRSTR(xml, openTag);
    if (start == NULL) {
        return NULL;
    }
    start += STRLEN(openTag);

    end = STRSTR(start, closeTag);
    if (end == NULL) {
        return NULL;
    }

    len = (UINT32) (end - start);
    if (len >= outBufferSize) {
        return NULL;
    }

    MEMCPY(outBuffer, start, len);
    outBuffer[len] = '\0';

    return outBuffer;
}

// ============================================================================
// SigV4 helper: HMAC-SHA256 (using OpenSSL directly)
// ============================================================================
static STATUS hmacSha256(PBYTE pKey, UINT32 keyLen, PBYTE pMessage, UINT32 messageLen, PBYTE pOutput, PUINT32 pOutputLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    unsigned int hmacLen = 0;
    CHK(pKey != NULL && pMessage != NULL && pOutput != NULL && pOutputLen != NULL, STATUS_NULL_ARG);
    CHK(NULL != HMAC(EVP_sha256(), pKey, (INT32) keyLen, pMessage, messageLen, pOutput, &hmacLen), STATUS_HMAC_GENERATION_ERROR);
    *pOutputLen = (UINT32) hmacLen;
CleanUp:
    return retStatus;
}

// ============================================================================
// SigV4 helper: hex-encoded SHA256
// ============================================================================
static STATUS hexEncodedSha256Impl(PBYTE pData, UINT32 dataLen, PCHAR pOutputHex)
{
    STATUS retStatus = STATUS_SUCCESS;
    BYTE digest[SHA256_DIGEST_LENGTH];
    UINT32 hexLen = SHA256_DIGEST_LENGTH * 2 + 1;
    CHK(pData != NULL && pOutputHex != NULL, STATUS_NULL_ARG);
    SHA256(pData, dataLen, digest);
    CHK_STATUS(hexEncodeCase(digest, SHA256_DIGEST_LENGTH, pOutputHex, &hexLen, FALSE));
CleanUp:
    return retStatus;
}

// ============================================================================
// SigV4 signing for STS (service = "sts")
// ============================================================================
#define STS_SERVICE_NAME "sts"

static STATUS signStsRequest(PCHAR pAccessKeyId, PCHAR pSecretKey, PCHAR pSessionToken, PCHAR pRegion, PCHAR pHost,
                             PCHAR pBody, UINT32 bodyLen, PCHAR pDateTimeStr, PCHAR pAuthHeader, UINT32 authHeaderSize,
                             PCHAR pSecurityTokenHeader, UINT32 securityTokenHeaderSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR dateStr[9]; // YYYYMMDD
    CHAR credentialScope[256];
    CHAR canonicalRequest[4096];
    CHAR stringToSign[4096];
    CHAR bodyHash[SHA256_DIGEST_LENGTH * 2 + 1];
    CHAR requestHash[SHA256_DIGEST_LENGTH * 2 + 1];
    BYTE hmac[SHA256_DIGEST_LENGTH];
    CHAR signatureHex[SHA256_DIGEST_LENGTH * 2 + 1];
    CHAR signingKey[256];
    UINT32 hmacLen = SIZEOF(hmac), hexLen;
    INT32 len;

    CHK(pAccessKeyId != NULL && pSecretKey != NULL && pRegion != NULL && pHost != NULL && pBody != NULL &&
        pDateTimeStr != NULL && pAuthHeader != NULL, STATUS_NULL_ARG);

    // Date string is first 8 chars of datetime
    MEMCPY(dateStr, pDateTimeStr, 8);
    dateStr[8] = '\0';

    // Credential scope: date/region/sts/aws4_request
    SNPRINTF(credentialScope, SIZEOF(credentialScope), "%s/%s/%s/aws4_request", dateStr, pRegion, STS_SERVICE_NAME);

    // Hash the body
    CHK_STATUS(hexEncodedSha256Impl((PBYTE) pBody, bodyLen, bodyHash));

    // Canonical request - include x-amz-security-token if present
    if (pSessionToken != NULL && pSessionToken[0] != '\0') {
        len = SNPRINTF(canonicalRequest, SIZEOF(canonicalRequest),
                       "POST\n"       // method
                       "/\n"          // canonical URI
                       "\n"           // canonical query string (empty)
                       "content-type:application/x-www-form-urlencoded\n"
                       "host:%s\n"
                       "x-amz-date:%s\n"
                       "x-amz-security-token:%s\n"
                       "\n"           // end of headers
                       "content-type;host;x-amz-date;x-amz-security-token\n" // signed headers
                       "%s",          // hashed payload
                       pHost, pDateTimeStr, pSessionToken, bodyHash);
    } else {
        len = SNPRINTF(canonicalRequest, SIZEOF(canonicalRequest),
                       "POST\n"       // method
                       "/\n"          // canonical URI
                       "\n"           // canonical query string (empty)
                       "content-type:application/x-www-form-urlencoded\n"
                       "host:%s\n"
                       "x-amz-date:%s\n"
                       "\n"           // end of headers
                       "content-type;host;x-amz-date\n" // signed headers
                       "%s",          // hashed payload
                       pHost, pDateTimeStr, bodyHash);
    }
    CHK(len > 0 && len < (INT32) SIZEOF(canonicalRequest), STATUS_BUFFER_TOO_SMALL);

    // Hash the canonical request
    CHK_STATUS(hexEncodedSha256Impl((PBYTE) canonicalRequest, (UINT32) len, requestHash));

    // String to sign
    len = SNPRINTF(stringToSign, SIZEOF(stringToSign), "AWS4-HMAC-SHA256\n%s\n%s\n%s", pDateTimeStr, credentialScope, requestHash);
    CHK(len > 0 && len < (INT32) SIZEOF(stringToSign), STATUS_BUFFER_TOO_SMALL);

    // Derive signing key: HMAC(HMAC(HMAC(HMAC("AWS4"+secret, date), region), service), "aws4_request")
    SNPRINTF(signingKey, SIZEOF(signingKey), "AWS4%s", pSecretKey);
    hmacLen = SIZEOF(hmac);
    CHK_STATUS(hmacSha256((PBYTE) signingKey, (UINT32) STRLEN(signingKey), (PBYTE) dateStr, 8, hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) pRegion, (UINT32) STRLEN(pRegion), hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) STS_SERVICE_NAME, (UINT32) STRLEN(STS_SERVICE_NAME), hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) "aws4_request", 12, hmac, &hmacLen));

    // Sign the string to sign
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) stringToSign, (UINT32) STRLEN(stringToSign), hmac, &hmacLen));

    // Hex encode the signature
    hexLen = SIZEOF(signatureHex);
    CHK_STATUS(hexEncodeCase(hmac, hmacLen, signatureHex, &hexLen, FALSE));

    // Build the Authorization header
    if (pSessionToken != NULL && pSessionToken[0] != '\0') {
        len = SNPRINTF(pAuthHeader, authHeaderSize,
                       "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=content-type;host;x-amz-date;x-amz-security-token, Signature=%s",
                       pAccessKeyId, credentialScope, signatureHex);
    } else {
        len = SNPRINTF(pAuthHeader, authHeaderSize,
                       "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=content-type;host;x-amz-date, Signature=%s",
                       pAccessKeyId, credentialScope, signatureHex);
    }
    CHK(len > 0 && (UINT32) len < authHeaderSize, STATUS_BUFFER_TOO_SMALL);

    // Security token header
    if (pSessionToken != NULL && pSessionToken[0] != '\0' && pSecurityTokenHeader != NULL) {
        STRNCPY(pSecurityTokenHeader, pSessionToken, securityTokenHeaderSize - 1);
        pSecurityTokenHeader[securityTokenHeaderSize - 1] = '\0';
    } else if (pSecurityTokenHeader != NULL) {
        pSecurityTokenHeader[0] = '\0';
    }

CleanUp:
    return retStatus;
}

// ============================================================================
// STS AssumeRole fetch function using libcurl + manual SigV4 (service=sts)
// ============================================================================
static STATUS stsAssumeRoleFetch(UINT64 customData, PCHAR* ppAccessKeyId, PCHAR* ppSecretKey, PCHAR* ppSessionToken,
                                 PUINT64 pExpirationEpochSeconds)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsAssumeRoleContext pCtx = (PStsAssumeRoleContext) customData;
    CURL* curl = NULL;
    CURLcode res;
    struct curl_slist* headers = NULL;
    CHAR stsHost[256];
    CHAR stsUrl[MAX_URI_CHAR_LEN + 1];
    CHAR postBody[MAX_URI_CHAR_LEN + 1];
    CHAR responseBuffer[STS_MAX_RESPONSE_SIZE];
    CurlResponseBuffer curlBuf;
    CHAR expirationStr[128];
    UINT64 expiration100ns;
    CHAR dateTimeStr[STS_DATETIME_STRING_LEN];
    CHAR authHeader[2048];
    CHAR securityTokenHeader[MAX_SESSION_TOKEN_LEN + 1];
    CHAR headerBuf[2048 + 64];
    long httpStatus = 0;
    time_t timeT;
    SIZE_T retSize;

    CHK(pCtx != NULL && ppAccessKeyId != NULL && ppSecretKey != NULL && ppSessionToken != NULL && pExpirationEpochSeconds != NULL, STATUS_NULL_ARG);

    // Build the STS endpoint
    SNPRINTF(stsHost, SIZEOF(stsHost), "sts.%s.amazonaws.com", pCtx->region);
    SNPRINTF(stsUrl, SIZEOF(stsUrl), "https://%s/", stsHost);

    // Build the POST body for AssumeRole (URL-encode the role ARN using libcurl)
    {
        CURL* encodeCurl = curl_easy_init();
        PCHAR encodedArn = NULL;
        CHK(encodeCurl != NULL, STATUS_NOT_ENOUGH_MEMORY);
        encodedArn = curl_easy_escape(encodeCurl, pCtx->roleArn, 0);
        CHK(encodedArn != NULL, STATUS_NOT_ENOUGH_MEMORY);
        SNPRINTF(postBody, SIZEOF(postBody), "Action=AssumeRole&Version=2011-06-15&RoleArn=%s&RoleSessionName=%s&DurationSeconds=%u", encodedArn,
                 pCtx->sessionName, STS_ASSUME_ROLE_DURATION_SECONDS);
        curl_free(encodedArn);
        curl_easy_cleanup(encodeCurl);
    }

    // Generate datetime string (UTC)
    timeT = (time_t) (GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND);
    retSize = STRFTIME(dateTimeStr, SIZEOF(dateTimeStr), "%Y%m%dT%H%M%SZ", GMTIME_THREAD_SAFE(&timeT));
    CHK(retSize > 0, STATUS_INVALID_OPERATION);
    dateTimeStr[retSize] = '\0';

    // Sign the request with service="sts"
    CHK_STATUS(signStsRequest(pCtx->baseAccessKeyId, pCtx->baseSecretKey,
                              pCtx->baseSessionToken[0] != '\0' ? pCtx->baseSessionToken : NULL,
                              pCtx->region, stsHost, postBody, (UINT32) STRLEN(postBody),
                              dateTimeStr, authHeader, SIZEOF(authHeader),
                              securityTokenHeader, SIZEOF(securityTokenHeader)));

    // Make the curl call
    CHK(0 == curl_global_init(CURL_GLOBAL_ALL), STATUS_INVALID_OPERATION);
    curl = curl_easy_init();
    CHK(curl != NULL, STATUS_INVALID_OPERATION);

    // Set up response buffer
    MEMSET(responseBuffer, 0, SIZEOF(responseBuffer));
    curlBuf.pBuffer = responseBuffer;
    curlBuf.size = 0;
    curlBuf.capacity = SIZEOF(responseBuffer) - 1;

    // Build headers
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    SNPRINTF(headerBuf, SIZEOF(headerBuf), "Host: %s", stsHost);
    headers = curl_slist_append(headers, headerBuf);
    SNPRINTF(headerBuf, SIZEOF(headerBuf), "X-Amz-Date: %s", dateTimeStr);
    headers = curl_slist_append(headers, headerBuf);
    SNPRINTF(headerBuf, SIZEOF(headerBuf), "Authorization: %s", authHeader);
    headers = curl_slist_append(headers, headerBuf);
    if (securityTokenHeader[0] != '\0') {
        SNPRINTF(headerBuf, SIZEOF(headerBuf), "X-Amz-Security-Token: %s", securityTokenHeader);
        headers = curl_slist_append(headers, headerBuf);
    }

    curl_easy_setopt(curl, CURLOPT_URL, stsUrl);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stsWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlBuf);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (pCtx->cacertPath[0] != '\0') {
        curl_easy_setopt(curl, CURLOPT_CAINFO, pCtx->cacertPath);
    }

    res = curl_easy_perform(curl);
    CHK_ERR(res == CURLE_OK, STATUS_INVALID_OPERATION, "STS AssumeRole curl call failed: %s", curl_easy_strerror(res));

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    CHK_ERR(httpStatus == 200, STATUS_INVALID_OPERATION, "STS AssumeRole returned HTTP %ld. Response: %.512s", httpStatus, responseBuffer);

    // Parse the XML response to extract credentials
    CHK_ERR(extractXmlTagValue(responseBuffer, "AccessKeyId", pCtx->accessKeyId, SIZEOF(pCtx->accessKeyId)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse AccessKeyId from STS response");
    CHK_ERR(extractXmlTagValue(responseBuffer, "SecretAccessKey", pCtx->secretKey, SIZEOF(pCtx->secretKey)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse SecretAccessKey from STS response");
    CHK_ERR(extractXmlTagValue(responseBuffer, "SessionToken", pCtx->sessionToken, SIZEOF(pCtx->sessionToken)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse SessionToken from STS response");
    CHK_ERR(extractXmlTagValue(responseBuffer, "Expiration", expirationStr, SIZEOF(expirationStr)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse Expiration from STS response");

    // Convert ISO 8601 expiration to epoch seconds using SDK utility
    CHK_STATUS(convertTimestampToEpoch(expirationStr, GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration100ns));
    pCtx->expirationEpochSeconds = expiration100ns / HUNDREDS_OF_NANOS_IN_A_SECOND;

#ifdef STS_TEST_EXPIRATION_OVERRIDE_SECONDS
    // Override expiration to force frequent rotation for testing
    pCtx->expirationEpochSeconds = (GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND) + STS_TEST_EXPIRATION_OVERRIDE_SECONDS;
    DLOGW("[TEST] Overriding credential expiration to %llu (now + %u seconds)", pCtx->expirationEpochSeconds, STS_TEST_EXPIRATION_OVERRIDE_SECONDS);
#endif

    *ppAccessKeyId = pCtx->accessKeyId;
    *ppSecretKey = pCtx->secretKey;
    *ppSessionToken = pCtx->sessionToken;
    *pExpirationEpochSeconds = pCtx->expirationEpochSeconds;

    DLOGI("STS AssumeRole succeeded. Credentials expire at epoch %llu", pCtx->expirationEpochSeconds);

CleanUp:
    if (headers != NULL) {
        curl_slist_free_all(headers);
    }
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }

    return retStatus;
}

// ============================================================================
// STS credential provider implementation
// ============================================================================
static STATUS stsRefreshLocked(PStsCredentialProvider pSts)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR ak = NULL, sk = NULL, token = NULL;
    UINT64 expSeconds = 0, expiration100ns;
    PAwsCredentials pNew = NULL;

    CHK_STATUS(pSts->stsFetchFn(pSts->customData, &ak, &sk, &token, &expSeconds));
    CHK(ak != NULL && sk != NULL && token != NULL && expSeconds != 0, STATUS_INVALID_ARG);

    expiration100ns = expSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
    CHK_STATUS(createAwsCredentials(ak, 0, sk, 0, token, 0, expiration100ns, &pNew));

    if (pSts->pAwsCredentials != NULL) {
        freeAwsCredentials(&pSts->pAwsCredentials);
    }
    pSts->pAwsCredentials = pNew;
    pNew = NULL;

CleanUp:
    if (pNew != NULL) {
        freeAwsCredentials(&pNew);
    }
    return retStatus;
}

static STATUS stsGetCredentials(PAwsCredentialProvider pCredentialProvider, PAwsCredentials* ppAwsCredentials)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsCredentialProvider pSts = (PStsCredentialProvider) pCredentialProvider;
    BOOL locked = FALSE, needRefresh;
    UINT64 now;

    CHK(pSts != NULL && ppAwsCredentials != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSts->lock);
    locked = TRUE;

    now = GETTIME();
    needRefresh = (pSts->pAwsCredentials == NULL) || (now + pSts->refreshAhead >= pSts->pAwsCredentials->expiration);

    if (needRefresh) {
        STATUS refreshStatus = stsRefreshLocked(pSts);
        if (STATUS_FAILED(refreshStatus)) {
            // If refresh fails but cached creds are still valid, use them
            CHK(pSts->pAwsCredentials != NULL && now < pSts->pAwsCredentials->expiration, refreshStatus);
            DLOGW("STS refresh failed (0x%08x); serving still-valid cached credentials", refreshStatus);
        }
    }

    CHK(pSts->pAwsCredentials != NULL && now < pSts->pAwsCredentials->expiration, STATUS_INVALID_OPERATION);
    *ppAwsCredentials = pSts->pAwsCredentials;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSts->lock);
    }
    return retStatus;
}

static STATUS stsFreeCredentialProvider(PAwsCredentialProvider* ppCredentialProvider)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsCredentialProvider pSts;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);
    pSts = (PStsCredentialProvider) *ppCredentialProvider;
    CHK(pSts != NULL, retStatus);

    if (IS_VALID_MUTEX_VALUE(pSts->lock)) {
        MUTEX_FREE(pSts->lock);
    }
    if (pSts->pAwsCredentials != NULL) {
        freeAwsCredentials(&pSts->pAwsCredentials);
    }
    MEMFREE(pSts);
    *ppCredentialProvider = NULL;

CleanUp:
    return retStatus;
}

static STATUS createStsCredentialProvider(StsFetchFunc stsFetchFn, UINT64 customData, UINT64 refreshAhead,
                                          PAwsCredentialProvider* ppCredentialProvider)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsCredentialProvider pSts = NULL;

    CHK(stsFetchFn != NULL && ppCredentialProvider != NULL, STATUS_NULL_ARG);

    pSts = (PStsCredentialProvider) MEMCALLOC(1, SIZEOF(StsCredentialProvider));
    CHK(pSts != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pSts->credentialProvider.getCredentialsFn = stsGetCredentials;
    pSts->lock = MUTEX_CREATE(FALSE);
    pSts->refreshAhead = refreshAhead;
    pSts->stsFetchFn = stsFetchFn;
    pSts->customData = customData;

    // Eager first fetch so credential problems surface immediately
    MUTEX_LOCK(pSts->lock);
    retStatus = stsRefreshLocked(pSts);
    MUTEX_UNLOCK(pSts->lock);
    CHK_STATUS(retStatus);

    *ppCredentialProvider = (PAwsCredentialProvider) pSts;

CleanUp:
    if (STATUS_FAILED(retStatus) && pSts != NULL) {
        PAwsCredentialProvider p = (PAwsCredentialProvider) pSts;
        stsFreeCredentialProvider(&p);
    }
    return retStatus;
}

// ============================================================================
// Frame reading helper
// ============================================================================
VOID defaultThreadSleep(UINT64);

static STATUS readFrameData(PFrame pFrame, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 index;
    UINT64 size;

    CHK(pFrame != NULL, STATUS_NULL_ARG);

    index = pFrame->index % NUMBER_OF_FRAME_FILES + 1;
    SNPRINTF(filePath, MAX_PATH_LEN, "%s/h264SampleFrames/frame-%03d.h264", frameFilePath, index);
    size = pFrame->size;

    CHK_STATUS(readFile(filePath, TRUE, NULL, &size));
    CHK_STATUS(readFile(filePath, TRUE, pFrame->frameData, &size));

    pFrame->size = (UINT32) size;

    if (pFrame->flags == FRAME_FLAG_KEY_FRAME) {
        DLOGD("Key frame file %s, size %" PRIu64, filePath, pFrame->size);
    }

CleanUp:
    return retStatus;
}

// ============================================================================
// Main
// ============================================================================
INT32 main(INT32 argc, CHAR* argv[])
{
    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
    PAuthCallbacks pAuthCallbacks = NULL;
    PAwsCredentialProvider pCredentialProvider = NULL;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR streamName = NULL, region = NULL, cacertPath = NULL, accessKey = NULL, secretKey = NULL, sessionToken = NULL;
    PCHAR roleArn = NULL, sessionName = NULL;
    CHAR frameFilePath[MAX_PATH_LEN + 1];
    Frame frame, eofr = EOFR_FRAME_INITIALIZER;
    BYTE frameBuffer[200000];
    UINT32 frameSize = SIZEOF(frameBuffer), frameIndex = 0, fileIndex = 0;
    UINT64 streamStopTime, streamingDuration = DEFAULT_STREAM_DURATION;
    DOUBLE startUpLatency;
    BOOL firstFrame = TRUE;
    UINT64 startTime;
    StsAssumeRoleContext stsCtx;
    CHAR endpointOverride[MAX_URI_CHAR_LEN];

    MEMSET(&stsCtx, 0, SIZEOF(stsCtx));

    if (argc < 2) {
        DLOGE("Usage: %s <stream_name> [video_codec] [duration_in_seconds] [sample_location]\n"
              "  Required env vars: AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_STS_ROLE_ARN\n"
              "  Optional env vars: AWS_SESSION_TOKEN, AWS_STS_SESSION_NAME, AWS_DEFAULT_REGION\n",
              argv[0]);
        CHK(FALSE, STATUS_INVALID_ARG);
    }

    streamName = argv[1];

    // Get base credentials for signing the AssumeRole request
    accessKey = GETENV(ACCESS_KEY_ENV_VAR);
    secretKey = GETENV(SECRET_KEY_ENV_VAR);
    CHK_ERR(accessKey != NULL && secretKey != NULL, STATUS_INVALID_ARG, "AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY must be set");

    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
    roleArn = GETENV("AWS_STS_ROLE_ARN");
    CHK_ERR(roleArn != NULL, STATUS_INVALID_ARG, "AWS_STS_ROLE_ARN must be set");

    sessionName = GETENV("AWS_STS_SESSION_NAME");
    if (sessionName == NULL) {
        sessionName = (PCHAR) DEFAULT_STS_SESSION_NAME;
    }

    cacertPath = GETENV(CACERT_PATH_ENV_VAR);
    if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    // argv[2] = video codec (ignored, always h264 in this sample)

    if (argc >= 4 && !IS_EMPTY_STRING(argv[3])) {
        CHK_STATUS(STRTOUI64(argv[3], NULL, 10, &streamingDuration));
        streamingDuration *= HUNDREDS_OF_NANOS_IN_A_SECOND;
    }

    MEMSET(frameFilePath, 0x00, MAX_PATH_LEN + 1);
    if (argc >= 5 && !IS_EMPTY_STRING(argv[4])) {
        STRNCPY(frameFilePath, argv[4], MAX_PATH_LEN);
    } else {
        STRCPY(frameFilePath, (PCHAR) "../samples/");
    }

    // Populate the STS context
    STRNCPY(stsCtx.baseAccessKeyId, accessKey, MAX_ACCESS_KEY_LEN);
    STRNCPY(stsCtx.baseSecretKey, secretKey, MAX_SECRET_KEY_LEN);
    if (sessionToken != NULL) {
        STRNCPY(stsCtx.baseSessionToken, sessionToken, MAX_SESSION_TOKEN_LEN);
    }
    STRNCPY(stsCtx.roleArn, roleArn, MAX_URI_CHAR_LEN);
    // Trim trailing whitespace from role ARN
    {
        UINT32 arnLen = (UINT32) STRLEN(stsCtx.roleArn);
        while (arnLen > 0 && (stsCtx.roleArn[arnLen - 1] == ' ' || stsCtx.roleArn[arnLen - 1] == '\t' || stsCtx.roleArn[arnLen - 1] == '\n')) {
            stsCtx.roleArn[--arnLen] = '\0';
        }
    }
    STRNCPY(stsCtx.sessionName, sessionName, SIZEOF(stsCtx.sessionName) - 1);
    STRNCPY(stsCtx.region, region, MAX_REGION_NAME_LEN);
    if (cacertPath != NULL) {
        STRNCPY(stsCtx.cacertPath, cacertPath, MAX_PATH_LEN);
    }

    streamStopTime = GETTIME() + streamingDuration;

    // Create device info
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    pDeviceInfo->clientInfo.loggerLogLevel = getSampleLogLevel();
    pDeviceInfo->storageInfo.storageSize = DEFAULT_STORAGE_SIZE;

    // Create stream info
    CHK_STATUS(createRealtimeVideoStreamInfoProvider(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION, &pStreamInfo));
    CHK_STATUS(setStreamInfoBasedOnStorageSize(DEFAULT_STORAGE_SIZE, RECORDED_FRAME_AVG_BITRATE_BIT_PS, 1, pStreamInfo));

    startTime = GETTIME();

    // Create the STS rotating credential provider (does an eager first AssumeRole)
    DLOGI("Creating STS credential provider with role: %s", roleArn);
    CHK_STATUS(createStsCredentialProvider(stsAssumeRoleFetch, (UINT64) &stsCtx, STS_REFRESH_AHEAD, &pCredentialProvider));

    // Create callbacks provider without built-in credentials (we use our own STS provider)
    getEndpointOverride(endpointOverride, SIZEOF(endpointOverride));
    CHK_STATUS(createAbstractDefaultCallbacksProvider(DEFAULT_CALLBACK_CHAIN_COUNT, API_CALL_CACHE_TYPE_ALL, ENDPOINT_UPDATE_PERIOD_SENTINEL_VALUE,
                                                      region, endpointOverride, cacertPath, NULL, NULL, &pClientCallbacks));
    CHK_STATUS(createCredentialProviderAuthCallbacks(pClientCallbacks, pCredentialProvider, &pAuthCallbacks));
    CHK_STATUS(createContinuousRetryStreamCallbacks(pClientCallbacks, &pStreamCallbacks));

    if (NULL != GETENV(ENABLE_FILE_LOGGING)) {
        if ((retStatus = addFileLoggerPlatformCallbacksProvider(pClientCallbacks, FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES,
                                                                (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH, TRUE) != STATUS_SUCCESS)) {
            DLOGE("File logging enable option failed with 0x%08x error code\n", retStatus);
        }
    }

    CHK_STATUS(createKinesisVideoClient(pDeviceInfo, pClientCallbacks, &clientHandle));
    CHK_STATUS(createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    // Setup frame
    MEMSET(frameBuffer, 0x00, frameSize);
    frame.frameData = frameBuffer;
    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE;
    frame.decodingTs = GETTIME();
    frame.presentationTs = frame.decodingTs;

    while (GETTIME() < streamStopTime) {
        frame.index = frameIndex;
        frame.flags = fileIndex % DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;
        frame.size = SIZEOF(frameBuffer);

        CHK_STATUS(readFrameData(&frame, frameFilePath));

        if (frame.flags == FRAME_FLAG_KEY_FRAME && !firstFrame) {
            putKinesisVideoFrame(streamHandle, &eofr);
        }

        CHK_STATUS(putKinesisVideoFrame(streamHandle, &frame));
        if (firstFrame) {
            startUpLatency = (DOUBLE) (GETTIME() - startTime) / (DOUBLE) HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
            DLOGD("Start up latency: %lf ms", startUpLatency);
            firstFrame = FALSE;
        }
        defaultThreadSleep(frame.duration);

        frame.decodingTs += frame.duration;
        frame.presentationTs = frame.decodingTs;
        frameIndex++;
        fileIndex++;
        fileIndex = fileIndex % NUMBER_OF_FRAME_FILES;
    }

    putKinesisVideoFrame(streamHandle, &eofr);

    CHK_STATUS(stopKinesisVideoStreamSync(streamHandle));
    CHK_STATUS(freeKinesisVideoStream(&streamHandle));
    CHK_STATUS(freeKinesisVideoClient(&clientHandle));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed with status 0x%08x", retStatus);
    }

    if (pDeviceInfo != NULL) {
        freeDeviceInfo(&pDeviceInfo);
    }

    if (pStreamInfo != NULL) {
        freeStreamInfoProvider(&pStreamInfo);
    }

    if (IS_VALID_STREAM_HANDLE(streamHandle)) {
        freeKinesisVideoStream(&streamHandle);
    }

    if (IS_VALID_CLIENT_HANDLE(clientHandle)) {
        freeKinesisVideoClient(&clientHandle);
    }

    if (pClientCallbacks != NULL) {
        freeCallbacksProvider(&pClientCallbacks);
    }

    if (pCredentialProvider != NULL) {
        stsFreeCredentialProvider(&pCredentialProvider);
    }

    return (INT32) retStatus;
}
