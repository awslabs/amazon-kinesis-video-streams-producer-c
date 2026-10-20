#include "ProducerTestFixture.h"

// Include Common internal header for access to Ec2CredentialProvider struct and internal functions
#define KVS_USE_OPENSSL
#define KVS_BUILD_WITH_CURL
#include <src/source/Common/Include_i.h>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {

class Ec2CredentialProviderTest : public ProducerClientTestBase {};

TEST_F(Ec2CredentialProviderTest, createEc2CredentialProvider_NullArgs)
{
    PAwsCredentialProvider pCredentialProvider = NULL;

    // NULL output pointer
    EXPECT_EQ(STATUS_NULL_ARG, createEc2CredentialProviderWithTime(0, 0, NULL, 0, NULL, NULL));

    // NULL service call function
    EXPECT_EQ(STATUS_NULL_ARG, createEc2CredentialProviderWithTime(0, 0, commonDefaultGetCurrentTimeFunc, 0, NULL, &pCredentialProvider));
    EXPECT_EQ(NULL, pCredentialProvider);
}

TEST_F(Ec2CredentialProviderTest, freeEc2CredentialProvider_NullArgs)
{
    PAwsCredentialProvider pNullProvider = NULL;

    EXPECT_EQ(STATUS_NULL_ARG, freeEc2CredentialProvider(NULL));

    // Idempotent free on NULL pointer
    EXPECT_EQ(STATUS_SUCCESS, freeEc2CredentialProvider(&pNullProvider));
}

TEST_F(Ec2CredentialProviderTest, parseEc2Response_NullArgs)
{
    EXPECT_EQ(STATUS_NULL_ARG, parseEc2Response(NULL, NULL));
}

TEST_F(Ec2CredentialProviderTest, parseEc2Response_EmptyResponse)
{
    Ec2CredentialProvider provider;
    CallInfo callInfo;

    MEMSET(&provider, 0, SIZEOF(provider));
    MEMSET(&callInfo, 0, SIZEOF(callInfo));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    callInfo.responseDataLen = 0;
    callInfo.responseData = NULL;

    EXPECT_EQ(STATUS_IMDS_INVALID_RESPONSE_LENGTH, parseEc2Response(&provider, &callInfo));
}

TEST_F(Ec2CredentialProviderTest, parseEc2Response_InvalidJson)
{
    Ec2CredentialProvider provider;
    CallInfo callInfo;
    CHAR invalidJson[] = "not json at all";

    MEMSET(&provider, 0, SIZEOF(provider));
    MEMSET(&callInfo, 0, SIZEOF(callInfo));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    callInfo.responseData = invalidJson;
    callInfo.responseDataLen = STRLEN(invalidJson);

    EXPECT_EQ(STATUS_INVALID_API_CALL_RETURN_JSON, parseEc2Response(&provider, &callInfo));
}

TEST_F(Ec2CredentialProviderTest, parseEc2Response_MissingFields)
{
    Ec2CredentialProvider provider;
    CallInfo callInfo;
    CHAR incompleteJson[] = "{"
                            "\"AccessKeyId\": \"AKIAIOSFODNN7EXAMPLE\","
                            "\"SecretAccessKey\": \"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\","
                            "\"Expiration\": \"2025-01-01T00:00:00Z\""
                            "}";

    MEMSET(&provider, 0, SIZEOF(provider));
    MEMSET(&callInfo, 0, SIZEOF(callInfo));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    callInfo.responseData = incompleteJson;
    callInfo.responseDataLen = STRLEN(incompleteJson);

    EXPECT_EQ(STATUS_IMDS_NULL_AWS_CREDS, parseEc2Response(&provider, &callInfo));
}

TEST_F(Ec2CredentialProviderTest, parseEc2Response_ValidResponse)
{
    Ec2CredentialProvider provider;
    CallInfo callInfo;
    CHAR validJson[] = "{"
                       "\"Code\": \"Success\","
                       "\"LastUpdated\": \"2025-01-01T00:00:00Z\","
                       "\"Type\": \"AWS-HMAC\","
                       "\"AccessKeyId\": \"AKIAIOSFODNN7EXAMPLE\","
                       "\"SecretAccessKey\": \"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\","
                       "\"Token\": \"FwoGZXIvYXdzEBYaDCTESTTOKEN\","
                       "\"Expiration\": \"2200-01-01T00:00:00Z\""
                       "}";

    MEMSET(&provider, 0, SIZEOF(provider));
    MEMSET(&callInfo, 0, SIZEOF(callInfo));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    callInfo.responseData = validJson;
    callInfo.responseDataLen = STRLEN(validJson);

    EXPECT_EQ(STATUS_SUCCESS, parseEc2Response(&provider, &callInfo));
    EXPECT_TRUE(provider.pAwsCredentials != NULL);

    if (provider.pAwsCredentials != NULL) {
        EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->accessKeyId, "AKIAIOSFODNN7EXAMPLE", 20));
        EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->secretKey, "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", 40));
        EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->sessionToken, "FwoGZXIvYXdzEBYaDCTESTTOKEN", 27));
        EXPECT_TRUE(provider.pAwsCredentials->expiration > 0);

        freeAwsCredentials(&provider.pAwsCredentials);
    }
}

TEST_F(Ec2CredentialProviderTest, parseEc2Response_OverwritesExistingCredentials)
{
    Ec2CredentialProvider provider;
    CallInfo callInfo;
    CHAR validJson[] = "{"
                       "\"AccessKeyId\": \"AKIANEWKEY\","
                       "\"SecretAccessKey\": \"NewSecretKey12345\","
                       "\"Token\": \"NewSessionToken\","
                       "\"Expiration\": \"2200-06-15T12:30:00Z\""
                       "}";

    MEMSET(&provider, 0, SIZEOF(provider));
    MEMSET(&callInfo, 0, SIZEOF(callInfo));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    // Create initial credentials
    EXPECT_EQ(STATUS_SUCCESS,
              createAwsCredentials((PCHAR) "OldKey", 0, (PCHAR) "OldSecret", 0, (PCHAR) "OldToken", 0, MAX_UINT64, &provider.pAwsCredentials));

    callInfo.responseData = validJson;
    callInfo.responseDataLen = STRLEN(validJson);

    EXPECT_EQ(STATUS_SUCCESS, parseEc2Response(&provider, &callInfo));
    EXPECT_TRUE(provider.pAwsCredentials != NULL);

    if (provider.pAwsCredentials != NULL) {
        EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->accessKeyId, "AKIANEWKEY", 10));
        EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->secretKey, "NewSecretKey12345", 17));
        EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->sessionToken, "NewSessionToken", 15));

        freeAwsCredentials(&provider.pAwsCredentials);
    }
}

TEST_F(Ec2CredentialProviderTest, ec2AuthCallbacks_NullArgs)
{
    PAuthCallbacks pAuthCallbacks = NULL;

    EXPECT_EQ(STATUS_NULL_ARG, createEc2AuthCallbacks(NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createEc2AuthCallbacks(NULL, &pAuthCallbacks));

    EXPECT_EQ(STATUS_NULL_ARG, freeEc2AuthCallbacks(NULL));

    // Idempotent free
    pAuthCallbacks = NULL;
    EXPECT_EQ(STATUS_SUCCESS, freeEc2AuthCallbacks(&pAuthCallbacks));
}

TEST_F(Ec2CredentialProviderTest, getEc2Credentials_NullArgs)
{
    PAwsCredentials pAwsCredentials = NULL;

    EXPECT_EQ(STATUS_NULL_ARG, getEc2Credentials(NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, getEc2Credentials(NULL, &pAwsCredentials));
}

TEST_F(Ec2CredentialProviderTest, ec2FetchImdsToken_NullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, ec2FetchImdsToken(NULL));
}

TEST_F(Ec2CredentialProviderTest, ec2CredentialHandler_NullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, ec2CredentialHandler(NULL));
}

TEST_F(Ec2CredentialProviderTest, ec2CredentialHandler_CachedCredentialsNotExpired)
{
    Ec2CredentialProvider provider;
    MEMSET(&provider, 0, SIZEOF(provider));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    // Create credentials with far-future expiration
    EXPECT_EQ(STATUS_SUCCESS,
              createAwsCredentials((PCHAR) "TestKey", 0, (PCHAR) "TestSecret", 0, (PCHAR) "TestToken", 0, MAX_UINT64, &provider.pAwsCredentials));

    // Should return immediately without fetching (no serviceCallFn needed)
    EXPECT_EQ(STATUS_SUCCESS, ec2CredentialHandler(&provider));

    // Verify credentials unchanged
    EXPECT_EQ(0, STRNCMP(provider.pAwsCredentials->accessKeyId, "TestKey", 7));

    freeAwsCredentials(&provider.pAwsCredentials);
}

TEST_F(Ec2CredentialProviderTest, ec2FetchImdsToken_CachedTokenNotExpired)
{
    Ec2CredentialProvider provider;
    MEMSET(&provider, 0, SIZEOF(provider));
    provider.getCurrentTimeFn = commonDefaultGetCurrentTimeFunc;

    // Set up a token that's not expired
    STRCPY(provider.imdsToken, "cached-token-value");
    provider.imdsTokenExpiration = MAX_UINT64;

    // Should return immediately without making a call
    EXPECT_EQ(STATUS_SUCCESS, ec2FetchImdsToken(&provider));

    // Verify token unchanged
    EXPECT_EQ(0, STRCMP(provider.imdsToken, "cached-token-value"));
}

} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
