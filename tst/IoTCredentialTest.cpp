#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class IoTCredentialTest : public ProducerClientTestBase {
};

TEST_F(IoTCredentialTest, createDefaultCallbacksProviderWithIotCertificateValidEndpoint)
{
    PClientCallbacks pClientCallbacks;
    PCHAR iotCoreCredentialEndPoint;
    PCHAR iotCoreCert;
    PCHAR iotCorePrivateKey;
    PCHAR iotCoreRoleAlias;
    PCHAR iotThingName;
    iotCoreCredentialEndPoint = GETENV("AWS_IOT_CORE_CREDENTIAL_ENDPOINT");
    iotCoreCert = GETENV("AWS_IOT_CORE_CERT");
    iotCorePrivateKey = GETENV("AWS_IOT_CORE_PRIVATE_KEY");
    iotCoreRoleAlias = GETENV("AWS_IOT_CORE_ROLE_ALIAS");
    iotThingName = GETENV("AWS_IOT_CORE_THING_NAME");


    EXPECT_EQ(STATUS_SUCCESS, createDefaultCallbacksProviderWithIotCertificate(
            iotCoreCredentialEndPoint,
            iotCoreCert,
            iotCorePrivateKey,
            NULL,
            iotCoreRoleAlias,
            iotThingName,
            (PCHAR) DEFAULT_AWS_REGION,
            NULL,
            NULL,
            &pClientCallbacks));
    EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));
}

TEST_F(IoTCredentialTest, createDefaultCallbacksProviderWithIotCertificateAndTimeoutsValidEndpoint)
{
    PClientCallbacks pClientCallbacks;
    PCHAR iotCoreCredentialEndPoint;
    PCHAR iotCoreCert;
    PCHAR iotCorePrivateKey;
    PCHAR iotCoreRoleAlias;
    PCHAR iotThingName;
    iotCoreCredentialEndPoint = GETENV("AWS_IOT_CORE_CREDENTIAL_ENDPOINT");
    iotCoreCert = GETENV("AWS_IOT_CORE_CERT");
    iotCorePrivateKey = GETENV("AWS_IOT_CORE_PRIVATE_KEY");
    iotCoreRoleAlias = GETENV("AWS_IOT_CORE_ROLE_ALIAS");
    iotThingName = GETENV("AWS_IOT_CORE_THING_NAME");

    EXPECT_EQ(STATUS_SUCCESS, createDefaultCallbacksProviderWithIotCertificateAndTimeouts(
            iotCoreCredentialEndPoint,
            iotCoreCert,
            iotCorePrivateKey,
            NULL,
            iotCoreRoleAlias,
            iotThingName,
            (PCHAR) DEFAULT_AWS_REGION,
            NULL,
            NULL,
            5 * HUNDREDS_OF_NANOS_IN_A_SECOND,
            10 * HUNDREDS_OF_NANOS_IN_A_SECOND,
            &pClientCallbacks));
    EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));
}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;