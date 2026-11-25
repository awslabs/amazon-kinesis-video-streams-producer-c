#!/bin/bash
#
# Verifies which crypto backend a build actually linked, from the artifacts alone.
#
#   tst/scripts/check_crypto_backend.sh <openssl|aws-lc|aws-lc-fips> <build-dir> [open-source-lib-dir]
#
# Checks, all of which must agree with the requested backend:
#   1. The AWS-LC-only TLS code (kvsCurlSslCtxCallback, which installs the PQC
#      groups list) is present in the SDK libraries for aws-lc and ABSENT for
#      openssl. This is the "default build is inert" guarantee: enabling
#      USE_AWS_LC must not leak into builds that did not ask for it.
#   2. The crypto library the SDK was built against identifies itself as AWS-LC
#      (version string "AWS-LC x.y.z"), as the AWS-LC FIPS module ("AWS-LC FIPS
#      x.y.z", aws-lc-fips) or as OpenSSL, when the dependency install directory
#      is available. aws-lc and aws-lc-fips are distinct: the two AWS-LC lines
#      install identical headers and library names, so a FIPS build that picked up
#      a stale non-FIPS install (or the reverse) only shows here.
#   3. For aws-lc and aws-lc-fips, the probe binary kvsTlsKeyExchangeProbe was built.
#
# Works on Linux and macOS, for shared and static builds (nm handles both). For a
# cross-compiled build set NM and STRINGS to the target toolchain's binutils, e.g.
# NM=aarch64-linux-gnu-nm STRINGS=aarch64-linux-gnu-strings.
# Exit code 0 when every check agrees, 1 otherwise.

set -u

EXPECTED="${1:-}"
BUILD_DIR="${2:-}"
OPEN_SRC_LIB="${3:-}"
NM="${NM:-nm}"
STRINGS="${STRINGS:-strings}"

if [[ "$EXPECTED" != "openssl" && "$EXPECTED" != "aws-lc" && "$EXPECTED" != "aws-lc-fips" ]] || [[ -z "$BUILD_DIR" ]]; then
  echo "usage: $0 <openssl|aws-lc|aws-lc-fips> <build-dir> [open-source-lib-dir]" >&2
  exit 2
fi
# aws-lc and aws-lc-fips share every check except the libcrypto identity
IS_AWS_LC=0
[[ "$EXPECTED" == "aws-lc" || "$EXPECTED" == "aws-lc-fips" ]] && IS_AWS_LC=1

FAILED=0
pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; FAILED=1; }

# --- 1. SDK library symbols ---------------------------------------------------
SDK_LIBS=$(ls "$BUILD_DIR"/libkvsCommonCurl.* "$BUILD_DIR"/libcproducer.* 2>/dev/null | grep -E '\.(a|so|dylib)$' || true)
if [[ -z "$SDK_LIBS" ]]; then
  fail "no SDK libraries found in $BUILD_DIR"
else
  AWS_LC_SYMBOL_HITS=0
  for lib in $SDK_LIBS; do
    if "$NM" -g "$lib" 2>/dev/null | grep -q 'kvsCurlSslCtxCallback'; then
      AWS_LC_SYMBOL_HITS=$((AWS_LC_SYMBOL_HITS + 1))
    fi
  done
  if [[ "$IS_AWS_LC" -eq 1 ]]; then
    if [[ "$AWS_LC_SYMBOL_HITS" -gt 0 ]]; then
      pass "kvsCurlSslCtxCallback (PQC groups list) present in SDK libraries"
    else
      fail "kvsCurlSslCtxCallback missing: AWS-LC build did not compile the PQC TLS path"
    fi
  else
    if [[ "$AWS_LC_SYMBOL_HITS" -eq 0 ]]; then
      pass "no AWS-LC-only symbols in the SDK libraries (default build is unaffected by USE_AWS_LC)"
    else
      fail "kvsCurlSslCtxCallback found in an OpenSSL build: AWS-LC code leaked into the default build"
    fi
  fi
fi

# --- 2. Crypto library identity -------------------------------------------------
if [[ -n "$OPEN_SRC_LIB" ]]; then
  CRYPTO_LIB=$(ls "$OPEN_SRC_LIB"/libcrypto.* 2>/dev/null | grep -E '\.(a|so|dylib)(\.[0-9.]+)?$' | head -1 || true)
  if [[ -z "$CRYPTO_LIB" ]]; then
    fail "no libcrypto found in $OPEN_SRC_LIB"
  else
    VERSION_TEXT=$("$STRINGS" "$CRYPTO_LIB" | grep -m1 -E 'AWS-LC (FIPS )?[0-9]+\.[0-9]+\.[0-9]+|^OpenSSL [0-9]+\.[0-9]+\.[0-9]+' || true)
    echo "libcrypto version string: ${VERSION_TEXT:-<none>}"
    if [[ "$EXPECTED" == "aws-lc-fips" ]]; then
      if [[ "$VERSION_TEXT" == *"AWS-LC FIPS"* ]]; then
        pass "libcrypto is the AWS-LC FIPS module"
      else
        fail "libcrypto is not the AWS-LC FIPS module (a non-FIPS AWS-LC may have been picked up from a stale prefix)"
      fi
    elif [[ "$EXPECTED" == "aws-lc" ]]; then
      if [[ "$VERSION_TEXT" == *"AWS-LC"* && "$VERSION_TEXT" != *"AWS-LC FIPS"* ]]; then
        pass "libcrypto is AWS-LC (non-FIPS line)"
      elif [[ "$VERSION_TEXT" == *"AWS-LC FIPS"* ]]; then
        fail "libcrypto is the AWS-LC FIPS module in a non-FIPS AWS-LC build (stale prefix?)"
      else
        fail "libcrypto is not AWS-LC"
      fi
    else
      if [[ "$VERSION_TEXT" == OpenSSL* && "$VERSION_TEXT" != *"AWS-LC"* ]]; then
        pass "libcrypto is OpenSSL"
      else
        fail "libcrypto is not plain OpenSSL (an AWS-LC library may have been picked up)"
      fi
    fi
  fi
else
  echo "SKIP: libcrypto identity (no open-source lib dir given)"
fi

# --- 3. Probe binary ----------------------------------------------------------------
if [[ "$IS_AWS_LC" -eq 1 ]]; then
  if [[ -x "$BUILD_DIR/kvsTlsKeyExchangeProbe" ]]; then
    pass "kvsTlsKeyExchangeProbe built"
  else
    fail "kvsTlsKeyExchangeProbe not built"
  fi
else
  if [[ -e "$BUILD_DIR/kvsTlsKeyExchangeProbe" ]]; then
    fail "kvsTlsKeyExchangeProbe built in an OpenSSL build"
  else
    pass "kvsTlsKeyExchangeProbe not built (as expected for OpenSSL)"
  fi
fi

if [[ "$FAILED" -ne 0 ]]; then
  echo "Crypto backend check FAILED for expected backend '$EXPECTED'"
  exit 1
fi
echo "Crypto backend check passed for '$EXPECTED'"
