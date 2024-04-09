#include "ProducerTestFixture.h"
#include "src/source/Response.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class CurlApiTest : public ProducerClientTestBase {
};


TEST_F(CurlApiTest, createCurlRequestApiNullTest)
{
    CHAR url[MAX_URI_CHAR_LEN];
    CurlApiCallbacks curlApiCallbacks;
    EXPECT_EQ(STATUS_NULL_ARG, createCurlRequest(HTTP_REQUEST_VERB_POST, NULL, NULL, INVALID_STREAM_HANDLE_VALUE, (PCHAR) DEFAULT_AWS_REGION, GETTIME(),
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5, NULL, NULL,
                                                 NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlRequest(HTTP_REQUEST_VERB_POST, url, NULL, INVALID_STREAM_HANDLE_VALUE, (PCHAR) DEFAULT_AWS_REGION, GETTIME(),
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5, NULL, NULL,
                                                 NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlRequest(HTTP_REQUEST_VERB_POST, url, NULL, INVALID_STREAM_HANDLE_VALUE, (PCHAR) DEFAULT_AWS_REGION, GETTIME(),
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5, NULL, NULL,
                                                 &curlApiCallbacks, NULL));
}

TEST_F(CurlApiTest, getServiceCallResultFromCurlStatusApiTest)
{
    EXPECT_EQ(SERVICE_CALL_RESULT_OK, getServiceCallResultFromCurlStatus(CURLE_OK));
    EXPECT_EQ(SERVICE_CALL_INVALID_ARG, getServiceCallResultFromCurlStatus(CURLE_UNSUPPORTED_PROTOCOL));
    EXPECT_EQ(SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT, getServiceCallResultFromCurlStatus(CURLE_OPERATION_TIMEDOUT));
    EXPECT_EQ(SERVICE_CALL_NOT_AUTHORIZED, getServiceCallResultFromCurlStatus(CURLE_SSL_CERTPROBLEM));
    EXPECT_EQ(SERVICE_CALL_NOT_AUTHORIZED, getServiceCallResultFromCurlStatus(CURLE_SSL_CACERT));
    EXPECT_EQ(SERVICE_CALL_UNKNOWN, getServiceCallResultFromCurlStatus((CURLcode) 0xffff));
}


}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;