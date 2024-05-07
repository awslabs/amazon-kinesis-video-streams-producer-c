#include "ProducerTestFixture.h"
#include "src/source/Common/AwsV4Signer.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video {

class AwsV4SignerTest : public ProducerClientTestBase {
};

TEST_F(AwsV4SignerTest, generateAwsSigV4SignatureApiInvalidTest)
{
    RequestInfo requestInfo;
    PCHAR pSigningInfo;
    UINT32 length;
    EXPECT_EQ(STATUS_NULL_ARG, generateAwsSigV4Signature(NULL, NULL, TRUE, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, generateAwsSigV4Signature(&requestInfo, NULL, TRUE, &pSigningInfo, NULL));
}

TEST_F(AwsV4SignerTest, signAwsRequestInfoQueryParamApiTest)
{
    RequestInfo requestInfo;
    AwsCredentials awsCredentials;
    // Initialize requestInfo and awsCredentials with default values
    MEMSET(&requestInfo, 0, SIZEOF(requestInfo));
    MEMSET(&awsCredentials, 0, SIZEOF(awsCredentials));

    // Set up default awsCredentials
    awsCredentials.expiration = GETTIME() + 3600 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 1 hour from now

    // Default setup for requestInfo
    STRCPY(requestInfo.url, "https://kinesisvideo.us-west-2.amazonaws.com/describeStream?query=param");
    requestInfo.currentTime = GETTIME();
    requestInfo.pAwsCredentials = &awsCredentials;
    EXPECT_EQ(STATUS_NULL_ARG, signAwsRequestInfoQueryParam(NULL));
    EXPECT_EQ(STATUS_SUCCESS, singleListCreate(&requestInfo.pRequestHeaders));
    EXPECT_EQ(STATUS_SUCCESS, signAwsRequestInfoQueryParam(&requestInfo));
    EXPECT_EQ(STATUS_SUCCESS, removeRequestHeaders(&requestInfo));
    EXPECT_EQ(STATUS_SUCCESS, singleListFree(requestInfo.pRequestHeaders));
}

TEST_F(AwsV4SignerTest, getCanonicalQueryParamsApiTest)
{
    PCHAR pQuery;
    RequestInfo requestInfo;
    EXPECT_EQ(STATUS_NULL_ARG, getCanonicalQueryParams(NULL, 0, TRUE, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, getCanonicalQueryParams(requestInfo.url, STRLEN(requestInfo.url), TRUE, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, getCanonicalQueryParams(requestInfo.url, STRLEN(requestInfo.url), TRUE, &pQuery, NULL));
}

TEST_F(AwsV4SignerTest, signAwsRequestInfoApiTest)
{
    RequestInfo requestInfo;
    AwsCredentials awsCredentials;
    UINT32 len;
    // Initialize requestInfo and awsCredentials with default values
    MEMSET(&requestInfo, 0, SIZEOF(requestInfo));
    MEMSET(&awsCredentials, 0, SIZEOF(awsCredentials));

    // Set up default awsCredentials
    awsCredentials.expiration = GETTIME() + 3600 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 1 hour from now
    // Default setup for requestInfo
    STRCPY(requestInfo.url, "https://kinesisvideo.us-west-2.amazonaws.com/describeStream");
    requestInfo.currentTime = GETTIME();
    requestInfo.pAwsCredentials = &awsCredentials;

    EXPECT_EQ(STATUS_NULL_ARG, signAwsRequestInfo(NULL));
    EXPECT_EQ(STATUS_SUCCESS, singleListCreate(&requestInfo.pRequestHeaders));
    EXPECT_EQ(STATUS_SUCCESS, signAwsRequestInfo(&requestInfo));
    EXPECT_EQ(STATUS_SUCCESS, removeRequestHeaders(&requestInfo));
    EXPECT_EQ(STATUS_SUCCESS, singleListFree(requestInfo.pRequestHeaders));
}

TEST_F(AwsV4SignerTest, generateSignatureDateTimeApiTest)
{
    EXPECT_EQ(STATUS_NULL_ARG, generateSignatureDateTime(GETTIME(), NULL));
}

TEST_F(AwsV4SignerTest, uriDecodeStringApiTest)
{
    CHAR source[] = "Hello%20World%21";
    CHAR invalidSource[] = "Hello%2";
    CHAR destination[50];
    CHAR invalidSrcDest[50];
    CHAR smallDestination[5];
    UINT32 destLen = SIZEOF(destination);
    UINT32 smallDestLen = SIZEOF(smallDestination);
    UINT32 invalidSrcDestLen = SIZEOF(invalidSrcDest);
    EXPECT_EQ(STATUS_SUCCESS, uriDecodeString(source, 0, destination, &destLen));
    EXPECT_STREQ("Hello World!", destination);
    EXPECT_EQ(13U, destLen); // Includes null terminator
    EXPECT_EQ(STATUS_NULL_ARG, uriDecodeString(NULL, 0, destination, &destLen));
    EXPECT_EQ(STATUS_NULL_ARG, uriDecodeString(source, STRLEN(source), destination, NULL));
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, uriDecodeString(source, STRLEN(source), smallDestination, &smallDestLen));
    EXPECT_EQ(STATUS_INVALID_ARG, uriDecodeString(invalidSource, STRLEN(invalidSource), invalidSrcDest, &invalidSrcDestLen));
}

TEST_F(AwsV4SignerTest, uriEncodeStringApiTest)
{
    CHAR input[] = "$";
    CHAR outputInvalid[2] = {'\0'}; // Space required for 3 characters + '\0' . But reserved only for 2
    CHAR outputValid[4] = {'\0'};
    UINT32 outputInvalidLength = SIZEOF(outputInvalid);
    UINT32 outputValidLength = SIZEOF(outputValid);
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, uriEncodeString(input, STRLEN(input), outputInvalid, &outputInvalidLength));
    EXPECT_EQ(STATUS_SUCCESS, uriEncodeString(input, STRLEN(input), outputValid, &outputValidLength));
}
TEST_F(AwsV4SignerTest, getRequestVerbStringApiTest)
{
    EXPECT_STREQ(HTTP_REQUEST_VERB_PUT_STRING, getRequestVerbString(HTTP_REQUEST_VERB_PUT));
    EXPECT_STREQ(HTTP_REQUEST_VERB_GET_STRING, getRequestVerbString(HTTP_REQUEST_VERB_GET));
    EXPECT_STREQ(HTTP_REQUEST_VERB_POST_STRING, getRequestVerbString(HTTP_REQUEST_VERB_POST));
    EXPECT_EQ(NULL, getRequestVerbString((HTTP_REQUEST_VERB) 0xffff));
}

}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com