/**
 * TLS key exchange referee tests (AWS-LC backend, network-free, no credentials).
 *
 * Which key exchange group a KVS endpoint negotiates depends on the service's rollout
 * state, not on the SDK (PutMedia endpoints still negotiate classical X25519, the control
 * plane only recently switched to X25519MLKEM768), so a service-facing test cannot prove
 * that the SDK offers post-quantum groups, and it cannot run without credentials. These
 * tests stand up an in-process TLS server on the loopback interface whose allowed key
 * exchange groups are controlled by the test, then drive the SDK's real HTTPS code path
 * (blockingCurlCall and initializeCurlSession, which both install kvsCurlSslCtxCallback)
 * against it:
 *
 *   - a server that accepts ONLY X25519MLKEM768 completes the handshake, and the server
 *     records the ClientHello supported_groups, which must be exactly the SDK's list in the
 *     SDK's order (the per-PR PQC gate). The handshake alone is not enough: AWS-LC mainline
 *     offers ML-KEM by default, so it would pass even if a call site lost the callback;
 *   - a server that accepts only classical groups completes the handshake with X25519,
 *     which proves clean fallback for endpoints without ML-KEM;
 *   - in both cases the campaign evidence log line ("TLS negotiated: ... keyExchange=...")
 *     reports the group the server actually negotiated.
 *
 * A raw AWS-LC client pinned to classical groups must be rejected by the PQC-only server:
 * that is the referee's own negative control, so a PASS above cannot be a permissive server.
 */
#include "ProducerTestFixture.h"

#if defined(KVS_USE_AWS_LC) && !defined(_WIN32)

#include <src/source/Common/Curl/CurlCall.h> // KVS_TLS_KEY_EXCHANGE_GROUP_LIST, the list the SDK installs
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ec_key.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {

#define REFEREE_PQC_ONLY_GROUPS       "X25519MLKEM768"
#define REFEREE_CLASSICAL_ONLY_GROUPS "X25519:P-256"
#define REFEREE_GROUP_X25519MLKEM768  "X25519MLKEM768"
#define REFEREE_GROUP_X25519          "X25519"
#define REFEREE_CA_FILE               "tls_referee_ca.pem"
#define REFEREE_ACCEPT_TIMEOUT_SEC    10
#define REFEREE_HTTP_RESPONSE         "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
#define REFEREE_HTTP_100_THEN_404     "HTTP/1.1 100 Continue\r\n\r\n" REFEREE_HTTP_RESPONSE
#define REFEREE_UNKNOWN_GROUP_NAME    "unknown"

// The list the SDK must offer, in order (from the PR requirement, not from the SDK constant)
#define REFEREE_SDK_OFFERED_GROUPS "X25519MLKEM768:SecP256r1MLKEM768:X25519:P-256:P-384"

// SSL_get_group_name() returns NULL for a group id AWS-LC has no name for; never feed that
// into "%s" (undefined behavior, a crash on glibc) when it should be an assertion failure.
static const CHAR* refereeNegotiatedGroupName(const SSL* pSsl)
{
    const CHAR* pName = SSL_get_group_name(SSL_get_group_id(pSsl));
    return pName != NULL ? pName : REFEREE_UNKNOWN_GROUP_NAME;
}

// ---------------------------------------------------------------------------
// Log capture: the SDK emits the negotiated group through DLOGI; hook the PIC
// logger to record every "keyExchange=" value seen during one request.
// ---------------------------------------------------------------------------
static CHAR gRefereeLoggedGroups[16][64];
static UINT32 gRefereeLoggedGroupCount = 0;
static logPrintFunc gRefereePreviousLogFn = NULL;

static VOID refereeCaptureLogPrint(UINT32 level, const PCHAR tag, const PCHAR fmt, ...)
{
    CHAR buf[1024];
    va_list args;
    PCHAR p, end;
    UNUSED_PARAM(level);
    UNUSED_PARAM(tag);

    va_start(args, fmt);
    vsnprintf(buf, SIZEOF(buf), fmt, args);
    va_end(args);

    if ((p = STRSTR(buf, "keyExchange=")) != NULL && gRefereeLoggedGroupCount < ARRAY_SIZE(gRefereeLoggedGroups)) {
        p += STRLEN("keyExchange=");
        end = STRCHR(p, ' ');
        SNPRINTF(gRefereeLoggedGroups[gRefereeLoggedGroupCount], SIZEOF(gRefereeLoggedGroups[0]), "%.*s",
                 (INT32) (end != NULL ? (SIZE_T) (end - p) : STRLEN(p)), p);
        gRefereeLoggedGroupCount++;
    }
}

static VOID refereeStartLogCapture()
{
    gRefereeLoggedGroupCount = 0;
    gRefereePreviousLogFn = globalCustomLogPrintFn;
    globalCustomLogPrintFn = refereeCaptureLogPrint;
}

static VOID refereeStopLogCapture()
{
    globalCustomLogPrintFn = gRefereePreviousLogFn;
}

// ---------------------------------------------------------------------------
// Self-signed server certificate for 127.0.0.1, generated in-process so the
// test needs no fixture files and the private key never leaves memory.
// ---------------------------------------------------------------------------
static BOOL refereeCreateSelfSignedCert(EVP_PKEY** ppKey, X509** ppCert)
{
    BOOL ok = FALSE;
    EVP_PKEY* pKey = NULL;
    EC_KEY* pEcKey = NULL;
    X509* pCert = NULL;
    X509_NAME* pName = NULL;
    X509_EXTENSION* pSan = NULL;
    X509V3_CTX v3Ctx;

    pKey = EVP_PKEY_new();
    pEcKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (pKey == NULL || pEcKey == NULL || 1 != EC_KEY_generate_key(pEcKey) || 1 != EVP_PKEY_assign_EC_KEY(pKey, pEcKey)) {
        EC_KEY_free(pEcKey);
        goto CleanUp;
    }
    pEcKey = NULL; // owned by pKey now

    pCert = X509_new();
    if (pCert == NULL || 1 != X509_set_version(pCert, X509_VERSION_3) || 1 != ASN1_INTEGER_set(X509_get_serialNumber(pCert), 1) ||
        NULL == X509_gmtime_adj(X509_getm_notBefore(pCert), -60) || NULL == X509_gmtime_adj(X509_getm_notAfter(pCert), 3600) ||
        1 != X509_set_pubkey(pCert, pKey)) {
        goto CleanUp;
    }

    pName = X509_get_subject_name(pCert);
    if (1 != X509_NAME_add_entry_by_txt(pName, "CN", MBSTRING_ASC, (const unsigned char*) "127.0.0.1", -1, -1, 0) ||
        1 != X509_set_issuer_name(pCert, pName)) {
        goto CleanUp;
    }

    // curl verifies the peer against the IP SAN (CURLOPT_SSL_VERIFYHOST=2)
    X509V3_set_ctx(&v3Ctx, pCert, pCert, NULL, NULL, 0);
    X509V3_set_ctx_nodb(&v3Ctx);
    pSan = X509V3_EXT_conf_nid(NULL, &v3Ctx, NID_subject_alt_name, "IP:127.0.0.1");
    if (pSan == NULL || 1 != X509_add_ext(pCert, pSan, -1)) {
        goto CleanUp;
    }

    if (0 == X509_sign(pCert, pKey, EVP_sha256())) {
        goto CleanUp;
    }

    *ppKey = pKey;
    *ppCert = pCert;
    pKey = NULL;
    pCert = NULL;
    ok = TRUE;

CleanUp:
    X509_EXTENSION_free(pSan);
    X509_free(pCert);
    EVP_PKEY_free(pKey);
    return ok;
}

static BOOL refereeWriteCertPem(X509* pCert, const CHAR* path)
{
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        return FALSE;
    }
    BOOL ok = (1 == PEM_write_X509(fp, pCert));
    fclose(fp);
    return ok;
}

// ---------------------------------------------------------------------------
// Minimal TLS server: accepts one connection, completes the handshake with the
// configured groups list, answers one HTTP request, records the negotiated group.
// ---------------------------------------------------------------------------
typedef struct {
    const CHAR* groups;
    const CHAR* httpResponse; // raw bytes written after the request; NULL selects REFEREE_HTTP_RESPONSE
    INT32 listenFd;
    UINT16 port;
    EVP_PKEY* pKey;
    X509* pCert;

    // Results
    BOOL acceptedConnection;
    BOOL handshakeOk;
    CHAR negotiatedGroup[64];
    CHAR offeredGroups[256]; // supported_groups from the ClientHello, colon-separated, in order
    CHAR errorText[256];
} RefereeServer;

// Records the client's supported_groups in offer order. This is what distinguishes the SDK's
// explicit list from the library default (AWS-LC mainline already offers ML-KEM by default).
static enum ssl_select_cert_result_t refereeRecordOfferedGroups(const SSL_CLIENT_HELLO* pClientHello)
{
    RefereeServer* pServer = (RefereeServer*) SSL_get_app_data(pClientHello->ssl);
    const UINT8* pData = NULL;
    size_t len = 0, i;
    const CHAR* pName;

    if (pServer != NULL && SSL_early_callback_ctx_extension_get(pClientHello, TLSEXT_TYPE_supported_groups, &pData, &len) && len >= 2) {
        for (i = 2; i + 1 < len; i += 2) {
            pName = SSL_get_group_name((UINT16) ((pData[i] << 8) | pData[i + 1]));
            STRNCAT(pServer->offeredGroups, i == 2 ? "" : ":", SIZEOF(pServer->offeredGroups) - STRLEN(pServer->offeredGroups) - 1);
            STRNCAT(pServer->offeredGroups, pName != NULL ? pName : REFEREE_UNKNOWN_GROUP_NAME,
                    SIZEOF(pServer->offeredGroups) - STRLEN(pServer->offeredGroups) - 1);
        }
    }
    return ssl_select_cert_success;
}

static BOOL refereeServerListen(RefereeServer* pServer)
{
    struct sockaddr_in addr;
    socklen_t addrLen = SIZEOF(addr);
    struct timeval acceptTimeout;
    INT32 one = 1;

    pServer->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (pServer->listenFd < 0) {
        return FALSE;
    }
    setsockopt(pServer->listenFd, SOL_SOCKET, SO_REUSEADDR, &one, SIZEOF(one));

    // accept() must not hang the test binary if the client never connects
    acceptTimeout.tv_sec = REFEREE_ACCEPT_TIMEOUT_SEC;
    acceptTimeout.tv_usec = 0;
    setsockopt(pServer->listenFd, SOL_SOCKET, SO_RCVTIMEO, &acceptTimeout, SIZEOF(acceptTimeout));

    MEMSET(&addr, 0x00, SIZEOF(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ephemeral port, no collisions between parallel CI jobs

    if (0 != bind(pServer->listenFd, (struct sockaddr*) &addr, SIZEOF(addr)) || 0 != listen(pServer->listenFd, 1) ||
        0 != getsockname(pServer->listenFd, (struct sockaddr*) &addr, &addrLen)) {
        close(pServer->listenFd);
        pServer->listenFd = -1;
        return FALSE;
    }
    pServer->port = ntohs(addr.sin_port);
    return TRUE;
}

static VOID refereeRecordSslError(RefereeServer* pServer, const CHAR* where)
{
    CHAR errBuf[128] = "";
    UINT32 err = ERR_get_error();
    if (err != 0) {
        ERR_error_string_n(err, errBuf, SIZEOF(errBuf));
    }
    SNPRINTF(pServer->errorText, SIZEOF(pServer->errorText), "%s: %s", where, errBuf[0] ? errBuf : "no error code");
}

static PVOID refereeServerThread(PVOID arg)
{
    RefereeServer* pServer = (RefereeServer*) arg;
    SSL_CTX* pCtx = NULL;
    SSL* pSsl = NULL;
    INT32 clientFd = -1;
    CHAR requestBuf[2048];
    INT32 readLen;
    const CHAR* pResponse = pServer->httpResponse != NULL ? pServer->httpResponse : REFEREE_HTTP_RESPONSE;

    pCtx = SSL_CTX_new(TLS_method());
    if (pCtx == NULL || 1 != SSL_CTX_use_certificate(pCtx, pServer->pCert) || 1 != SSL_CTX_use_PrivateKey(pCtx, pServer->pKey) ||
        1 != SSL_CTX_set1_groups_list(pCtx, pServer->groups)) {
        refereeRecordSslError(pServer, "server SSL_CTX setup");
        goto CleanUp;
    }
    SSL_CTX_set_select_certificate_cb(pCtx, refereeRecordOfferedGroups);

    clientFd = accept(pServer->listenFd, NULL, NULL);
    if (clientFd < 0) {
        SNPRINTF(pServer->errorText, SIZEOF(pServer->errorText), "accept timed out or failed");
        goto CleanUp;
    }
    pServer->acceptedConnection = TRUE;

    pSsl = SSL_new(pCtx);
    if (pSsl == NULL || 1 != SSL_set_fd(pSsl, clientFd)) {
        refereeRecordSslError(pServer, "server SSL setup");
        goto CleanUp;
    }
    SSL_set_app_data(pSsl, pServer);

    if (1 != SSL_accept(pSsl)) {
        // Expected outcome for the negative control (client offers no acceptable group)
        refereeRecordSslError(pServer, "SSL_accept");
        goto CleanUp;
    }
    pServer->handshakeOk = TRUE;
    SNPRINTF(pServer->negotiatedGroup, SIZEOF(pServer->negotiatedGroup), "%s", refereeNegotiatedGroupName(pSsl));

    // Consume the request line/headers and answer, so curl sees a status line. The SDK's
    // header callback logs the negotiated group when the final (non-1xx) status line arrives.
    readLen = SSL_read(pSsl, requestBuf, SIZEOF(requestBuf) - 1);
    if (readLen > 0) {
        SSL_write(pSsl, pResponse, (INT32) STRLEN(pResponse));
    }
    SSL_shutdown(pSsl);

CleanUp:
    if (pSsl != NULL) {
        SSL_free(pSsl);
    }
    if (pCtx != NULL) {
        SSL_CTX_free(pCtx);
    }
    if (clientFd >= 0) {
        close(clientFd);
    }
    return NULL;
}

static SIZE_T refereeDiscardCurlData(PCHAR pBuffer, SIZE_T size, SIZE_T numItems, PVOID customData)
{
    UNUSED_PARAM(pBuffer);
    UNUSED_PARAM(customData);
    return size * numItems;
}

// Runs one SDK HTTPS request (the production code path) against a referee server that
// only accepts the given groups. viaCurlSession selects the call site: FALSE is
// blockingCurlCall (CurlCall.c), TRUE is initializeCurlSession (Response.c), which every
// KVS API call and PutMedia use. httpResponse is the raw HTTP reply (NULL for the default
// 404). Returns the SDK call status; fills the server results.
static STATUS refereeRunSdkRequestWithResponse(const CHAR* serverGroups, const CHAR* httpResponse, RefereeServer* pServer, BOOL viaCurlSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    TID serverTid = INVALID_TID_VALUE;
    BOOL threadStarted = FALSE;
    PRequestInfo pRequestInfo = NULL;
    CallInfo callInfo;
    CHAR url[64];
    CHAR caPath[MAX_PATH_LEN + 1];
    CURL* pCurl = NULL;
    BOOL curlGlobalInitialized = FALSE;
    PCurlRequest pCurlRequest = NULL;
    PCurlResponse pCurlResponse = NULL;

    MEMSET(pServer, 0x00, SIZEOF(RefereeServer));
    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));
    pServer->groups = serverGroups;
    pServer->httpResponse = httpResponse;
    pServer->listenFd = -1;

    CHK(refereeCreateSelfSignedCert(&pServer->pKey, &pServer->pCert), STATUS_INTERNAL_ERROR);
    SNPRINTF(caPath, SIZEOF(caPath), "%s", REFEREE_CA_FILE);
    CHK(refereeWriteCertPem(pServer->pCert, caPath), STATUS_INTERNAL_ERROR);
    CHK(refereeServerListen(pServer), STATUS_INTERNAL_ERROR);

    CHK_STATUS(THREAD_CREATE(&serverTid, refereeServerThread, (PVOID) pServer));
    threadStarted = TRUE;

    SNPRINTF(url, SIZEOF(url), "https://127.0.0.1:%u/", (UINT32) pServer->port);
    CHK_STATUS(createRequestInfo(url, NULL, TEST_DEFAULT_REGION, caPath, NULL, NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED,
                                 (PCHAR) "TlsKeyExchangeRefereeTest", 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 10 * HUNDREDS_OF_NANOS_IN_A_SECOND, 0, 0,
                                 NULL, &pRequestInfo));
    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    // Unauthenticated GET: the HTTP status is irrelevant, only the TLS handshake matters.
    // These are plain TEST()s, not ProducerClientTestBase fixtures, so nothing else in the
    // binary is guaranteed to have initialized libcurl first. blockingCurlCall calls
    // curl_global_init() itself on every call (CurlCall.c); initializeCurlSession does not,
    // so the session path initializes (and later releases) libcurl explicitly. The session
    // path uses the SDK's own header callback (writeHeaderCallback, which emits the
    // "TLS negotiated" line), wired to minimal request/response objects as createCurlResponse
    // would.
    if (viaCurlSession) {
        CHK(CURLE_OK == curl_global_init(CURL_GLOBAL_ALL), STATUS_INTERNAL_ERROR);
        curlGlobalInitialized = TRUE;
        CHK(NULL != (pCurlRequest = (PCurlRequest) MEMCALLOC(1, SIZEOF(CurlRequest))), STATUS_NOT_ENOUGH_MEMORY);
        CHK(NULL != (pCurlResponse = (PCurlResponse) MEMCALLOC(1, SIZEOF(CurlResponse))), STATUS_NOT_ENOUGH_MEMORY);
        pCurlRequest->pCurlResponse = pCurlResponse;
        pCurlResponse->pCurlRequest = pCurlRequest;
        CHK_STATUS(initializeCurlSession(pRequestInfo, &pCurlResponse->callInfo, &pCurl, pCurlRequest, writeHeaderCallback, NULL,
                                         refereeDiscardCurlData, refereeDiscardCurlData));
        pCurlResponse->pCurl = pCurl;
        CHK(CURLE_OK == curl_easy_perform(pCurl), STATUS_CURL_PERFORM_FAILED);
    } else {
        retStatus = blockingCurlCall(pRequestInfo, &callInfo);
    }

CleanUp:
    if (pCurl != NULL) {
        curl_easy_cleanup(pCurl);
    }
    if (curlGlobalInitialized) {
        curl_global_cleanup();
    }
    if (threadStarted) {
        THREAD_JOIN(serverTid, NULL);
    }
    if (pCurlResponse != NULL) {
        releaseCallInfo(&pCurlResponse->callInfo);
        MEMFREE(pCurlResponse);
    }
    SAFE_MEMFREE(pCurlRequest);
    releaseCallInfo(&callInfo);
    freeRequestInfo(&pRequestInfo);
    if (pServer->listenFd >= 0) {
        close(pServer->listenFd);
    }
    X509_free(pServer->pCert);
    EVP_PKEY_free(pServer->pKey);
    pServer->pCert = NULL;
    pServer->pKey = NULL;
    FREMOVE(REFEREE_CA_FILE);
    return retStatus;
}

static STATUS refereeRunSdkRequest(const CHAR* serverGroups, RefereeServer* pServer, BOOL viaCurlSession)
{
    return refereeRunSdkRequestWithResponse(serverGroups, NULL, pServer, viaCurlSession);
}

// Raw AWS-LC client (not the SDK) pinned to the given groups. Used as the negative
// control that proves the referee server really rejects clients without the group.
static BOOL refereeRawClientHandshake(const CHAR* clientGroups, const CHAR* serverGroups, CHAR* pNegotiatedGroup, UINT32 negotiatedGroupLen)
{
    BOOL ok = FALSE;
    RefereeServer server;
    TID serverTid = INVALID_TID_VALUE;
    BOOL threadStarted = FALSE;
    SSL_CTX* pCtx = NULL;
    SSL* pSsl = NULL;
    INT32 fd = -1;
    struct sockaddr_in addr;

    MEMSET(&server, 0x00, SIZEOF(server));
    server.groups = serverGroups;
    server.listenFd = -1;
    pNegotiatedGroup[0] = '\0';

    if (!refereeCreateSelfSignedCert(&server.pKey, &server.pCert) || !refereeServerListen(&server)) {
        goto CleanUp;
    }
    if (STATUS_FAILED(THREAD_CREATE(&serverTid, refereeServerThread, (PVOID) &server))) {
        goto CleanUp;
    }
    threadStarted = TRUE;

    pCtx = SSL_CTX_new(TLS_method());
    if (pCtx == NULL || 1 != SSL_CTX_set1_groups_list(pCtx, clientGroups)) {
        goto CleanUp;
    }
    SSL_CTX_set_verify(pCtx, SSL_VERIFY_NONE, NULL); // self-signed; trust is not under test here

    fd = socket(AF_INET, SOCK_STREAM, 0);
    MEMSET(&addr, 0x00, SIZEOF(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(server.port);
    if (fd < 0 || 0 != connect(fd, (struct sockaddr*) &addr, SIZEOF(addr))) {
        goto CleanUp;
    }

    pSsl = SSL_new(pCtx);
    if (pSsl == NULL || 1 != SSL_set_fd(pSsl, fd)) {
        goto CleanUp;
    }
    if (1 == SSL_connect(pSsl)) {
        SNPRINTF(pNegotiatedGroup, negotiatedGroupLen, "%s", refereeNegotiatedGroupName(pSsl));
        SSL_write(pSsl, "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", 35);
        SSL_shutdown(pSsl);
        ok = TRUE;
    }

CleanUp:
    if (pSsl != NULL) {
        SSL_free(pSsl);
    }
    if (pCtx != NULL) {
        SSL_CTX_free(pCtx);
    }
    if (fd >= 0) {
        close(fd);
    }
    if (threadStarted) {
        THREAD_JOIN(serverTid, NULL);
    }
    if (server.listenFd >= 0) {
        close(server.listenFd);
    }
    X509_free(server.pCert);
    EVP_PKEY_free(server.pKey);
    return ok;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Referee sanity (negative control): a client that offers only classical groups must be
// rejected by the PQC-only server. Without this, a passing PQC test could mean a server
// that silently accepts anything.
TEST(TlsKeyExchangeRefereeTest, pqcOnlyServerRejectsClassicalOnlyClient)
{
    CHAR negotiated[64];
    EXPECT_FALSE(refereeRawClientHandshake(REFEREE_GROUP_X25519, REFEREE_PQC_ONLY_GROUPS, negotiated, SIZEOF(negotiated)));
    EXPECT_STREQ("", negotiated);
}

// Referee sanity (positive control): the raw AWS-LC client with the PQC group completes
// the handshake, so a failure in the SDK test below is attributable to the SDK.
TEST(TlsKeyExchangeRefereeTest, pqcOnlyServerAcceptsPqcCapableClient)
{
    CHAR negotiated[64];
    EXPECT_TRUE(refereeRawClientHandshake(REFEREE_GROUP_X25519MLKEM768, REFEREE_PQC_ONLY_GROUPS, negotiated, SIZEOF(negotiated)));
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, negotiated);
}

// The PQC gate: the SDK's own HTTPS path (blockingCurlCall + kvsCurlSslCtxCallback) must
// offer X25519MLKEM768, proven by completing a handshake with a server that accepts
// nothing else. The campaign log line must report the same group.
TEST(TlsKeyExchangeRefereeTest, sdkNegotiatesHybridMlKemWithPqcOnlyServer)
{
    RefereeServer server;

    refereeStartLogCapture();
    refereeRunSdkRequest(REFEREE_PQC_ONLY_GROUPS, &server, FALSE);
    refereeStopLogCapture();

    ASSERT_TRUE(server.acceptedConnection) << server.errorText;
    ASSERT_TRUE(server.handshakeOk) << server.errorText;
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, server.negotiatedGroup);
    EXPECT_STREQ(REFEREE_SDK_OFFERED_GROUPS, server.offeredGroups);

    ASSERT_EQ(1u, gRefereeLoggedGroupCount) << "expected exactly one 'TLS negotiated' log line for one request";
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, gRefereeLoggedGroups[0]);
}

// The same gate for the curl session setup in Response.c, which every KVS API call and
// PutMedia use: it must install the SDK's group list too.
TEST(TlsKeyExchangeRefereeTest, sdkCurlSessionNegotiatesHybridMlKemWithPqcOnlyServer)
{
    RefereeServer server;

    refereeRunSdkRequest(REFEREE_PQC_ONLY_GROUPS, &server, TRUE);

    ASSERT_TRUE(server.acceptedConnection) << server.errorText;
    ASSERT_TRUE(server.handshakeOk) << server.errorText;
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, server.negotiatedGroup);
    EXPECT_STREQ(REFEREE_SDK_OFFERED_GROUPS, server.offeredGroups);
}

// Fallback: against a server without ML-KEM (the KVS PutMedia endpoints today, and any
// region or partition that has not enabled it) the SDK must still connect, using classical
// X25519, and the log line must say so. This is the behavior a silent-fallback detector
// relies on being visible.
TEST(TlsKeyExchangeRefereeTest, sdkFallsBackToClassicalWithClassicalOnlyServer)
{
    RefereeServer server;

    refereeStartLogCapture();
    refereeRunSdkRequest(REFEREE_CLASSICAL_ONLY_GROUPS, &server, FALSE);
    refereeStopLogCapture();

    ASSERT_TRUE(server.acceptedConnection) << server.errorText;
    ASSERT_TRUE(server.handshakeOk) << server.errorText;
    EXPECT_STREQ(REFEREE_GROUP_X25519, server.negotiatedGroup);

    ASSERT_EQ(1u, gRefereeLoggedGroupCount);
    EXPECT_STREQ(REFEREE_GROUP_X25519, gRefereeLoggedGroups[0]);
}

// The group list the SDK installs must put the hybrid post-quantum group first: a server
// that supports both and honors client preference picks ML-KEM. (AWS-LC servers follow
// their own list order, so the server list here is PQC-first as well; this test guards the
// SDK's list against a reordering that would demote ML-KEM behind a classical group.)
TEST(TlsKeyExchangeRefereeTest, sdkGroupListPrefersHybridMlKemOverClassical)
{
    RefereeServer server;

    refereeStartLogCapture();
    refereeRunSdkRequest(KVS_TLS_KEY_EXCHANGE_GROUP_LIST, &server, FALSE);
    refereeStopLogCapture();

    ASSERT_TRUE(server.handshakeOk) << server.errorText;
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, server.negotiatedGroup);
    ASSERT_EQ(1u, gRefereeLoggedGroupCount);
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, gRefereeLoggedGroups[0]);
}

// An interim "100 Continue" before the final response must not produce a second
// "TLS negotiated" line for the same connection, at either call site: the log is emitted on
// the final (non-1xx) status line only.
TEST(TlsKeyExchangeRefereeTest, sdkLogsKeyExchangeOnceWhenServerSendsContinue)
{
    RefereeServer server;

    refereeStartLogCapture();
    refereeRunSdkRequestWithResponse(REFEREE_PQC_ONLY_GROUPS, REFEREE_HTTP_100_THEN_404, &server, FALSE);
    refereeStopLogCapture();

    ASSERT_TRUE(server.handshakeOk) << server.errorText;
    ASSERT_EQ(1u, gRefereeLoggedGroupCount) << "100 Continue must not add a 'TLS negotiated' line (blockingCurlCall)";
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, gRefereeLoggedGroups[0]);
}

TEST(TlsKeyExchangeRefereeTest, sdkCurlSessionLogsKeyExchangeOnceWhenServerSendsContinue)
{
    RefereeServer server;

    refereeStartLogCapture();
    refereeRunSdkRequestWithResponse(REFEREE_PQC_ONLY_GROUPS, REFEREE_HTTP_100_THEN_404, &server, TRUE);
    refereeStopLogCapture();

    ASSERT_TRUE(server.handshakeOk) << server.errorText;
    ASSERT_EQ(1u, gRefereeLoggedGroupCount) << "100 Continue must not add a 'TLS negotiated' line (curl session)";
    EXPECT_STREQ(REFEREE_GROUP_X25519MLKEM768, gRefereeLoggedGroups[0]);
}

} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif // KVS_USE_AWS_LC && !_WIN32
