/**
 * Helper functionality for CURL
 */
#define LOG_CLASS "CurlCall"
#include "../Include_i.h"

#if defined(KVS_USE_OPENSSL) || defined(KVS_USE_AWS_LC)
#include <openssl/ssl.h>
#endif

/**
 * curl header callback used by blockingCurlCall solely to surface TLS negotiation details.
 * The HTTP status line ("HTTP/1.1 200 OK") arrives exactly once per response, which makes it
 * a convenient one-shot hook while the TLS session pointer is still valid.
 */
static SIZE_T tlsInfoHeaderCallback(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData)
{
    SIZE_T dataSize = size * numItems;
    if (customData != NULL && dataSize >= 5 && 0 == STRNCMP(pBuffer, "HTTP/", 5)) {
        logCurlTlsKeyExchange((CURL*) customData);
    }
    return dataSize;
}

#if defined(KVS_USE_AWS_LC)
/**
 * curl SSL context callback: pin the TLS key exchange groups to a post-quantum-first list.
 *
 * AWS-LC mainline already defaults to a PQC-first group list, but the AWS-LC-FIPS release
 * branches default to classical-only groups, so an explicit list is required there. Setting
 * the same list unconditionally for both variants keeps behavior identical and independent
 * of upstream default changes. Order: hybrid post-quantum first, classical fallback so
 * endpoints without ML-KEM (including all KVS endpoints today) keep working.
 */
CURLcode kvsCurlSslCtxCallback(CURL* pCurl, PVOID pSslCtx, PVOID pUserData)
{
    UNUSED_PARAM(pCurl);
    UNUSED_PARAM(pUserData);
    if (pSslCtx == NULL || 1 != SSL_CTX_set1_groups_list((SSL_CTX*) pSslCtx, KVS_TLS_KEY_EXCHANGE_GROUP_LIST)) {
        DLOGE("Failed to set TLS key exchange groups list");
        return CURLE_SSL_CONNECT_ERROR;
    }
    return CURLE_OK;
}
#endif

VOID logCurlTlsKeyExchange(PVOID pCurlHandle)
{
    CURL* pCurl = (CURL*) pCurlHandle;
    struct curl_tlssessioninfo* pTlsInfo = NULL;

    if (pCurl == NULL) {
        return;
    }

    if (CURLE_OK != curl_easy_getinfo(pCurl, CURLINFO_TLS_SSL_PTR, &pTlsInfo) || pTlsInfo == NULL || pTlsInfo->internals == NULL) {
        // Plain HTTP or TLS pointer not available
        return;
    }

#if defined(KVS_USE_OPENSSL) || defined(KVS_USE_AWS_LC)
    // curl reports AWS-LC and BoringSSL under the OpenSSL backend id
    if (pTlsInfo->backend == CURLSSLBACKEND_OPENSSL) {
        SSL* pSsl = (SSL*) pTlsInfo->internals;
        const CHAR* pGroupName = "unknown";

#if defined(OPENSSL_IS_AWSLC) || defined(OPENSSL_IS_BORINGSSL)
        UINT16 groupId = SSL_get_group_id(pSsl);
        if (groupId != 0) {
            pGroupName = SSL_get_group_name(groupId);
        }
#elif OPENSSL_VERSION_NUMBER >= 0x30000000L
        INT32 groupNid = SSL_get_negotiated_group(pSsl);
        if (groupNid != NID_undef) {
            pGroupName = SSL_group_to_name(pSsl, groupNid);
        }
#else
        // OpenSSL 1.1.1 has no public accessor for the negotiated group
        pGroupName = "n/a (OpenSSL 1.1.1)";
#endif

        DLOGI("TLS negotiated: version=%s keyExchange=%s cipher=%s backend=%s", SSL_get_version(pSsl), pGroupName,
              SSL_get_cipher_name(pSsl), OPENSSL_VERSION_TEXT);
    }
#else
    UNUSED_PARAM(pTlsInfo);
    DLOGD("TLS negotiated: key exchange introspection not implemented for this crypto backend");
#endif
}

STATUS blockingCurlCall(PRequestInfo pRequestInfo, PCallInfo pCallInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CURL* curl = NULL;
    CURLcode res;
    PCHAR url;
    UINT32 httpStatusCode;
    struct curl_slist* pHeaderList = NULL;
    CHAR errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';
    UINT32 length;
    STAT_STRUCT entryStat;
    BOOL secureConnection;

    CHK(pRequestInfo != NULL && pCallInfo != NULL, STATUS_NULL_ARG);

    // CURL global initialization
    CHK(0 == curl_global_init(CURL_GLOBAL_ALL), STATUS_CURL_LIBRARY_INIT_FAILED);
    curl = curl_easy_init();
    CHK(curl != NULL, STATUS_CURL_INIT_FAILED);

    CHK_STATUS(createCurlHeaderList(pRequestInfo, &pHeaderList));

    // set verification for SSL connections
    CHK_STATUS(requestRequiresSecureConnection(pRequestInfo->url, &secureConnection));
    if (secureConnection) {
        // Use the default cert store at /etc/ssl in most common platforms
        if (pRequestInfo->certPath[0] != '\0') {
            CHK(0 == FSTAT(pRequestInfo->certPath, &entryStat), STATUS_DIRECTORY_ENTRY_STAT_ERROR);

            if (S_ISDIR(entryStat.st_mode)) {
                // Assume it's the path as we have a directory
                curl_easy_setopt(curl, CURLOPT_CAPATH, pRequestInfo->certPath);
            } else {
                // We should check for the extension being PEM
                length = (UINT32) STRNLEN(pRequestInfo->certPath, MAX_PATH_LEN);
                CHK(length > ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION), STATUS_INVALID_ARG_LEN);
                CHK(0 == STRCMPI(CA_CERT_PEM_FILE_EXTENSION, &pRequestInfo->certPath[length - ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1]),
                    STATUS_INVALID_CA_CERT_PATH);

                curl_easy_setopt(curl, CURLOPT_CAINFO, pRequestInfo->certPath);
            }
        }

        // Enforce the public cert verification - even though this is the default
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#if defined(KVS_USE_AWS_LC)
        curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, (curl_ssl_ctx_callback) kvsCurlSslCtxCallback);
#endif
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pHeaderList);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_URL, pRequestInfo->url);
    // Only configure a client certificate when one is provided. curl >= 8.x rejects an
    // unrecognized CURLOPT_SSLCERTTYPE ("Unknown") with CURLE_BAD_FUNCTION_ARGUMENT,
    // whereas curl 7.74 silently ignored it.
    if (pRequestInfo->sslCertPath[0] != '\0' && pRequestInfo->certType != SSL_CERTIFICATE_TYPE_NOT_SPECIFIED) {
        curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, getSslCertNameFromType(pRequestInfo->certType));
        curl_easy_setopt(curl, CURLOPT_SSLCERT, pRequestInfo->sslCertPath);
        curl_easy_setopt(curl, CURLOPT_SSLKEY, pRequestInfo->sslPrivateKeyPath);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback) writeCurlResponseCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, pCallInfo);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback) tlsInfoHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, curl);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long) (pRequestInfo->connectionTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    if (pRequestInfo->completionTimeout != SERVICE_CALL_INFINITE_TIMEOUT) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long) (pRequestInfo->completionTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    }

    // Setting up limits for curl timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, (long) (pRequestInfo->lowSpeedTimeLimit / HUNDREDS_OF_NANOS_IN_A_SECOND));
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, (long) pRequestInfo->lowSpeedLimit);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);
        CHK_ERR(FALSE, STATUS_CURL_PERFORM_FAILED, "Curl perform failed for url %s with result %s : %s ", url, curl_easy_strerror(res), errorBuffer);
    }

    // curl writes a long; go through an intermediary to avoid clobbering adjacent stack on LP64
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    httpStatusCode = (UINT32) responseCode;
    CHK_ERR(httpStatusCode == HTTP_STATUS_CODE_OK, STATUS_CURL_PERFORM_FAILED, "Curl call response failed with http status %lu", httpStatusCode);

CleanUp:

    if (pHeaderList != NULL) {
        curl_slist_free_all(pHeaderList);
    }

    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }

    LEAVES();
    return retStatus;
}

SIZE_T writeCurlResponseCallback(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData)
{
    PCallInfo pCallInfo = (PCallInfo) customData;

    // Does not include the NULL terminator
    SIZE_T dataSize = size * numItems;

    if (pCallInfo == NULL) {
        return CURL_READFUNC_ABORT;
    }

    // Alloc and copy if needed
    PCHAR pNewBuffer = pCallInfo->responseData == NULL
        ? (PCHAR) MEMALLOC(pCallInfo->responseDataLen + dataSize + SIZEOF(CHAR))
        : (PCHAR) REALLOC(pCallInfo->responseData, pCallInfo->responseDataLen + dataSize + SIZEOF(CHAR));
    if (pNewBuffer != NULL) {
        // Append the new data
        MEMCPY((PBYTE) pNewBuffer + pCallInfo->responseDataLen, pBuffer, dataSize);

        pCallInfo->responseData = pNewBuffer;
        pCallInfo->responseDataLen += (UINT32) dataSize;
        pCallInfo->responseData[pCallInfo->responseDataLen] = '\0';
    } else {
        return CURL_READFUNC_ABORT;
    }

    return dataSize;
}

STATUS createCurlHeaderList(PRequestInfo pRequestInfo, struct curl_slist** ppHeaderList)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    struct curl_slist* pHeaderList = NULL;
    PSingleListNode pCurNode;
    UINT64 item;
    PCHAR pCurPtr;
    CHAR headerBuffer[MAX_REQUEST_HEADER_STRING_LEN];
    PRequestHeader pRequestHeader;

    CHK(pRequestInfo != NULL && ppHeaderList != NULL, STATUS_NULL_ARG);

    // Add headers using a temporary buffer accounting for the delimiter
    CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));

    // Iterate through the headers
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pRequestHeader = (PRequestHeader) item;
        pCurPtr = headerBuffer;
        MEMCPY(pCurPtr, pRequestHeader->pName, pRequestHeader->nameLen * SIZEOF(CHAR));
        pCurPtr += pRequestHeader->nameLen;
        MEMCPY(pCurPtr, REQUEST_HEADER_DELIMITER, REQUEST_HEADER_DELIMITER_SIZE);
        pCurPtr += REQUEST_HEADER_DELIMITER_SIZE;
        MEMCPY(pCurPtr, pRequestHeader->pValue, pRequestHeader->valueLen * SIZEOF(CHAR));
        pCurPtr += pRequestHeader->valueLen;
        *pCurPtr = '\0';

        pHeaderList = curl_slist_append(pHeaderList, headerBuffer);

        // Iterate
        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

    *ppHeaderList = pHeaderList;

CleanUp:

    LEAVES();
    return retStatus;
}
