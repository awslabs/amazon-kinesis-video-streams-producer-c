#!/bin/bash
#
# CI helper: shorten the producer's streaming-token rotation period so the
# token-rotation sample exercises a rotation within a ~2 minute run.
#
#   .github/scripts/shorten-token-rotation.sh [kvspic-src-dir]
#
# Rewrites one line in the fetched PIC source (StreamEvent.c) so the streaming
# token expiration is capped at MIN_STREAMING_TOKEN_EXPIRATION_DURATION instead
# of MAX_ENFORCED_TOKEN_EXPIRATION_DURATION. Fails if the line is missing or
# ambiguous, so a PIC upstream change surfaces here rather than as a silent
# no-op. The caller rebuilds afterwards.
#
# Default kvspic-src-dir is the location the SDK's own CMake fetches PIC into.

set -euo pipefail

file_path="${1:-./dependency/libkvspic/kvspic-src}/src/client/src/StreamEvent.c"
string_to_replace='pKinesisVideoStream->streamingAuthInfo.expiration = MIN(expiration, currentTime + MAX_ENFORCED_TOKEN_EXPIRATION_DURATION);'
string_to_replace_with='pKinesisVideoStream->streamingAuthInfo.expiration = MIN(expiration, currentTime + MIN_STREAMING_TOKEN_EXPIRATION_DURATION);'

set +e
echo "Checking if the pattern exists before replacement:"
occurrences_of_old=$(grep -F -c "$string_to_replace" "$file_path")
if [[ "$occurrences_of_old" -gt 1 ]]; then
  echo "There is more than 1 of: \`$string_to_replace\` in \`$file_path\`!"
  exit 1
elif [[ "$occurrences_of_old" -eq 0 ]]; then
  echo "Was not able to find: \`$string_to_replace\` in \`$file_path\`!"
  exit 1
fi
occurrences_of_new_before=$(grep -F -c "$string_to_replace_with" "$file_path")
echo "Yes"
set -e

# Replace the line, while preserving the indentation
temp_file=$(mktemp "$(dirname "$file_path")/StreamEvent.c.XXXXXX")
awk -v old="$string_to_replace" -v new="$string_to_replace_with" \
  '{
    match($0, /^[[:space:]]+/);                       # Find leading spaces
    leading_spaces = substr($0, RSTART, RLENGTH);     # Extract them
    trimmed = $0; sub(/^[[:space:]]+/, "", trimmed);  # Trim leading spaces
    if (trimmed == old) print leading_spaces new;     # Preserve leading spaces
    else print $0;                                    # Print original lines
  }' "$file_path" > "$temp_file"
# Copy back through the original file so its mode is preserved (mktemp creates 0600)
cat "$temp_file" > "$file_path"
rm -f "$temp_file"

grep "EXPIRATION" "$file_path"

set +e
echo "Checking if the replacement was successful:"
occurrences_of_new_after=$(grep -F -c "$string_to_replace_with" "$file_path")
if [[ "$occurrences_of_new_after" -eq "$occurrences_of_new_before" ]]; then
  echo "Replacement failed - Nothing changed!"
  exit 1
fi
echo "Yes"
