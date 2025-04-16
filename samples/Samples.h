/*******************************************
Shared include file for the samples
*******************************************/
#ifndef __KINESIS_VIDEO_PRODUCER_SAMPLE_INCLUDE__
#define __KINESIS_VIDEO_PRODUCER_SAMPLE_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/cproducer/Include.h>

#define IOT_CORE_CREDENTIAL_ENDPOINT ((PCHAR) "AWS_IOT_CORE_CREDENTIAL_ENDPOINT")
#define IOT_CORE_CERT                ((PCHAR) "AWS_IOT_CORE_CERT")
#define IOT_CORE_PRIVATE_KEY         ((PCHAR) "AWS_IOT_CORE_PRIVATE_KEY")
#define IOT_CORE_ROLE_ALIAS          ((PCHAR) "AWS_IOT_CORE_ROLE_ALIAS")
#define IOT_CORE_THING_NAME          ((PCHAR) "AWS_IOT_CORE_THING_NAME")
#define IOT_CORE_CERTIFICATE_ID      ((PCHAR) "AWS_IOT_CORE_CERTIFICATE_ID")

#define CONTROL_PLANE_URI_ENV_VAR ((PCHAR) "CONTROL_PLANE_URI")

#define FILE_LOGGING_BUFFER_SIZE (100 * 1024)
#define MAX_NUMBER_OF_LOG_FILES  5

VOID getEndpointOverride(PCHAR, SIZE_T);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_PRODUCER_SAMPLE_INCLUDE__ */