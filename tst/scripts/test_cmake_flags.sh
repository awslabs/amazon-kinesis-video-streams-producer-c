#!/bin/bash

# Define flag combinations as tuples with expected results
# Format: "flags expected_result[|expected_message]"
# - expected_result is either OK or FAIL
# - expected_message (optional, FAIL only): text the configure output must contain, so the
#   case only passes when configure fails for the intended reason. Matched after collapsing
#   whitespace, since CMake wraps long error messages across lines.
COMBINATIONS=(
  "-DAWS_KVS_IPV4_ONLY=1 OK"
  "-DAWS_KVS_IPV6_ONLY=TRUE OK"
  "-DAWS_KVS_IPV4_AND_IPV6_ONLY=true OK"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV6_ONLY=YES FAIL"
  "-DAWS_KVS_IPV6_ONLY=ON -DAWS_KVS_IPV4_AND_IPV6_ONLY=ON FAIL"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV4_AND_IPV6_ONLY=ON FAIL"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV6_ONLY=ON -DAWS_KVS_IPV4_AND_IPV6_ONLY=ON FAIL"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV6_ONLY=N OK"
  "-DAWS_KVS_IPV4_ONLY=on -DAWS_KVS_IPV6_ONLY=NO OK"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV6_ONLY=false OK"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV6_ONLY=ignore OK"
  "-DAWS_KVS_IPV4_ONLY=ON -DAWS_KVS_IPV6_ONLY=0 OK"

  "-DAWS_KVS_USE_LEGACY_ENDPOINT_ONLY=ON OK"
  "-DAWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY=ON OK"
  "-DAWS_KVS_USE_LEGACY_ENDPOINT_ONLY=ON -DAWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY=ON FAIL"
  "-DAWS_KVS_USE_LEGACY_ENDPOINT_ONLY=1 -DAWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY=TRUE FAIL"

  # Crypto backend selection: exactly one of USE_OPENSSL (default ON), USE_MBEDTLS,
  # USE_AWS_LC, USE_AWS_LC_FIPS. Configure-only, so the FIPS Go toolchain check
  # (dependency build time) is not exercised here. With BUILD_DEPENDENCIES=OFF and no
  # AWS-LC install, selecting AWS-LC must fail too: configure must not silently accept
  # the system OpenSSL in its place. The two single-backend cases also require the
  # "is not AWS-LC" error, so they need a system OpenSSL (libssl-dev in CI) to be found.
  "-DUSE_OPENSSL=OFF -DUSE_AWS_LC=ON FAIL|is not AWS-LC"
  "-DUSE_OPENSSL=OFF -DUSE_AWS_LC_FIPS=ON FAIL|is not AWS-LC"
  "-DUSE_AWS_LC=ON FAIL"
  "-DUSE_AWS_LC_FIPS=ON FAIL"
  "-DUSE_OPENSSL=OFF -DUSE_AWS_LC=ON -DUSE_AWS_LC_FIPS=ON FAIL"
  "-DUSE_OPENSSL=OFF -DUSE_MBEDTLS=ON -DUSE_AWS_LC=ON FAIL"
  "-DUSE_OPENSSL=OFF FAIL"
)

# Initialize result counters
TOTAL_TESTS=${#COMBINATIONS[@]}
PASSED=0
FAILED=0

# Markdown summary header
SUMMARY="# CMake Flag Test Summary\n"

# Test each combination
for combination in "${COMBINATIONS[@]}"; do
  # Split off the optional expected message, then extract flags and expected result
  EXPECTED_MESSAGE=""
  if [[ "$combination" == *"|"* ]]; then
    EXPECTED_MESSAGE="${combination#*|}"
    combination="${combination%%|*}"
  fi
  FLAGS="${combination% *}"      # Everything except the last word
  EXPECTED_RESULT="${combination##* }"  # The last word

  # Clean up the build directory before each test
  rm -rf build
  mkdir build

  # Run only the CMake configuration with the current combination
  CMAKE_OUTPUT=$(cmake -B build $FLAGS -DBUILD_DEPENDENCIES=OFF 2>&1)
  EXIT_CODE=$?

  # Check if the exit code matches the expected result
  if [[ "$EXPECTED_RESULT" == "FAIL" ]]; then
    if [[ $EXIT_CODE -ne 0 && -n "$EXPECTED_MESSAGE" ]] &&
      ! echo "$CMAKE_OUTPUT" | tr -s ' \n' '  ' | grep -qF -- "$EXPECTED_MESSAGE"; then
      FAILED=$((FAILED + 1))
      SUMMARY+="* ❌ \`$FLAGS\`: Failed, but output lacks \"$EXPECTED_MESSAGE\".\n"
    elif [[ $EXIT_CODE -ne 0 ]]; then
      PASSED=$((PASSED + 1))
      SUMMARY+="* ✅ \`$FLAGS\`: Correctly failed.\n"
    else
      FAILED=$((FAILED + 1))
      SUMMARY+="* ❌ \`$FLAGS\`: Expected failure but succeeded.\n"
    fi
  else
    if [[ $EXIT_CODE -ne 0 ]]; then
      FAILED=$((FAILED + 1))
      SUMMARY+="* ❌ \`$FLAGS\`: Unexpected failure.\n"
    else
      PASSED=$((PASSED + 1))
      SUMMARY+="* ✅ \`$FLAGS\`: Success as expected.\n"
    fi
  fi
done

# Generate a Markdown summary
SUMMARY+="\n### Total Tests: $TOTAL_TESTS\n"
SUMMARY+="### Passed: $PASSED\n"
SUMMARY+="### Failed: $FAILED\n"

echo -e "$SUMMARY"

if [[ $FAILED -gt 0 ]]; then
  exit 1
fi
