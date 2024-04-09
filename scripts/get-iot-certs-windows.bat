@echo off
setlocal

rem Set variables based on input prefix
set PREFIX=%1
set THING_NAME=%PREFIX%_thing
set POLICY_NAME=%PREFIX%_policy
set ROLE_NAME=%PREFIX%_role
set ROLE_ALIAS=%PREFIX%_role_alias
set CERT=%PREFIX%_certificate.pem
set PUBLIC_KEY=%PREFIX%_public.key
set PRIVATE_KEY=%PREFIX%_private.key

rem Create the certificate and output to a file
aws iot create-keys-and-certificate --set-as-active --certificate-pem-outfile %CERT% --public-key-outfile %PUBLIC_KEY% --private-key-outfile %PRIVATE_KEY% > certificate.json

rem Parse the certificate ARN using jq and set it as a variable
for /f "delims=" %%a in ('jq --raw-output ".certificateArn" certificate.json') do set CERT_ARN=%%a

rem Attach the IoT policy to the certificate
aws iot attach-policy --policy-name %POLICY_NAME% --target %CERT_ARN%

rem Attach the IoT thing to the certificate
aws iot attach-thing-principal --thing-name %THING_NAME% --principal %CERT_ARN%

rem Get the IoT credentials endpoint and save to a file
aws iot describe-endpoint --endpoint-type iot:CredentialProvider --output text > iot-credential-provider.txt

rem Download the CA certificate required for TLS
curl "https://www.amazontrust.com/repository/SFSRootCAG2.pem" -o cacert.pem

rem Set environment variables for session use
set AWS_IOT_CORE_CREDENTIAL_ENDPOINT=<iot-credential-provider.txt
set AWS_IOT_CORE_CERT=%CD%\%CERT%
set AWS_IOT_CORE_PRIVATE_KEY=%CD%\%PRIVATE_KEY%
set AWS_IOT_CORE_ROLE_ALIAS=%ROLE_ALIAS%
set AWS_IOT_CORE_THING_NAME=%THING_NAME%

endlocal