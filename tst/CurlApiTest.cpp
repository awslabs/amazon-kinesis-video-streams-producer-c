#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class CurlApiTest : public ProducerClientTestBase {
};


TEST_F(CurlApiTest, createCurlRequestApiNullTest)
{
    CHAR url[MAX_URI_CHAR_LEN];
    PCurlRequest pCurlRequest;
    PCurlApiCallbacks pCurlApiCallbacks;
    EXPECT_EQ(STATUS_NULL_ARG, createCurlRequest(HTTP_REQUEST_VERB_POST, NULL, NULL, INVALID_STREAM_HANDLE_VALUE, (PCHAR) DEFAULT_AWS_REGION, GETTIME(),
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5, NULL, NULL,
                                                 NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlRequest(HTTP_REQUEST_VERB_POST, url, NULL, INVALID_STREAM_HANDLE_VALUE, (PCHAR) DEFAULT_AWS_REGION, GETTIME(),
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5, NULL, NULL,
                                                 NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlRequest(HTTP_REQUEST_VERB_POST, url, NULL, INVALID_STREAM_HANDLE_VALUE, (PCHAR) DEFAULT_AWS_REGION, GETTIME(),
                                                 3 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND, 5, NULL, NULL,
                                                 pCurlApiCallbacks, NULL));
}


}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;