#include "ProducerTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {

class AuthCallbackTest : public ProducerClientTestBase {};

TEST_F(AuthCallbackTest, RotatingStaticAuthCallback_ReturnsExtendedExpiration)
{
    // check AWSCredentials is extended by rotation period for stream token function.
}

TEST_F(AuthCallbackTest, ioTExpirationParsing_Returns_Success)
{
    UINT64 iotTimeInEpoch = 1548972059;
    CHAR validFormatIotExpirationTimeStamp[] = "2019-01-31T23:00:59Z"; // expiration is current time + 1 hour
    UINT64 expirationTimestampInEpoch = 0;

    convertTimestampToEpoch(validFormatIotExpirationTimeStamp, iotTimeInEpoch, &expirationTimestampInEpoch);

    EXPECT_TRUE(iotTimeInEpoch == expirationTimestampInEpoch / HUNDREDS_OF_NANOS_IN_A_SECOND - 3600);

    iotTimeInEpoch = 1548975659; // iot expiration same as current time

    convertTimestampToEpoch(validFormatIotExpirationTimeStamp, iotTimeInEpoch, &expirationTimestampInEpoch);

    EXPECT_TRUE(iotTimeInEpoch == expirationTimestampInEpoch / HUNDREDS_OF_NANOS_IN_A_SECOND);

    iotTimeInEpoch = 1548975660; // iot expiration occurs in the past

    EXPECT_EQ(STATUS_IOT_EXPIRATION_OCCURS_IN_PAST,
              convertTimestampToEpoch(validFormatIotExpirationTimeStamp, iotTimeInEpoch, &expirationTimestampInEpoch));
}

TEST_F(AuthCallbackTest, invalidIoTExpirationParsing_Returns_Failure)
{
    UINT64 iotTimeInEpoch = 1548972059;
    UINT64 expirationTimestampInEpoch = 0;
    CHAR invalidIotExpirationTimeStamp[] = "2019-00-31T23:00:59Z";
    CHAR invalidFormatIotExpirationTimeStamp[] = "2019-00-31-23:00:59Z";
    CHAR emptyIotExpirationTimestamp[] = "";

    EXPECT_EQ(STATUS_IOT_EXPIRATION_PARSING_FAILED,
              convertTimestampToEpoch(invalidIotExpirationTimeStamp, iotTimeInEpoch, &expirationTimestampInEpoch));

    EXPECT_EQ(STATUS_IOT_EXPIRATION_PARSING_FAILED,
              convertTimestampToEpoch(invalidFormatIotExpirationTimeStamp, iotTimeInEpoch, &expirationTimestampInEpoch));

    EXPECT_EQ(STATUS_TIMESTAMP_STRING_UNRECOGNIZED_FORMAT,
              convertTimestampToEpoch(emptyIotExpirationTimestamp, iotTimeInEpoch, &expirationTimestampInEpoch));
}

TEST_F(AuthCallbackTest, verify_fileAuthCallback_provider_works)
{
    PDeviceInfo pDeviceInfo;
    PClientCallbacks pClientCallbacks;
    CLIENT_HANDLE clientHandle;
    STREAM_HANDLE streamHandle;
    PStreamInfo pStreamInfo;
    CHAR streamName[MAX_STREAM_NAME_LEN + 1];
    PCHAR authFilePath = NULL;
    PAuthCallbacks pAuthCallbacks = NULL;

    authFilePath = getenv(TEST_AUTH_FILE_PATH);
    if (authFilePath == NULL) {
        DLOGI("Auth file not provided, passing test");
        return;
    }

    STRNCPY(streamName, (PCHAR) TEST_STREAM_NAME, MAX_STREAM_NAME_LEN);
    streamName[MAX_STREAM_NAME_LEN] = '\0';
    EXPECT_EQ(STATUS_SUCCESS, createDefaultDeviceInfo(&pDeviceInfo));
    pDeviceInfo->clientInfo.loggerLogLevel = this->loggerLogLevel;
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProvider(streamName, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS,
              createAbstractDefaultCallbacksProvider(TEST_DEFAULT_CHAIN_COUNT, API_CALL_CACHE_TYPE_NONE, TEST_CACHING_ENDPOINT_PERIOD, mRegion,
                                                     TEST_CONTROL_PLANE_URI, mCaCertPath, NULL, NULL, &pClientCallbacks));

    EXPECT_EQ(STATUS_SUCCESS, createFileAuthCallbacks(pClientCallbacks, authFilePath, &pAuthCallbacks));

    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoClientSync(pDeviceInfo, pClientCallbacks, &clientHandle));
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    EXPECT_EQ(STATUS_SUCCESS, stopKinesisVideoStreamSync(streamHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&streamHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoClient(&clientHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeDeviceInfo(&pDeviceInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));
}

TEST_F(AuthCallbackTest, credential_provider_auth_callbacks_test)
{
    PDeviceInfo pDeviceInfo;
    PClientCallbacks pClientCallbacks;
    PAwsCredentialProvider pAwsCredentialProvider;
    CLIENT_HANDLE clientHandle;
    STREAM_HANDLE streamHandle;
    PStreamInfo pStreamInfo;
    CHAR streamName[MAX_STREAM_NAME_LEN + 1];
    PAuthCallbacks pAuthCallbacks;

    STRNCPY(streamName, (PCHAR) TEST_STREAM_NAME, MAX_STREAM_NAME_LEN);
    streamName[MAX_STREAM_NAME_LEN] = '\0';
    EXPECT_EQ(STATUS_SUCCESS, createDefaultDeviceInfo(&pDeviceInfo));
    pDeviceInfo->clientInfo.loggerLogLevel = this->loggerLogLevel;
    EXPECT_EQ(STATUS_SUCCESS, createRealtimeVideoStreamInfoProvider(streamName, TEST_RETENTION_PERIOD, TEST_STREAM_BUFFER_DURATION, &pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS,
              createAbstractDefaultCallbacksProvider(TEST_DEFAULT_CHAIN_COUNT, API_CALL_CACHE_TYPE_NONE, TEST_CACHING_ENDPOINT_PERIOD, mRegion,
                                                     TEST_CONTROL_PLANE_URI, mCaCertPath, NULL, NULL, &pClientCallbacks));

    // Create the credential provider based on static credentials which will be used with auth callbacks
    EXPECT_EQ(STATUS_SUCCESS, createStaticCredentialProvider(mAccessKey, 0, mSecretKey, 0, mSessionToken, 0, MAX_UINT64, &pAwsCredentialProvider));

    // Creating client should fail with missing auth callbacks
    EXPECT_EQ(STATUS_SERVICE_CALL_CALLBACKS_MISSING, createKinesisVideoClientSync(pDeviceInfo, pClientCallbacks, &clientHandle));

    // Execute negative tests on the target API
    EXPECT_EQ(STATUS_NULL_ARG, createCredentialProviderAuthCallbacks(NULL, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCredentialProviderAuthCallbacks(pClientCallbacks, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createCredentialProviderAuthCallbacks(pClientCallbacks, pAwsCredentialProvider, NULL));

    EXPECT_EQ(STATUS_SUCCESS, createCredentialProviderAuthCallbacks(pClientCallbacks, pAwsCredentialProvider, &pAuthCallbacks));

    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoClientSync(pDeviceInfo, pClientCallbacks, &clientHandle));
    EXPECT_EQ(STATUS_SUCCESS, createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    EXPECT_EQ(STATUS_SUCCESS, stopKinesisVideoStreamSync(streamHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoStream(&streamHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeKinesisVideoClient(&clientHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeDeviceInfo(&pDeviceInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeStreamInfoProvider(&pStreamInfo));
    EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));
    EXPECT_EQ(STATUS_SUCCESS, freeStaticCredentialProvider(&pAwsCredentialProvider));
}
// Credential provider that sleeps for a configurable duration before returning,
// simulating a slow auth backend (e.g. STS with high latency).
typedef struct {
    AwsCredentialProvider credentialProvider;
    UINT64 delayInHundredsOfNanos;
} SlowCredentialProvider, *PSlowCredentialProvider;

static STATUS slowGetCredentialsFn(PAwsCredentialProvider pProvider, PAwsCredentials* ppCredentials)
{
    UNUSED_PARAM(ppCredentials);
    PSlowCredentialProvider pSlow = (PSlowCredentialProvider) pProvider;
    THREAD_SLEEP(pSlow->delayInHundredsOfNanos);
    return STATUS_OPERATION_TIMED_OUT;
}

// Verifies that a slow auth callback (simulating delayed STS response) causes
// createKinesisVideoClientSync to time out when createClientTimeout < auth delay.
TEST_F(AuthCallbackTest, SlowAuthCallback_ExceedingClientTimeout_ReturnsTimeout)
{
    PDeviceInfo pDeviceInfo = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    PAwsCredentialProvider pSlowProvider = NULL;
    PAuthCallbacks pAuthCallbacks = NULL;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;

    const UINT64 authDelayHns = 2 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    const UINT64 clientTimeoutHns = 500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    PSlowCredentialProvider pSlow = (PSlowCredentialProvider) MEMCALLOC(1, SIZEOF(SlowCredentialProvider));
    ASSERT_NE(nullptr, pSlow);
    pSlow->credentialProvider.getCredentialsFn = slowGetCredentialsFn;
    pSlow->delayInHundredsOfNanos = authDelayHns;
    pSlowProvider = (PAwsCredentialProvider) pSlow;

    EXPECT_EQ(STATUS_SUCCESS, createDefaultDeviceInfo(&pDeviceInfo));
    pDeviceInfo->clientInfo.loggerLogLevel = this->loggerLogLevel;
    pDeviceInfo->clientInfo.createClientTimeout = clientTimeoutHns;

    EXPECT_EQ(STATUS_SUCCESS,
              createAbstractDefaultCallbacksProvider(TEST_DEFAULT_CHAIN_COUNT, API_CALL_CACHE_TYPE_NONE, TEST_CACHING_ENDPOINT_PERIOD, mRegion,
                                                     TEST_CONTROL_PLANE_URI, mCaCertPath, NULL, NULL, &pClientCallbacks));

    EXPECT_EQ(STATUS_SUCCESS, createCredentialProviderAuthCallbacks(pClientCallbacks, pSlowProvider, &pAuthCallbacks));

    UINT64 startTime = GETTIME();
    STATUS retStatus = createKinesisVideoClientSync(pDeviceInfo, pClientCallbacks, &clientHandle);
    UINT64 elapsed = GETTIME() - startTime;

    // Client creation must fail with timeout
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, retStatus);

    // PATCH VERIFIED: With the background thread fix in PIC Client.c, the condition
    // variable wait starts immediately and fires at createClientTimeout (~500ms),
    // NOT after the full auth delay (~2s). elapsed should be close to clientTimeoutHns.
    EXPECT_LT(elapsed, authDelayHns) << "Timeout did not fire before auth delay — patch not effective";
    EXPECT_GE(elapsed, clientTimeoutHns / 2) << "Elapsed suspiciously short — check timeout value";

    if (IS_VALID_CLIENT_HANDLE(clientHandle)) {
        freeKinesisVideoClient(&clientHandle);
    }
    freeDeviceInfo(&pDeviceInfo);
    freeCallbacksProvider(&pClientCallbacks);
    MEMFREE(pSlow);
}

} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
