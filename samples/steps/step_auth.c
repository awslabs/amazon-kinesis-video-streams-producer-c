/**
 * step_auth.c — Step 3: Auth.
 *
 * Each configureXxxAuth() creates the client-callbacks provider wired with one
 * credential mechanism. This file contains two implementations:
 *
 *   configureStaticAuth        - long-term IAM keys, no rotation
 *   configureStsAssumeRoleAuth - rotating STS credentials via a custom
 *                                AwsCredentialProvider (reference pattern)
 *
 * THE RULE THAT THIS FILE TEACHES
 * -------------------------------
 * The SDK caches whatever credentials it is handed, together with their
 * expiration, and calls back for fresh ones only when that expiration
 * approaches. Two integration bugs follow from getting this wrong:
 *
 *   1. Handing rotating credentials to the static path with expiration
 *      MAX_UINT64. The SDK never asks again; when the token expires every
 *      API call fails and streams time out (STATUS_OPERATION_TIMED_OUT)
 *      intermittently — whenever creation coincides with a rotation gap.
 *
 *   2. Refreshing credentials in application memory on a timer. The SDK holds
 *      its own copy; mutating your copy changes nothing. Refresh must happen
 *      inside the provider's getCredentialsFn, which is the only place the
 *      SDK looks.
 *
 * The correct integration for any rotating source (STS AssumeRole shown here;
 * the same shape works for instance profiles or your own credential vending
 * service) is a custom AwsCredentialProvider: implement getCredentialsFn,
 * refresh ahead of expiry inside it, and hand the provider to
 * createCredentialProviderAuthCallbacks().
 *
 * Constraints inside getCredentialsFn: it runs on SDK threads in the API-call
 * path, so keep it fast — refresh only when the expiry window requires it,
 * never unconditionally. On refresh failure, serve still-valid cached
 * credentials rather than failing the call.
 */
#include "steps.h"
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

// ============================================================================
// configureStaticAuth — long-term IAM keys from the environment
// ============================================================================

STATUS configureStaticAuth(PAuthConfig pAuth)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR accessKey, secretKey, sessionToken, region, cacertPath;
    CHAR endpointOverride[MAX_URI_CHAR_LEN];

    CHK(pAuth != NULL, STATUS_NULL_ARG);
    MEMSET(pAuth, 0x00, SIZEOF(AuthConfig));

    CHK_ERR((accessKey = GETENV(ACCESS_KEY_ENV_VAR)) != NULL && (secretKey = GETENV(SECRET_KEY_ENV_VAR)) != NULL, STATUS_INVALID_ARG,
            "AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY must be set");
    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
    cacertPath = GETENV(CACERT_PATH_ENV_VAR);
    if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    getEndpointOverride(endpointOverride, SIZEOF(endpointOverride));

    // MAX_UINT64 expiration tells the SDK these credentials never expire.
    // That is ONLY correct for long-term IAM user keys. If the credentials in
    // your environment are temporary (STS-issued session credentials), this
    // is the wrong function — use configureStsAssumeRoleAuth() as the model.
    CHK_STATUS(createDefaultCallbacksProviderWithAwsCredentialsAndEndpointOverride(accessKey, secretKey, sessionToken, MAX_UINT64, region, cacertPath,
                                                                                   NULL, NULL, endpointOverride, &pAuth->pClientCallbacks));

CleanUp:
    return retStatus;
}

// ============================================================================
// configureStsAssumeRoleAuth — rotating STS credentials
//
// Layout of the rest of this file:
//   1. StsCredentialProvider     - the custom AwsCredentialProvider (the
//                                  integration pattern — reusable as-is)
//   2. STS AssumeRole fetch      - libcurl + manual SigV4 call to STS (the
//                                  credential source — replace with your own)
//   3. configureStsAssumeRoleAuth - wires provider + callbacks together
// ============================================================================

#define STS_REFRESH_AHEAD                60 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define STS_ASSUME_ROLE_DURATION_SECONDS 3600
#define STS_MAX_RESPONSE_SIZE            16384
#define DEFAULT_STS_SESSION_NAME         "kvs-producer-session"
#define STS_DATETIME_STRING_LEN          17
#define STS_SERVICE_NAME                 "sts"
#define STS_SESSION_NAME_MAX_LEN         256

// ----------------------------------------------------------------------------
// 1. The custom AwsCredentialProvider
//
// AwsCredentialProvider is the SDK's extension point for credential sourcing:
// a struct whose first member is the public AwsCredentialProvider with a
// getCredentialsFn. The SDK invokes getCredentialsFn before API calls and
// token rotations; whatever the function returns is what the SDK signs with.
// ----------------------------------------------------------------------------

// The credential source callback: fills in pointers to a fresh credential set
// and its expiration. Kept as a function pointer so the provider logic below
// is reusable with any source, not just STS AssumeRole.
typedef STATUS (*StsFetchFunc)(UINT64 customData, PCHAR* ppAccessKeyId, PCHAR* ppSecretKey, PCHAR* ppSessionToken, PUINT64 pExpirationEpochSeconds);

// Inputs and cached outputs for the STS AssumeRole call.
typedef struct {
    CHAR baseAccessKeyId[MAX_ACCESS_KEY_LEN + 1]; // Base credentials that sign the AssumeRole request
    CHAR baseSecretKey[MAX_SECRET_KEY_LEN + 1];
    CHAR baseSessionToken[MAX_SESSION_TOKEN_LEN + 1];
    CHAR roleArn[MAX_URI_CHAR_LEN + 1];
    CHAR sessionName[STS_SESSION_NAME_MAX_LEN];
    CHAR region[MAX_REGION_NAME_LEN + 1];
    CHAR cacertPath[MAX_PATH_LEN + 1];
    CHAR accessKeyId[MAX_ACCESS_KEY_LEN + 1]; // Latest credentials returned by STS
    CHAR secretKey[MAX_SECRET_KEY_LEN + 1];
    CHAR sessionToken[MAX_SESSION_TOKEN_LEN + 1];
    UINT64 expirationEpochSeconds;
} StsAssumeRoleContext, *PStsAssumeRoleContext;

typedef struct {
    AwsCredentialProvider credentialProvider; // MUST be the first member — the SDK casts to this
    MUTEX lock;                               // getCredentialsFn is called from multiple SDK threads
    PAwsCredentials pAwsCredentials;          // Cached credentials handed to the SDK
    UINT64 refreshAhead;                      // Refresh this far before expiration
    StsFetchFunc fetchFn;
    UINT64 fetchCustomData;
    StsAssumeRoleContext stsCtx; // Owned by the provider so freeing it frees everything
} StsCredentialProvider, *PStsCredentialProvider;

// Fetch a fresh credential set and swap it into the cache. Called with the
// provider lock held.
static STATUS stsRefreshLocked(PStsCredentialProvider pSts)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR ak = NULL, sk = NULL, token = NULL;
    UINT64 expSeconds = 0, expiration100ns;
    PAwsCredentials pNew = NULL;

    CHK_STATUS(pSts->fetchFn(pSts->fetchCustomData, &ak, &sk, &token, &expSeconds));
    CHK(ak != NULL && sk != NULL && token != NULL && expSeconds != 0, STATUS_INVALID_ARG);

    // The REAL expiration from the credential source — never a placeholder.
    // This is the value the SDK uses to decide when to call back.
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

// getCredentialsFn — the only function the SDK ever calls on the provider.
static STATUS stsGetCredentials(PAwsCredentialProvider pCredentialProvider, PAwsCredentials* ppAwsCredentials)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsCredentialProvider pSts = (PStsCredentialProvider) pCredentialProvider;
    BOOL locked = FALSE, needRefresh;
    UINT64 now;

    CHK(pSts != NULL && ppAwsCredentials != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSts->lock);
    locked = TRUE;

    // Refresh on demand, ahead of expiry — NOT on every call. This function
    // sits in the SDK's API-call path; an unconditional network call here
    // would add latency to every API call the SDK makes.
    now = GETTIME();
    needRefresh = (pSts->pAwsCredentials == NULL) || (now + pSts->refreshAhead >= pSts->pAwsCredentials->expiration);

    if (needRefresh) {
        STATUS refreshStatus = stsRefreshLocked(pSts);
        if (STATUS_FAILED(refreshStatus)) {
            // Graceful degradation: if the refresh failed but the cached
            // credentials are still valid, serve them and let a later call
            // retry the refresh. Only fail once nothing valid remains.
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

static STATUS createStsCredentialProvider(StsFetchFunc fetchFn, UINT64 refreshAhead, PAwsCredentialProvider* ppCredentialProvider)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsCredentialProvider pSts = NULL;

    CHK(fetchFn != NULL && ppCredentialProvider != NULL, STATUS_NULL_ARG);

    pSts = (PStsCredentialProvider) MEMCALLOC(1, SIZEOF(StsCredentialProvider));
    CHK(pSts != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pSts->credentialProvider.getCredentialsFn = stsGetCredentials;
    pSts->lock = MUTEX_CREATE(FALSE);
    pSts->refreshAhead = refreshAhead;
    pSts->fetchFn = fetchFn;
    pSts->fetchCustomData = (UINT64) &pSts->stsCtx;

    *ppCredentialProvider = (PAwsCredentialProvider) pSts;

CleanUp:
    if (STATUS_FAILED(retStatus) && pSts != NULL) {
        PAwsCredentialProvider p = (PAwsCredentialProvider) pSts;
        stsFreeCredentialProvider(&p);
    }
    return retStatus;
}

// ----------------------------------------------------------------------------
// 2. The credential source: STS AssumeRole over libcurl with manual SigV4
//
// Everything below this line is specific to sourcing credentials from STS.
// To integrate a different rotating source (your own vending service, a
// metadata endpoint), replace this section with your own StsFetchFunc and
// keep the provider above unchanged.
// ----------------------------------------------------------------------------

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
        return 0; // Signals curl to abort the transfer
    }

    MEMCPY(pBuf->pBuffer + pBuf->size, pData, dataSize);
    pBuf->size += (UINT32) dataSize;
    pBuf->pBuffer[pBuf->size] = '\0';

    return dataSize;
}

// Minimal XML tag extractor — sufficient for the flat AssumeRole response.
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

// SigV4-signs the AssumeRole POST (service = "sts") with the base credentials.
static STATUS signStsRequest(PCHAR pAccessKeyId, PCHAR pSecretKey, PCHAR pSessionToken, PCHAR pRegion, PCHAR pHost, PCHAR pBody, UINT32 bodyLen,
                             PCHAR pDateTimeStr, PCHAR pAuthHeader, UINT32 authHeaderSize, PCHAR pSecurityTokenHeader, UINT32 securityTokenHeaderSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR dateStr[9];
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

    CHK(pAccessKeyId != NULL && pSecretKey != NULL && pRegion != NULL && pHost != NULL && pBody != NULL && pDateTimeStr != NULL &&
            pAuthHeader != NULL,
        STATUS_NULL_ARG);

    MEMCPY(dateStr, pDateTimeStr, 8);
    dateStr[8] = '\0';

    SNPRINTF(credentialScope, SIZEOF(credentialScope), "%s/%s/%s/aws4_request", dateStr, pRegion, STS_SERVICE_NAME);
    CHK_STATUS(hexEncodedSha256Impl((PBYTE) pBody, bodyLen, bodyHash));

    if (pSessionToken != NULL && pSessionToken[0] != '\0') {
        len = SNPRINTF(canonicalRequest, SIZEOF(canonicalRequest),
                       "POST\n/\n\n"
                       "content-type:application/x-www-form-urlencoded\n"
                       "host:%s\nx-amz-date:%s\nx-amz-security-token:%s\n\n"
                       "content-type;host;x-amz-date;x-amz-security-token\n%s",
                       pHost, pDateTimeStr, pSessionToken, bodyHash);
    } else {
        len = SNPRINTF(canonicalRequest, SIZEOF(canonicalRequest),
                       "POST\n/\n\n"
                       "content-type:application/x-www-form-urlencoded\n"
                       "host:%s\nx-amz-date:%s\n\n"
                       "content-type;host;x-amz-date\n%s",
                       pHost, pDateTimeStr, bodyHash);
    }
    CHK(len > 0 && len < (INT32) SIZEOF(canonicalRequest), STATUS_BUFFER_TOO_SMALL);

    CHK_STATUS(hexEncodedSha256Impl((PBYTE) canonicalRequest, (UINT32) len, requestHash));

    len = SNPRINTF(stringToSign, SIZEOF(stringToSign), "AWS4-HMAC-SHA256\n%s\n%s\n%s", pDateTimeStr, credentialScope, requestHash);
    CHK(len > 0 && len < (INT32) SIZEOF(stringToSign), STATUS_BUFFER_TOO_SMALL);

    SNPRINTF(signingKey, SIZEOF(signingKey), "AWS4%s", pSecretKey);
    hmacLen = SIZEOF(hmac);
    CHK_STATUS(hmacSha256((PBYTE) signingKey, (UINT32) STRLEN(signingKey), (PBYTE) dateStr, 8, hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) pRegion, (UINT32) STRLEN(pRegion), hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) STS_SERVICE_NAME, (UINT32) STRLEN(STS_SERVICE_NAME), hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) "aws4_request", 12, hmac, &hmacLen));
    CHK_STATUS(hmacSha256(hmac, hmacLen, (PBYTE) stringToSign, (UINT32) STRLEN(stringToSign), hmac, &hmacLen));

    hexLen = SIZEOF(signatureHex);
    CHK_STATUS(hexEncodeCase(hmac, hmacLen, signatureHex, &hexLen, FALSE));

    if (pSessionToken != NULL && pSessionToken[0] != '\0') {
        len = SNPRINTF(pAuthHeader, authHeaderSize,
                       "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=content-type;host;x-amz-date;x-amz-security-token, Signature=%s",
                       pAccessKeyId, credentialScope, signatureHex);
    } else {
        len = SNPRINTF(pAuthHeader, authHeaderSize, "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=content-type;host;x-amz-date, Signature=%s",
                       pAccessKeyId, credentialScope, signatureHex);
    }
    CHK(len > 0 && (UINT32) len < authHeaderSize, STATUS_BUFFER_TOO_SMALL);

    if (pSessionToken != NULL && pSessionToken[0] != '\0' && pSecurityTokenHeader != NULL) {
        STRNCPY(pSecurityTokenHeader, pSessionToken, securityTokenHeaderSize - 1);
        pSecurityTokenHeader[securityTokenHeaderSize - 1] = '\0';
    } else if (pSecurityTokenHeader != NULL) {
        pSecurityTokenHeader[0] = '\0';
    }

CleanUp:
    return retStatus;
}

// The StsFetchFunc implementation: calls STS AssumeRole and returns the fresh
// credentials with their REAL expiration parsed from the response.
static STATUS stsAssumeRoleFetch(UINT64 customData, PCHAR* ppAccessKeyId, PCHAR* ppSecretKey, PCHAR* ppSessionToken, PUINT64 pExpirationEpochSeconds)
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

    SNPRINTF(stsHost, SIZEOF(stsHost), "sts.%s.amazonaws.com", pCtx->region);
    SNPRINTF(stsUrl, SIZEOF(stsUrl), "https://%s/", stsHost);

    // Build the form-encoded AssumeRole request body (RoleArn is URL-encoded).
    {
        CURL* encodeCurl = curl_easy_init();
        PCHAR encodedArn = NULL;
        CHK(encodeCurl != NULL, STATUS_NOT_ENOUGH_MEMORY);
        encodedArn = curl_easy_escape(encodeCurl, pCtx->roleArn, 0);
        if (encodedArn == NULL) {
            curl_easy_cleanup(encodeCurl);
            CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
        }
        SNPRINTF(postBody, SIZEOF(postBody), "Action=AssumeRole&Version=2011-06-15&RoleArn=%s&RoleSessionName=%s&DurationSeconds=%u", encodedArn,
                 pCtx->sessionName, STS_ASSUME_ROLE_DURATION_SECONDS);
        curl_free(encodedArn);
        curl_easy_cleanup(encodeCurl);
    }

    timeT = (time_t) (GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND);
    retSize = STRFTIME(dateTimeStr, SIZEOF(dateTimeStr), "%Y%m%dT%H%M%SZ", GMTIME_THREAD_SAFE(&timeT));
    CHK(retSize > 0, STATUS_INVALID_OPERATION);
    dateTimeStr[retSize] = '\0';

    CHK_STATUS(signStsRequest(pCtx->baseAccessKeyId, pCtx->baseSecretKey, pCtx->baseSessionToken[0] != '\0' ? pCtx->baseSessionToken : NULL,
                              pCtx->region, stsHost, postBody, (UINT32) STRLEN(postBody), dateTimeStr, authHeader, SIZEOF(authHeader),
                              securityTokenHeader, SIZEOF(securityTokenHeader)));

    curl = curl_easy_init();
    CHK(curl != NULL, STATUS_INVALID_OPERATION);

    MEMSET(responseBuffer, 0, SIZEOF(responseBuffer));
    curlBuf.pBuffer = responseBuffer;
    curlBuf.size = 0;
    curlBuf.capacity = SIZEOF(responseBuffer) - 1;

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
    // Tight timeouts: this call happens inside getCredentialsFn, which is on
    // the SDK's API-call path. A hung STS call must not hang the stream.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (pCtx->cacertPath[0] != '\0') {
        curl_easy_setopt(curl, CURLOPT_CAINFO, pCtx->cacertPath);
    }

    res = curl_easy_perform(curl);
    CHK_ERR(res == CURLE_OK, STATUS_INVALID_OPERATION, "STS AssumeRole curl call failed: %s", curl_easy_strerror(res));

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    CHK_ERR(httpStatus == 200, STATUS_INVALID_OPERATION, "STS AssumeRole returned HTTP %ld. Response: %.512s", httpStatus, responseBuffer);

    CHK_ERR(extractXmlTagValue(responseBuffer, "AccessKeyId", pCtx->accessKeyId, SIZEOF(pCtx->accessKeyId)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse AccessKeyId from STS response");
    CHK_ERR(extractXmlTagValue(responseBuffer, "SecretAccessKey", pCtx->secretKey, SIZEOF(pCtx->secretKey)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse SecretAccessKey from STS response");
    CHK_ERR(extractXmlTagValue(responseBuffer, "SessionToken", pCtx->sessionToken, SIZEOF(pCtx->sessionToken)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse SessionToken from STS response");
    CHK_ERR(extractXmlTagValue(responseBuffer, "Expiration", expirationStr, SIZEOF(expirationStr)) != NULL, STATUS_INVALID_OPERATION,
            "Failed to parse Expiration from STS response");

    CHK_STATUS(convertTimestampToEpoch(expirationStr, GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration100ns));
    pCtx->expirationEpochSeconds = expiration100ns / HUNDREDS_OF_NANOS_IN_A_SECOND;

    *ppAccessKeyId = pCtx->accessKeyId;
    *ppSecretKey = pCtx->secretKey;
    *ppSessionToken = pCtx->sessionToken;
    *pExpirationEpochSeconds = pCtx->expirationEpochSeconds;

    DLOGI("STS AssumeRole succeeded. Credentials expire at epoch %" PRIu64, pCtx->expirationEpochSeconds);

CleanUp:
    if (headers != NULL) {
        curl_slist_free_all(headers);
    }
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }

    return retStatus;
}

// ----------------------------------------------------------------------------
// 3. Wiring it together
// ----------------------------------------------------------------------------

STATUS configureStsAssumeRoleAuth(PAuthConfig pAuth)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStsCredentialProvider pSts = NULL;
    PAuthCallbacks pAuthCallbacks = NULL;
    PCHAR accessKey, secretKey, sessionToken, roleArn, sessionName, region, cacertPath;
    CHAR endpointOverride[MAX_URI_CHAR_LEN];
    UINT32 arnLen;

    CHK(pAuth != NULL, STATUS_NULL_ARG);
    MEMSET(pAuth, 0x00, SIZEOF(AuthConfig));

    // Base credentials sign the AssumeRole request itself. In production these
    // typically come from an instance profile or container credentials.
    CHK_ERR((accessKey = GETENV(ACCESS_KEY_ENV_VAR)) != NULL && (secretKey = GETENV(SECRET_KEY_ENV_VAR)) != NULL, STATUS_INVALID_ARG,
            "AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY must be set");
    sessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
    CHK_ERR((roleArn = GETENV("AWS_STS_ROLE_ARN")) != NULL, STATUS_INVALID_ARG, "AWS_STS_ROLE_ARN must be set");
    if ((sessionName = GETENV("AWS_STS_SESSION_NAME")) == NULL) {
        sessionName = (PCHAR) DEFAULT_STS_SESSION_NAME;
    }
    cacertPath = GETENV(CACERT_PATH_ENV_VAR);
    if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    // libcurl global init is not thread-safe; do it once here on the main
    // thread, never inside the fetch function (which runs on SDK threads).
    CHK(0 == curl_global_init(CURL_GLOBAL_ALL), STATUS_INVALID_OPERATION);

    // Create the provider, then populate its embedded STS context.
    CHK_STATUS(createStsCredentialProvider(stsAssumeRoleFetch, STS_REFRESH_AHEAD, (PAwsCredentialProvider*) &pSts));

    STRNCPY(pSts->stsCtx.baseAccessKeyId, accessKey, MAX_ACCESS_KEY_LEN);
    STRNCPY(pSts->stsCtx.baseSecretKey, secretKey, MAX_SECRET_KEY_LEN);
    if (sessionToken != NULL) {
        STRNCPY(pSts->stsCtx.baseSessionToken, sessionToken, MAX_SESSION_TOKEN_LEN);
    }
    STRNCPY(pSts->stsCtx.roleArn, roleArn, MAX_URI_CHAR_LEN);
    // Trim trailing whitespace that commonly sneaks into copy-pasted ARNs.
    arnLen = (UINT32) STRLEN(pSts->stsCtx.roleArn);
    while (arnLen > 0 &&
           (pSts->stsCtx.roleArn[arnLen - 1] == ' ' || pSts->stsCtx.roleArn[arnLen - 1] == '\t' || pSts->stsCtx.roleArn[arnLen - 1] == '\n')) {
        pSts->stsCtx.roleArn[--arnLen] = '\0';
    }
    STRNCPY(pSts->stsCtx.sessionName, sessionName, SIZEOF(pSts->stsCtx.sessionName) - 1);
    STRNCPY(pSts->stsCtx.region, region, MAX_REGION_NAME_LEN);
    if (cacertPath != NULL) {
        STRNCPY(pSts->stsCtx.cacertPath, cacertPath, MAX_PATH_LEN);
    }

    // Fetch the first credential set eagerly so a misconfiguration (bad role
    // ARN, missing permissions) fails here at startup, not later mid-stream.
    MUTEX_LOCK(pSts->lock);
    retStatus = stsRefreshLocked(pSts);
    MUTEX_UNLOCK(pSts->lock);
    CHK_ERR(STATUS_SUCCEEDED(retStatus), retStatus, "Initial STS AssumeRole failed — check AWS_STS_ROLE_ARN and base credentials");

    // Create the callbacks provider WITHOUT built-in credentials, then chain
    // the credential-provider auth callbacks onto it. This is the pairing that
    // makes rotation work: the SDK pulls fresh credentials from the provider
    // instead of holding a frozen copy.
    getEndpointOverride(endpointOverride, SIZEOF(endpointOverride));
    CHK_STATUS(createAbstractDefaultCallbacksProvider(DEFAULT_CALLBACK_CHAIN_COUNT, API_CALL_CACHE_TYPE_ALL, ENDPOINT_UPDATE_PERIOD_SENTINEL_VALUE,
                                                      region, endpointOverride, cacertPath, NULL, NULL, &pAuth->pClientCallbacks));
    CHK_STATUS(createCredentialProviderAuthCallbacks(pAuth->pClientCallbacks, (PAwsCredentialProvider) pSts, &pAuthCallbacks));

    // The AuthConfig owns the provider; freeAuth() releases it after the
    // callbacks provider (which references it) has been freed.
    pAuth->pCredentialProvider = (PAwsCredentialProvider) pSts;
    pSts = NULL;

CleanUp:
    if (pSts != NULL) {
        PAwsCredentialProvider p = (PAwsCredentialProvider) pSts;
        stsFreeCredentialProvider(&p);
    }
    if (STATUS_FAILED(retStatus) && pAuth != NULL && pAuth->pClientCallbacks != NULL) {
        freeCallbacksProvider(&pAuth->pClientCallbacks);
    }
    return retStatus;
}

STATUS freeAuth(PAuthConfig pAuth)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pAuth != NULL, STATUS_NULL_ARG);

    // Order matters: the auth callbacks inside the callbacks provider hold a
    // reference to the credential provider, so free the callbacks first.
    if (pAuth->pClientCallbacks != NULL) {
        freeCallbacksProvider(&pAuth->pClientCallbacks);
    }
    if (pAuth->pCredentialProvider != NULL) {
        stsFreeCredentialProvider(&pAuth->pCredentialProvider);
    }

CleanUp:
    return retStatus;
}
