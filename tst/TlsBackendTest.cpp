/**
 * Tests for crypto-backend-specific TLS behavior. Network-free.
 */
#include "ProducerTestFixture.h"

#if defined(KVS_USE_AWS_LC)
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <curl/curl.h>

extern "C" CURLcode kvsCurlSslCtxCallback(CURL*, PVOID, PVOID);
#endif

extern "C" PUBLIC_API VOID logCurlTlsKeyExchange(PVOID);
extern "C" PUBLIC_API BOOL isCurlFinalHttpStatusLine(PCHAR, SIZE_T);

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {

// Plain TESTs (no fixture): these are network-free and must not require credentials

// The TLS log helper must be a safe no-op for a NULL handle on every backend
TEST(TlsBackendTest, logCurlTlsKeyExchangeNullHandleIsNoOp)
{
    logCurlTlsKeyExchange(NULL);
}

// Only the final (non-1xx) status line may trigger the "TLS negotiated" log, so an interim
// "100 Continue" does not log a second line for the same connection.
TEST(TlsBackendTest, finalHttpStatusLineSkipsInformationalResponses)
{
    CHAR ok11[] = "HTTP/1.1 200 OK\r\n";
    CHAR notFound11[] = "HTTP/1.1 404 Not Found\r\n";
    CHAR ok2[] = "HTTP/2 200\r\n";
    CHAR continue11[] = "HTTP/1.1 100 Continue\r\n";
    CHAR earlyHints2[] = "HTTP/2 103\r\n";
    CHAR header[] = "Content-Length: 0\r\n";
    CHAR truncated[] = "HTTP/1.1 ";

    EXPECT_TRUE(isCurlFinalHttpStatusLine(ok11, STRLEN(ok11)));
    EXPECT_TRUE(isCurlFinalHttpStatusLine(notFound11, STRLEN(notFound11)));
    EXPECT_TRUE(isCurlFinalHttpStatusLine(ok2, STRLEN(ok2)));
    EXPECT_FALSE(isCurlFinalHttpStatusLine(continue11, STRLEN(continue11)));
    EXPECT_FALSE(isCurlFinalHttpStatusLine(earlyHints2, STRLEN(earlyHints2)));
    EXPECT_FALSE(isCurlFinalHttpStatusLine(header, STRLEN(header)));
    EXPECT_FALSE(isCurlFinalHttpStatusLine(truncated, STRLEN(truncated)));
    EXPECT_FALSE(isCurlFinalHttpStatusLine(NULL, 0));
}

#if defined(KVS_USE_AWS_LC)

// The groups callback must accept a real SSL_CTX: proves the PQC-first group list
// is valid for the linked AWS-LC version (guards against upstream group renames
// and against FIPS branches that lack an entry in the list).
TEST(TlsBackendTest, sslCtxCallbackSetsPqcFirstGroupsList)
{
    SSL_CTX* pCtx = SSL_CTX_new(TLS_method());
    ASSERT_TRUE(pCtx != NULL);
    EXPECT_EQ(CURLE_OK, kvsCurlSslCtxCallback(NULL, pCtx, NULL));
    SSL_CTX_free(pCtx);
}

// NULL SSL_CTX must fail the connection rather than crash
TEST(TlsBackendTest, sslCtxCallbackNullCtxFails)
{
    EXPECT_EQ(CURLE_SSL_CONNECT_ERROR, kvsCurlSslCtxCallback(NULL, NULL, NULL));
}

// The linked AWS-LC must match the selected build variant: the FIPS build
// (USE_AWS_LC_FIPS) has to run with the FIPS module active, and the regular
// build must not silently pick up a FIPS library (or vice versa).
TEST(TlsBackendTest, fipsModeMatchesSelectedBuildVariant)
{
#if defined(KVS_USE_AWS_LC_FIPS)
    EXPECT_EQ(1, FIPS_mode());
#else
    EXPECT_EQ(0, FIPS_mode());
#endif
}

#endif // KVS_USE_AWS_LC

} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
