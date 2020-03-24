#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class CallbacksProviderApiFunctionalityTest : public ProducerClientTestBase {
};

TEST_F(CallbacksProviderApiFunctionalityTest, checkIotCallbackProviderOptionalCaCertPath) 
{
    PClientCallbacks pClientCallbacks;
    STATUS status;
    status = createDefaultCallbacksProviderWithIotCertificate((PCHAR) "Test.iot.endpoint",
                                                              (PCHAR) "/Test/credentials/cert/path",
                                                              (PCHAR) "/Test/private/key/path",
                                                              NULL,
                                                              (PCHAR) "TestRole",
                                                              (PCHAR) "TestThingName",
                                                              (PCHAR) DEFAULT_AWS_REGION,
                                                              NULL,
                                                              NULL,
                                                              API_CALL_CACHE_TYPE_ALL,
                                                              &pClientCallbacks);

    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_NE(STATUS_NULL_ARG, status);

    status = createDefaultCallbacksProviderWithIotCertificate((PCHAR) "Test.iot.endpoint",
                                                              (PCHAR) "/Test/credentials/cert/path",
                                                              (PCHAR) "/Test/private/key/path",
                                                              NULL,
                                                              (PCHAR) "TestRole",
                                                              (PCHAR) "TestThingName",
                                                              (PCHAR) DEFAULT_AWS_REGION,
                                                              NULL,
                                                              NULL,
                                                              API_CALL_CACHE_TYPE_NONE,
                                                              &pClientCallbacks);

    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_NE(STATUS_NULL_ARG, status);

    status = createDefaultCallbacksProviderWithIotCertificate((PCHAR) "Test.iot.endpoint",
                                                              (PCHAR) "/Test/credentials/cert/path",
                                                              (PCHAR) "/Test/private/key/path",
                                                              NULL,
                                                              (PCHAR) "TestRole",
                                                              (PCHAR) "TestThingName",
                                                              (PCHAR) DEFAULT_AWS_REGION,
                                                              NULL,
                                                              NULL,
                                                              API_CALL_CACHE_TYPE_ENDPOINT_ONLY,
                                                              &pClientCallbacks);

    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_NE(STATUS_NULL_ARG, status);
}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;
