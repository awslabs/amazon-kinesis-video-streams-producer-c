#include "ProducerTestFixture.h"
#include <map>

namespace com { namespace amazonaws { namespace kinesis { namespace video {

    class ProducerCallbackProviderApiTest : public ProducerClientTestBase {
    };

    TEST_F(ProducerCallbackProviderApiTest, AddProducerCallbackProvider_Returns_Valid)
    {
        PClientCallbacks pClientCallbacks = NULL;

        ProducerCallbacks producerCallbacks = {
                PRODUCER_CALLBACKS_CURRENT_VERSION,
                0,
                NULL,
                NULL,
                NULL,
                NULL
        };

        EXPECT_EQ(STATUS_SUCCESS, createDefaultCallbacksProvider(TEST_DEFAULT_CHAIN_COUNT,
                                                                 TEST_ACCESS_KEY,
                                                                 TEST_SECRET_KEY,
                                                                 TEST_SESSION_TOKEN,
                                                                 TEST_STREAMING_TOKEN_DURATION,
                                                                 TEST_DEFAULT_REGION,
                                                                 TEST_CONTROL_PLANE_URI,
                                                                 mCaCertPath,
                                                                 NULL,
                                                                 TEST_USER_AGENT,
                                                                 API_CALL_CACHE_TYPE_NONE,
                                                                 TEST_CACHING_ENDPOINT_PERIOD,
                                                                 TRUE,
                                                                 &pClientCallbacks));

        addProducerCallbacks(pClientCallbacks, &producerCallbacks);

        EXPECT_TRUE(clientReadyAggregate != pClientCallbacks->clientReadyFn);
        EXPECT_TRUE(clientShutdownAggregate != producerCallbacks.clientShutdownFn);
        EXPECT_TRUE(storageOverflowPressureAggregate != producerCallbacks.storageOverflowPressureFn);

        //non-null callback definition should return aggregated platform callback
        producerCallbacks.clientReadyFn = (UINT32 (*)(UINT64, UINT64)) 2;

        addProducerCallbacks(pClientCallbacks, &producerCallbacks);

        addProducerCallbacks(pClientCallbacks, &producerCallbacks);
        EXPECT_TRUE(4 == ((PCallbacksProvider) pClientCallbacks)->producerCallbacksCount);

        EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));

        EXPECT_EQ(NULL, pClientCallbacks);

        EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));

        EXPECT_EQ(STATUS_NULL_ARG, freeCallbacksProvider(NULL));

    }

    TEST_F(ProducerCallbackProviderApiTest, TestCorrectControlPlaneUriForSpecifiedRegion)
    {
        PClientCallbacks pClientCallbacks = NULL;
        // Map region to control plane url
        std::map<std::string, std::string> regionToControlPlaneUrlMap = {
                                    {"us-east-1", "https://kinesisvideo.us-east-1.amazonaws.com"},
                                    {"us-west-2", "https://kinesisvideo.us-west-2.amazonaws.com"},
                                    {"ap-northeast-1", "https://kinesisvideo.ap-northeast-1.amazonaws.com"},
                                    {"ap-southeast-2", "https://kinesisvideo.ap-southeast-2.amazonaws.com"},
                                    {"eu-central-1", "https://kinesisvideo.eu-central-1.amazonaws.com"},
                                    {"eu-west-1", "https://kinesisvideo.eu-west-1.amazonaws.com"},
                                    {"ap-northeast-2", "https://kinesisvideo.ap-northeast-2.amazonaws.com"},
                                    {"ap-south-1", "https://kinesisvideo.ap-south-1.amazonaws.com"},
                                    {"ap-southeast-1", "https://kinesisvideo.ap-southeast-1.amazonaws.com"},
                                    {"ca-central-1", "https://kinesisvideo.ca-central-1.amazonaws.com"},
                                    {"eu-north-1", "https://kinesisvideo.eu-north-1.amazonaws.com"},
                                    {"eu-west-2", "https://kinesisvideo.eu-west-2.amazonaws.com"},
                                    {"sa-east-1", "https://kinesisvideo.sa-east-1.amazonaws.com"},
                                    {"us-east-2", "https://kinesisvideo.us-east-2.amazonaws.com"},
                                    {"ap-east-1", "https://kinesisvideo.ap-east-1.amazonaws.com"},
                                    {"af-south-1", "https://kinesisvideo.af-south-1.amazonaws.com"},
                                    {"us-iso-east-1", "https://kinesisvideo-fips.us-iso-east-1.c2s.ic.gov"},
                                    {"us-iso-west-1", "https://kinesisvideo-fips.us-iso-west-1.c2s.ic.gov"},
                                    {"us-isob-east-1", "https://kinesisvideo-fips.us-isob-east-1.sc2s.sgov.gov"},
                                    {"us-gov-west-1", "https://kinesisvideo-fips.us-gov-west-1.amazonaws.com"},
                                    {"us-gov-east-1", "https://kinesisvideo-fips.us-gov-east-1.amazonaws.com"},
                                    {"cn-north-1", "https://kinesisvideo.cn-north-1.amazonaws.com.cn"},
                                    {"cn-northwest-1", "https://kinesisvideo.cn-northwest-1.amazonaws.com.cn"},
                                };

        for (auto it : regionToControlPlaneUrlMap) {
            EXPECT_EQ(STATUS_SUCCESS,
                      createDefaultCallbacksProvider(TEST_DEFAULT_CHAIN_COUNT, TEST_ACCESS_KEY, TEST_SECRET_KEY, TEST_SESSION_TOKEN,
                                                     TEST_STREAMING_TOKEN_DURATION, (PCHAR) it.first.c_str(), TEST_CONTROL_PLANE_URI, mCaCertPath, NULL,
                                                     TEST_USER_AGENT, API_CALL_CACHE_TYPE_NONE, TEST_CACHING_ENDPOINT_PERIOD, TRUE,
                                                     &pClientCallbacks));

            PCallbacksProvider pCallbacksProvider = (PCallbacksProvider) pClientCallbacks;
            PCurlApiCallbacks pCurlApiCallbacks = (PCurlApiCallbacks) pCallbacksProvider->pApiCallbacks[0].customData;

            EXPECT_EQ(0, STRNCMP(pCurlApiCallbacks->controlPlaneUrl, (PCHAR) it.second.c_str(), MAX_URI_CHAR_LEN));

            EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));

            EXPECT_EQ(STATUS_NULL_ARG, freeCallbacksProvider(NULL));
        }

    }

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;


