#!/bin/bash

# Define flag combinations as tuples with expected results
# Format: "flags expected_result"
# - expected_result is either OK or FAIL
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
)

# Initialize result counters
TOTAL_TESTS=${#COMBINATIONS[@]}
PASSED=0
FAILED=0

# Markdown summary header
SUMMARY="# CMake Flag Test Summary\n"

# Test each combination
for combination in "${COMBINATIONS[@]}"; do
  # Extract flags and expected result from the tuple
  FLAGS="${combination% *}"      # Everything except the last word
  EXPECTED_RESULT="${combination##* }"  # The last word

  # Clean up the build directory before each test
  rm -rf build
  mkdir build

  # Run only the CMake configuration with the current combination
  cmake -B build $FLAGS -DBUILD_DEPENDENCIES=OFF > /dev/null 2>&1
  EXIT_CODE=$?

  # Check if the exit code matches the expected result
  if [[ "$EXPECTED_RESULT" == "FAIL" ]]; then
    if [[ $EXIT_CODE -ne 0 ]]; then
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
