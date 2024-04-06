#include "ProducerTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class IoTCredentialTest : public ProducerClientTestBase {
};

TEST_F(IoTCredentialTest, createDefaultCallbacksProviderWithIotCertificateValidEndpoint)
{
    PClientCallbacks pClientCallbacks;
    PCHAR iotCoreCredentialEndPoint = NULL;
    PCHAR iotCoreCert = NULL;
    PCHAR iotCorePrivateKey = NULL;
    PCHAR iotCoreRoleAlias = NULL;
    PCHAR iotThingName = NULL;
    EXPECT_TRUE((iotCoreCredentialEndPoint = GETENV(AWS_IOT_CORE_CREDENTIAL_ENDPOINT_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCoreCert = GETENV(AWS_IOT_CORE_CERT_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCorePrivateKey = GETENV(AWS_IOT_CORE_PRIVATE_KEY_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCoreRoleAlias = GETENV(AWS_IOT_CORE_ROLE_ALIAS_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotThingName = GETENV(AWS_IOT_CORE_THING_NAME_ENV_VAR)) != NULL);


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
    PCHAR iotCoreCredentialEndPoint = NULL;
    PCHAR iotCoreCert = NULL;
    PCHAR iotCorePrivateKey = NULL;
    PCHAR iotCoreRoleAlias = NULL;
    PCHAR iotThingName = NULL;
    EXPECT_TRUE((iotCoreCredentialEndPoint = GETENV(AWS_IOT_CORE_CREDENTIAL_ENDPOINT_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCoreCert = GETENV(AWS_IOT_CORE_CERT_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCorePrivateKey = GETENV(AWS_IOT_CORE_PRIVATE_KEY_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCoreRoleAlias = GETENV(AWS_IOT_CORE_ROLE_ALIAS_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotThingName = GETENV(AWS_IOT_CORE_THING_NAME_ENV_VAR)) != NULL);

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

TEST_F(IoTCredentialTest, createCurlIotCredentialProviderApiTest)
{
    PAwsCredentialProvider pCredentialProvider = NULL;
    PCHAR iotCoreCredentialEndPoint;
    PCHAR iotCoreCert;
    PCHAR iotCorePrivateKey;
    PCHAR iotCoreRoleAlias;
    PCHAR iotThingName;

    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               iotCoreCert,
                                                               iotCorePrivateKey,
                                                               NULL,
                                                               iotCoreRoleAlias,
                                                               iotThingName,
                                                               &pCredentialProvider));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               NULL,
                                                               iotCorePrivateKey,
                                                               NULL,
                                                               iotCoreRoleAlias,
                                                               iotThingName,
                                                               &pCredentialProvider));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               iotCoreRoleAlias,
                                                               iotThingName,
                                                               &pCredentialProvider));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               iotThingName,
                                                               &pCredentialProvider));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               &pCredentialProvider));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCurlIotCredentialProvider(NULL,
                                                               iotCoreCert,
                                                               iotCorePrivateKey,
                                                               NULL,
                                                               iotCoreRoleAlias,
                                                               iotThingName,
                                                               &pCredentialProvider));

}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;