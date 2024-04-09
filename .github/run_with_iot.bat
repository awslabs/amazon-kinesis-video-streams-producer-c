@echo off
setlocal

REM Define the environment variables
set "prefix=ci"
set "iotCert=%prefix%_certificate.pem"
set "iotPublicKey=%prefix%_public.key"
set "iotPrivateKey=%prefix%_private.key"
set "iotPolicyName=%prefix%_policy"
set "thingName=%prefix%_thing"
set "iotRoleAlias=%prefix%_role_alias"

REM Create the keys and certificate using the AWS CLI
aws iot create-keys-and-certificate --set-as-active ^
    --certificate-pem-outfile "%iotCert%" ^
    --public-key-outfile "%iotPublicKey%" ^
    --private-key-outfile "%iotPrivateKey%" > "certificate.json"

REM Extract certificate ARN
for /f "tokens=* usebackq" %%a in (`jq --raw-output ".certificateArn" "certificate.json"`) do set "certificateArn=%%a"

REM Attach the policy to the certificate
aws iot attach-policy --policy-name "%iotPolicyName%" --target "%certificateArn%"

REM Attach the thing to the certificate
aws iot attach-thing-principal --thing-name "%thingName%" --principal "%certificateArn%"

REM Get the IoT credentials endpoint
for /f "tokens=* usebackq" %%b in (`aws iot describe-endpoint --endpoint-type iot:CredentialProvider --output text`) do set "iotEndpoint=%%b"

REM Download the CA certificate
curl "https://www.amazontrust.com/repository/SFSRootCAG2.pem" -o "cacert.pem"

REM Set environment variables for the test
set "AWS_IOT_CORE_CREDENTIAL_ENDPOINT=%iotEndpoint%"
set "AWS_IOT_CORE_CERT=%CD%\%iotCert%"
set "AWS_IOT_CORE_PRIVATE_KEY=%CD%\%iotPrivateKey%"
set "AWS_IOT_CORE_ROLE_ALIAS=%iotRoleAlias%"
set "AWS_IOT_CORE_THING_NAME=%thingName%"

REM Print environment variables
echo AWS_IOT_CORE_CREDENTIAL_ENDPOINT: %AWS_IOT_CORE_CREDENTIAL_ENDPOINT%
echo AWS_IOT_CORE_CERT: %AWS_IOT_CORE_CERT%
echo AWS_IOT_CORE_PRIVATE_KEY: %AWS_IOT_CORE_PRIVATE_KEY%
echo AWS_IOT_CORE_ROLE_ALIAS: %AWS_IOT_CORE_ROLE_ALIAS%
echo AWS_IOT_CORE_THING_NAME: %AWS_IOT_CORE_THING_NAME%

REM Run the unit tests (example command, adjust path and executable as necessary)
start tst\producer_test.exe --gtest_filter="IoTCredentialTest.*"

endlocal
