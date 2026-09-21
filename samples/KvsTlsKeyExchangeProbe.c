/**
 * TLS key exchange probe (post-quantum readiness check).
 *
 * Performs one HTTPS request through the SDK's own curl code path (blockingCurlCall, the same
 * path used for KVS control plane calls) and reports the negotiated TLS key exchange group.
 * Exits non-zero if the group does not match the expected one, which makes it usable as a
 * regression check against silent fallback from post-quantum to classical key exchange.
 *
 * Usage:
 *   kvsTlsKeyExchangeProbe <https-url> [ca-cert.pem] [expected-group]
 *
 *   expected-group defaults to KVS_EXPECTED_KEY_EXCHANGE_GROUP below. Today KVS endpoints
 *   negotiate X25519; flip the default to X25519MLKEM768 once the service side supports ML-KEM.
 */
#include <com/amazonaws/kinesis/video/common/Include.h>

// What we expect a production KVS endpoint to negotiate today. Change to "X25519MLKEM768"
// when KVS endpoints enable hybrid post-quantum key exchange.
#define KVS_EXPECTED_KEY_EXCHANGE_GROUP "X25519"

// Internal to kvsCommonCurl, not part of the public header
extern STATUS blockingCurlCall(PRequestInfo, PCallInfo);

// Captured by the log hook below
static CHAR gNegotiatedGroup[64] = "";

// Intercept SDK logs to pull out the "keyExchange=" value emitted by logCurlTlsKeyExchange()
static VOID probeLogPrint(UINT32 level, PCHAR tag, PCHAR fmt, ...)
{
    CHAR buf[1024];
    va_list args;
    PCHAR p, end;
    UNUSED_PARAM(tag);

    va_start(args, fmt);
    vsnprintf(buf, SIZEOF(buf), fmt, args);
    va_end(args);

    if (level >= LOG_LEVEL_INFO) {
        fprintf(stderr, "%s\n", buf);
    }

    if ((p = STRSTR(buf, "keyExchange=")) != NULL) {
        p += STRLEN("keyExchange=");
        end = STRCHR(p, ' ');
        SNPRINTF(gNegotiatedGroup, SIZEOF(gNegotiatedGroup), "%.*s", (INT32) (end != NULL ? (SIZE_T) (end - p) : STRLEN(p)), p);
    }
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PRequestInfo pRequestInfo = NULL;
    CallInfo callInfo;
    PCHAR url, caCert = NULL, expected = KVS_EXPECTED_KEY_EXCHANGE_GROUP;
    INT32 exitCode = 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <https-url> [ca-cert.pem] [expected-group]\n", argv[0]);
        return 1;
    }
    url = argv[1];
    if (argc >= 3 && !IS_EMPTY_STRING(argv[2]) && STRCMP(argv[2], "-") != 0) {
        caCert = argv[2];
    }
    if (argc >= 4) {
        expected = argv[3];
    }

    globalCustomLogPrintFn = probeLogPrint;
    MEMSET(&callInfo, 0x00, SIZEOF(CallInfo));

    retStatus = createRequestInfo(url, NULL, "us-west-2", caCert, NULL, NULL, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, (PCHAR) "kvsTlsKeyExchangeProbe",
                                  10 * HUNDREDS_OF_NANOS_IN_A_SECOND, 20 * HUNDREDS_OF_NANOS_IN_A_SECOND, 0, 0, NULL, &pRequestInfo);
    CHK_ERR(STATUS_SUCCEEDED(retStatus), retStatus, "createRequestInfo failed 0x%08x", retStatus);
    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    // Unauthenticated GET: an HTTP error status is fine, we only care that the TLS handshake completed.
    retStatus = blockingCurlCall(pRequestInfo, &callInfo);

    printf("url:                 %s\n", url);
    printf("negotiated group:    %s\n", gNegotiatedGroup[0] ? gNegotiatedGroup : "<none: handshake failed or not captured>");
    printf("expected group:      %s\n", expected);

    if (gNegotiatedGroup[0] == '\0') {
        printf("RESULT: FAIL (no TLS session; status 0x%08x)\n", retStatus);
    } else if (STRCMP(gNegotiatedGroup, expected) == 0) {
        printf("RESULT: PASS\n");
        exitCode = 0;
    } else {
        printf("RESULT: FAIL (key exchange group mismatch)\n");
    }

CleanUp:
    if (STATUS_FAILED(retStatus) && gNegotiatedGroup[0] == '\0') {
        fprintf(stderr, "Request failed with status 0x%08x\n", retStatus);
    }
    releaseCallInfo(&callInfo);
    freeRequestInfo(&pRequestInfo);
    return exitCode;
}
