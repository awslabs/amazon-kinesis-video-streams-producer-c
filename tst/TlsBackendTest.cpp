/**
 * Tests for crypto-backend-specific TLS behavior. Network-free.
 */
#include "ProducerTestFixture.h"

#if defined(KVS_USE_AWS_LC)
#include <openssl/ssl.h>
#include <curl/curl.h>

extern "C" CURLcode kvsCurlSslCtxCallback(CURL*, PVOID, PVOID);
#endif

extern "C" PUBLIC_API VOID logCurlTlsKeyExchange(PVOID);

namespace com { namespace amazonaws { namespace kinesis { namespace video {

// Plain TESTs (no fixture): these are network-free and must not require credentials

// The TLS log helper must be a safe no-op for a NULL handle on every backend
TEST(TlsBackendTest, logCurlTlsKeyExchangeNullHandleIsNoOp)
{
    logCurlTlsKeyExchange(NULL);
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

#endif // KVS_USE_AWS_LC

} } } }
