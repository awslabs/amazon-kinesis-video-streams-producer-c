#include "ProducerTestFixture.h"
#include <map>
#include <algorithm>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {

class ProducerCallbackProviderApiTest : public ProducerClientTestBase {};

TEST_F(ProducerCallbackProviderApiTest, AddProducerCallbackProvider_Returns_Valid)
{
    PClientCallbacks pClientCallbacks = NULL;

    ProducerCallbacks producerCallbacks = {PRODUCER_CALLBACKS_CURRENT_VERSION, 0, NULL, NULL, NULL, NULL};

    EXPECT_EQ(STATUS_SUCCESS,
              createDefaultCallbacksProvider(TEST_DEFAULT_CHAIN_COUNT, TEST_ACCESS_KEY, TEST_SECRET_KEY, TEST_SESSION_TOKEN,
                                             TEST_STREAMING_TOKEN_DURATION, TEST_DEFAULT_REGION, TEST_CONTROL_PLANE_URI, mCaCertPath, NULL,
                                             TEST_USER_AGENT, API_CALL_CACHE_TYPE_NONE, TEST_CACHING_ENDPOINT_PERIOD, TRUE, &pClientCallbacks));

    addProducerCallbacks(pClientCallbacks, &producerCallbacks);

    EXPECT_TRUE(clientReadyAggregate != pClientCallbacks->clientReadyFn);
    EXPECT_TRUE(clientShutdownAggregate != producerCallbacks.clientShutdownFn);
    EXPECT_TRUE(storageOverflowPressureAggregate != producerCallbacks.storageOverflowPressureFn);

    // non-null callback definition should return aggregated platform callback
    producerCallbacks.clientReadyFn = (UINT32(*)(UINT64, UINT64)) 2;

    addProducerCallbacks(pClientCallbacks, &producerCallbacks);

    addProducerCallbacks(pClientCallbacks, &producerCallbacks);
    EXPECT_TRUE(4 == ((PCallbacksProvider) pClientCallbacks)->producerCallbacksCount);

    EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));

    EXPECT_EQ(NULL, pClientCallbacks);

    EXPECT_EQ(STATUS_SUCCESS, freeCallbacksProvider(&pClientCallbacks));

    EXPECT_EQ(STATUS_NULL_ARG, freeCallbacksProvider(NULL));
}

// ---------------- GeneratesCorrectUrls Parameterized Test ----------------

struct ControlPlaneUrlTestParam {
    std::string region;
    std::string expectedUrl;
    BOOL isLegacyEndpoint;
};

// Test fixture class for parameterized tests
class ControlPlaneUrlTest : public ::testing::TestWithParam<ControlPlaneUrlTestParam> {};

// Parameterized test case
TEST_P(ControlPlaneUrlTest, GeneratesCorrectUrls)
{
    char buffer[MAX_URI_CHAR_LEN] = {0};
    const ControlPlaneUrlTestParam& param = GetParam();

    constructControlPlaneUrl(buffer, MAX_URI_CHAR_LEN, param.region.c_str(), param.isLegacyEndpoint);
    EXPECT_STREQ(buffer, param.expectedUrl.c_str());
}

// Custom test name generator based on the region and legacy endpoint flag
// Otherwise you'll see GeneratesCorrectUrls/0, GeneratesCorrectUrls/1, etc
std::string ControlPlaneUrlTestName(const testing::TestParamInfo<ControlPlaneUrlTestParam>& info)
{
    const ControlPlaneUrlTestParam& param = info.param;

    // Note: Gtest doesn't allow hyphens in the test names
    // Replace hyphens with underscores to make the test name valid
    std::string region = param.region;
    std::replace(region.begin(), region.end(), '-', '_');

    return region + "_" + (param.isLegacyEndpoint ? "Legacy" : "New");
}

// Instantiate the test suite with multiple test cases
INSTANTIATE_TEST_SUITE_P(ControlPlaneUrlTests, ControlPlaneUrlTest,
                         ::testing::Values(
                             // Legacy endpoints
                             ControlPlaneUrlTestParam{"us-east-1", "https://kinesisvideo.us-east-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"us-west-2", "https://kinesisvideo.us-west-2.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ap-northeast-1", "https://kinesisvideo.ap-northeast-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ap-southeast-2", "https://kinesisvideo.ap-southeast-2.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"eu-central-1", "https://kinesisvideo.eu-central-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"eu-west-1", "https://kinesisvideo.eu-west-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ap-northeast-2", "https://kinesisvideo.ap-northeast-2.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ap-south-1", "https://kinesisvideo.ap-south-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ap-southeast-1", "https://kinesisvideo.ap-southeast-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ca-central-1", "https://kinesisvideo.ca-central-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"eu-north-1", "https://kinesisvideo.eu-north-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"eu-west-2", "https://kinesisvideo.eu-west-2.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"sa-east-1", "https://kinesisvideo.sa-east-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"us-east-2", "https://kinesisvideo.us-east-2.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"ap-east-1", "https://kinesisvideo.ap-east-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"af-south-1", "https://kinesisvideo.af-south-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"us-iso-east-1", "https://kinesisvideo-fips.us-iso-east-1.c2s.ic.gov", TRUE},
                             ControlPlaneUrlTestParam{"us-iso-west-1", "https://kinesisvideo-fips.us-iso-west-1.c2s.ic.gov", TRUE},
                             ControlPlaneUrlTestParam{"us-isob-east-1", "https://kinesisvideo-fips.us-isob-east-1.sc2s.sgov.gov", TRUE},
                             ControlPlaneUrlTestParam{"us-gov-west-1", "https://kinesisvideo-fips.us-gov-west-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"us-gov-east-1", "https://kinesisvideo-fips.us-gov-east-1.amazonaws.com", TRUE},
                             ControlPlaneUrlTestParam{"cn-north-1", "https://kinesisvideo.cn-north-1.amazonaws.com.cn", TRUE},
                             ControlPlaneUrlTestParam{"cn-northwest-1", "https://kinesisvideo.cn-northwest-1.amazonaws.com.cn", TRUE},

                             // Dual-Stack endpoints
                             ControlPlaneUrlTestParam{"us-west-2", "https://kinesisvideo.us-west-2.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ap-northeast-1", "https://kinesisvideo.ap-northeast-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ap-southeast-2", "https://kinesisvideo.ap-southeast-2.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"eu-central-1", "https://kinesisvideo.eu-central-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"eu-west-1", "https://kinesisvideo.eu-west-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ap-northeast-2", "https://kinesisvideo.ap-northeast-2.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ap-south-1", "https://kinesisvideo.ap-south-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ap-southeast-1", "https://kinesisvideo.ap-southeast-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ca-central-1", "https://kinesisvideo.ca-central-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"eu-north-1", "https://kinesisvideo.eu-north-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"eu-west-2", "https://kinesisvideo.eu-west-2.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"sa-east-1", "https://kinesisvideo.sa-east-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"us-east-2", "https://kinesisvideo.us-east-2.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"ap-east-1", "https://kinesisvideo.ap-east-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"af-south-1", "https://kinesisvideo.af-south-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"us-iso-east-1", "https://kinesisvideo-fips.us-iso-east-1.api.aws.ic.gov", FALSE},
                             ControlPlaneUrlTestParam{"us-iso-west-1", "https://kinesisvideo-fips.us-iso-west-1.api.aws.ic.gov", FALSE},
                             ControlPlaneUrlTestParam{"us-isob-east-1", "https://kinesisvideo-fips.us-isob-east-1.api.aws.scloud", FALSE},
                             ControlPlaneUrlTestParam{"us-gov-west-1", "https://kinesisvideo-fips.us-gov-west-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"us-gov-east-1", "https://kinesisvideo-fips.us-gov-east-1.api.aws", FALSE},
                             ControlPlaneUrlTestParam{"cn-north-1", "https://kinesisvideo.cn-north-1.api.amazonwebservices.com.cn", FALSE},
                             ControlPlaneUrlTestParam{"cn-northwest-1", "https://kinesisvideo.cn-northwest-1.api.amazonwebservices.com.cn", FALSE}),
                         ControlPlaneUrlTestName);
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
