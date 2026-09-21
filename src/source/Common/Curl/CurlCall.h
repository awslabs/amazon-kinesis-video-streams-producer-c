
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

#if defined(KVS_USE_AWS_LC)
CURLcode kvsCurlSslCtxCallback(CURL*, PVOID, PVOID);
#endif
SIZE_T writeCurlResponseCallback(PCHAR, SIZE_T, SIZE_T, PVOID);
STATUS blockingCurlCall(PRequestInfo, PCallInfo);
STATUS createCurlHeaderList(PRequestInfo, struct curl_slist**);


#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_CURL_CALL_INCLUDE_I__ */
