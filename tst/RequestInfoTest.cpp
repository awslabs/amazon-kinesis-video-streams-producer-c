#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class RequestInfoTest : public ProducerClientTestBase {
};

TEST_F(RequestInfoTest, createRequestInfoArgsTest)
{
    PRequestInfo pRequestInfo = NULL;
    CHAR body[11] = "HelloThere";
    EXPECT_EQ(STATUS_NULL_ARG, createRequestInfo((PCHAR) "http://test.com", NULL, (PCHAR) DEFAULT_AWS_REGION, NULL, NULL,
                                                 NULL, SSL_CERTIFICATE_TYPE_PEM, (PCHAR) DEFAULT_USER_AGENT_NAME,
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, DEFAULT_LOW_SPEED_LIMIT,
                                                 DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRequestInfo(NULL, NULL, (PCHAR) DEFAULT_AWS_REGION, NULL, NULL,
                                                 NULL, SSL_CERTIFICATE_TYPE_PEM, (PCHAR) DEFAULT_USER_AGENT_NAME,
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, DEFAULT_LOW_SPEED_LIMIT,
                                                 DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRequestInfo(NULL, NULL, NULL, NULL, NULL,
                                                 NULL, SSL_CERTIFICATE_TYPE_PEM, (PCHAR) DEFAULT_USER_AGENT_NAME,
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, DEFAULT_LOW_SPEED_LIMIT,
                                                 DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, &pRequestInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeRequestInfo(&pRequestInfo)); // Test for idempotency
    EXPECT_EQ(STATUS_SUCCESS, createRequestInfo((PCHAR) "http://test.com", NULL, (PCHAR) DEFAULT_AWS_REGION, NULL, NULL,
                                                NULL, SSL_CERTIFICATE_TYPE_PEM, (PCHAR) DEFAULT_USER_AGENT_NAME,
                                                3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, DEFAULT_LOW_SPEED_LIMIT,
                                                DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, &pRequestInfo));
    EXPECT_EQ(STATUS_NULL_ARG, freeRequestInfo(NULL));
    EXPECT_EQ(STATUS_SUCCESS, freeRequestInfo(&pRequestInfo));
    EXPECT_EQ(STATUS_SUCCESS, createRequestInfo((PCHAR) "http://test.com", body, (PCHAR) DEFAULT_AWS_REGION, NULL, NULL,
                                                NULL, SSL_CERTIFICATE_TYPE_PEM, (PCHAR) DEFAULT_USER_AGENT_NAME,
                                                3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, DEFAULT_LOW_SPEED_LIMIT,
                                                DEFAULT_LOW_SPEED_TIME_LIMIT, NULL, &pRequestInfo));
    EXPECT_EQ(0, STRNCMP(body, pRequestInfo->body, STRLEN(body)));
    EXPECT_EQ(STRLEN(body), pRequestInfo->bodySize);
    EXPECT_EQ(STATUS_SUCCESS, freeRequestInfo(&pRequestInfo));
}

TEST_F(RequestInfoTest, removeRequestHeaderApiTest)
{
    RequestInfo requestInfo;
    AwsCredentials awsCredentials;
    CHAR headerName[5];
    PCHAR pHostStart, pHostEnd;
    UINT32 len;
    EXPECT_EQ(STATUS_NULL_ARG, removeRequestHeader(NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, removeRequestHeader(&requestInfo, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, removeRequestHeader(NULL, headerName));
    EXPECT_EQ(STATUS_SUCCESS, singleListCreate(&requestInfo.pRequestHeaders));

    STRCPY(requestInfo.url, "https://kinesisvideo.us-west-2.amazonaws.com/describeStream");
    requestInfo.currentTime = GETTIME();
    requestInfo.pAwsCredentials = &awsCredentials;

    // Get the host header
    EXPECT_EQ(STATUS_SUCCESS, getRequestHost(requestInfo.url, &pHostStart, &pHostEnd));
    len = (UINT32) (pHostEnd - pHostStart);
    EXPECT_EQ(STATUS_SUCCESS, setRequestHeader(&requestInfo, (PCHAR) "host", 0, pHostStart, len));
    EXPECT_EQ(STATUS_SUCCESS, removeRequestHeader(&requestInfo, (PCHAR) "test"));
    EXPECT_EQ(STATUS_SUCCESS, removeRequestHeader(&requestInfo, (PCHAR) "host"));
    EXPECT_EQ(STATUS_SUCCESS, singleListFree(requestInfo.pRequestHeaders));
}

TEST_F(RequestInfoTest, getServiceCallResultFromHttpStatusApiTest)
{
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_RESULT_OK, getServiceCallResultFromHttpStatus(SERVICE_CALL_RESULT_OK));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_INVALID_ARG, getServiceCallResultFromHttpStatus(SERVICE_CALL_INVALID_ARG));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_RESOURCE_NOT_FOUND, getServiceCallResultFromHttpStatus(SERVICE_CALL_RESOURCE_NOT_FOUND));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_FORBIDDEN, getServiceCallResultFromHttpStatus(SERVICE_CALL_FORBIDDEN));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_RESOURCE_DELETED, getServiceCallResultFromHttpStatus(SERVICE_CALL_RESOURCE_DELETED));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_NOT_AUTHORIZED, getServiceCallResultFromHttpStatus(SERVICE_CALL_NOT_AUTHORIZED));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_NOT_IMPLEMENTED, getServiceCallResultFromHttpStatus(SERVICE_CALL_NOT_IMPLEMENTED));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_INTERNAL_ERROR, getServiceCallResultFromHttpStatus(SERVICE_CALL_INTERNAL_ERROR));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_REQUEST_TIMEOUT, getServiceCallResultFromHttpStatus(SERVICE_CALL_REQUEST_TIMEOUT));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_GATEWAY_TIMEOUT, getServiceCallResultFromHttpStatus(SERVICE_CALL_GATEWAY_TIMEOUT));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_NETWORK_READ_TIMEOUT, getServiceCallResultFromHttpStatus(SERVICE_CALL_NETWORK_READ_TIMEOUT));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT, getServiceCallResultFromHttpStatus(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT));
    EXPECT_EQ((SERVICE_CALL_RESULT) SERVICE_CALL_UNKNOWN, getServiceCallResultFromHttpStatus(MAX_UINT32));
}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com