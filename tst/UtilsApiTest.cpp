#include "ProducerTestFixture.h"
#include <src/source/Common/Util.h>

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class UtilsApiTest : public ProducerClientTestBase {
};


TEST_F(UtilsApiTest, ioTExpirationParsingNullArgs)
{
    UINT64 iotTimeInEpoch = 1548972059;
    CHAR validFormatIotExpirationTimeStamp[] = "2019-01-31T23:00:59Z"; // expiration is current time + 1 hour
    UINT64 expirationTimestampInEpoch = 0;

    EXPECT_EQ(STATUS_NULL_ARG, convertTimestampToEpoch(NULL, iotTimeInEpoch, &expirationTimestampInEpoch));
    EXPECT_EQ(STATUS_NULL_ARG, convertTimestampToEpoch(NULL, iotTimeInEpoch, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, convertTimestampToEpoch(validFormatIotExpirationTimeStamp, iotTimeInEpoch, NULL));
}

TEST_F(UtilsApiTest, ioTExpirationParsingValidArgs)
{
    UINT64 iotTimeInEpoch = 1548972059;
    CHAR validFormatIotExpirationTimeStamp[] = "2019-01-31T23:00:59Z"; // expiration is current time + 1 hour
    UINT64 expirationTimestampInEpoch = 0;

    EXPECT_EQ(STATUS_SUCCESS, convertTimestampToEpoch(validFormatIotExpirationTimeStamp, iotTimeInEpoch, &expirationTimestampInEpoch));
}

TEST_F(UtilsApiTest, getSslCertNameFromTypeApiTest)
{
    EXPECT_STREQ(SSL_CERTIFICATE_TYPE_PEM_STR, getSslCertNameFromType(SSL_CERTIFICATE_TYPE_PEM));
    EXPECT_STREQ(SSL_CERTIFICATE_TYPE_DER_STR, getSslCertNameFromType(SSL_CERTIFICATE_TYPE_DER));
    EXPECT_STREQ(SSL_CERTIFICATE_TYPE_ENG_STR, getSslCertNameFromType(SSL_CERTIFICATE_TYPE_ENG));
    EXPECT_STREQ(SSL_CERTIFICATE_TYPE_UNKNOWN_STR, getSslCertNameFromType((SSL_CERTIFICATE_TYPE) 0xffff));
}


}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;