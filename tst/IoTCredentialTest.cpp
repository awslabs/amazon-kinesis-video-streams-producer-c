#include "ProducerTestFixture.h"
#include "src/source/Common/IotCredentialProvider.h"

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


TEST_F(IoTCredentialTest, createCurlIotCredentialProviderWithTimeAndTimeoutApiTest) {
    PAwsCredentialProvider pCredentialProvider = NULL;
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

    EXPECT_EQ(STATUS_SUCCESS, createCurlIotCredentialProviderWithTimeAndTimeout(iotCoreCredentialEndPoint, iotCoreCert,
                                                                                iotCorePrivateKey, NULL,
                                                                                iotCoreRoleAlias,
                                                                                iotThingName,
                                                                                10 * HUNDREDS_OF_NANOS_IN_A_SECOND,
                                                                                5 * HUNDREDS_OF_NANOS_IN_A_SECOND, NULL,
                                                                                0,
                                                                                &pCredentialProvider)
    );
    EXPECT_EQ(IOT_REQUEST_CONNECTION_TIMEOUT, reinterpret_cast<IotCredentialProvider*>(pCredentialProvider)->connectionTimeout);
    EXPECT_EQ(IOT_REQUEST_COMPLETION_TIMEOUT, reinterpret_cast<IotCredentialProvider*>(pCredentialProvider)->completionTimeout);
    EXPECT_EQ(STATUS_SUCCESS, freeIotCredentialProvider(&pCredentialProvider));
}
#ifdef KVS_BUILD_WITH_LWS
TEST_F(IoTCredentialTest, createLwsIotCredentialProviderApiTest) {
    PAwsCredentialProvider pCredentialProvider;
    PCHAR iotCoreCredentialEndPoint;
    PCHAR iotCoreCert;
    PCHAR iotCorePrivateKey;
    PCHAR iotCoreRoleAlias;
    PCHAR iotThingName;
    CHAR invalidCredentialEndpointLength[MAX_URI_CHAR_LEN + 2];
    CHAR invalidRoleAliasLength[MAX_ROLE_ALIAS_LEN + 2];
    CHAR invalidPrivateKeyPathLength[MAX_PATH_LEN + 2];
    CHAR invalidCaCertPathLength[MAX_PATH_LEN + 2];
    CHAR invalidCoreCertPathLength[MAX_PATH_LEN + 2];
    CHAR invalidThingNameLength[MAX_IOT_THING_NAME_LEN + 2];

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                             iotCoreCert,
                                                             iotCorePrivateKey,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              iotCorePrivateKey,
                                                              (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                              iotCoreRoleAlias,
                                                              iotThingName,
                                                              &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              iotCorePrivateKey,
                                                              (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                              iotCoreRoleAlias,
                                                              iotThingName,
                                                              &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              NULL,
                                                              (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                              iotCoreRoleAlias,
                                                              iotThingName,
                                                              &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              iotCoreRoleAlias,
                                                              iotThingName,
                                                              &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              iotThingName,
                                                              &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              &pCredentialProvider));

    EXPECT_EQ(STATUS_NULL_ARG, createLwsIotCredentialProvider(NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL,
                                                              NULL));

    EXPECT_TRUE((iotCoreCredentialEndPoint = GETENV(AWS_IOT_CORE_CREDENTIAL_ENDPOINT_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCoreCert = GETENV(AWS_IOT_CORE_CERT_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCorePrivateKey = GETENV(AWS_IOT_CORE_PRIVATE_KEY_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotCoreRoleAlias = GETENV(AWS_IOT_CORE_ROLE_ALIAS_ENV_VAR)) != NULL);
    EXPECT_TRUE((iotThingName = GETENV(AWS_IOT_CORE_THING_NAME_ENV_VAR)) != NULL);
    MEMSET(invalidCredentialEndpointLength, 'a', SIZEOF(invalidCredentialEndpointLength));
    MEMSET(invalidRoleAliasLength, 'b', SIZEOF(invalidRoleAliasLength));
    MEMSET(invalidPrivateKeyPathLength, 'c', SIZEOF(invalidPrivateKeyPathLength));
    MEMSET(invalidCaCertPathLength, 'd', SIZEOF(invalidCaCertPathLength));
    MEMSET(invalidCoreCertPathLength, 'e', SIZEOF(invalidCoreCertPathLength));
    MEMSET(invalidThingNameLength, 'f', SIZEOF(invalidThingNameLength));

    EXPECT_EQ(STATUS_INVALID_ARG, createLwsIotCredentialProvider(invalidCredentialEndpointLength,
                                                             iotCoreCert,
                                                             iotCorePrivateKey,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_MAX_ROLE_ALIAS_LEN_EXCEEDED, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             iotCoreCert,
                                                             iotCorePrivateKey,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             invalidRoleAliasLength,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_PATH_TOO_LONG, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             iotCoreCert,
                                                             invalidPrivateKeyPathLength,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_PATH_TOO_LONG, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             iotCoreCert,
                                                             invalidPrivateKeyPathLength,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_PATH_TOO_LONG, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             iotCoreCert,
                                                             iotCorePrivateKey,
                                                             invalidCaCertPathLength,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_PATH_TOO_LONG, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             invalidCoreCertPathLength,
                                                             iotCorePrivateKey,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_MAX_IOT_THING_NAME_LENGTH, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             iotCoreCert,
                                                             iotCorePrivateKey,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             invalidThingNameLength,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_SUCCESS, createLwsIotCredentialProvider(iotCoreCredentialEndPoint,
                                                             iotCoreCert,
                                                             iotCorePrivateKey,
                                                             (PCHAR) DEFAULT_KVS_CACERT_PATH,
                                                             iotCoreRoleAlias,
                                                             iotThingName,
                                                             &pCredentialProvider));
    EXPECT_EQ(STATUS_SUCCESS, freeIotCredentialProvider(&pCredentialProvider));
}
#endif

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com