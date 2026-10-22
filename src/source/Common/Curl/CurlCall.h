
#ifndef __KINESIS_VIDEO_CURL_CALL_INCLUDE_I__
#define __KINESIS_VIDEO_CURL_CALL_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// CA pem file extension
#define CA_CERT_PEM_FILE_EXTENSION ".pem"

// PQC-first TLS key exchange group list used with the AWS-LC backend. Hybrid ML-KEM groups
// first, classical fallback. Single definition so tests and both curl call sites agree.
#define KVS_TLS_KEY_EXCHANGE_GROUP_LIST "X25519MLKEM768:SecP256r1MLKEM768:X25519:P-256:P-384"

// The SDK uses the OpenSSL / AWS-LC TLS API only where curl does TLS through that library.
// On Windows curl uses Schannel, and <openssl/ssl.h> conflicts with macros from wincrypt.h.
#if (defined(KVS_USE_OPENSSL) || defined(KVS_USE_AWS_LC)) && !defined(_WIN32)
#define KVS_CURL_TLS_USES_OPENSSL_API
#endif

#if defined(KVS_USE_AWS_LC) && !defined(_WIN32)
CURLcode kvsCurlSslCtxCallback(CURL*, PVOID, PVOID);
#endif
SIZE_T writeCurlResponseCallback(PCHAR, SIZE_T, SIZE_T, PVOID);
STATUS blockingCurlCall(PRequestInfo, PCallInfo);
STATUS createCurlHeaderList(PRequestInfo, struct curl_slist**);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_CURL_CALL_INCLUDE_I__ */
