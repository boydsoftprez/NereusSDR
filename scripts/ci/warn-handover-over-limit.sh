#!/usr/bin/env bash
# Print a CI warning for each handover step that passed its 15 s requirement,
# from the test log ctest wrote (default Testing/Temporary/LastTest.log).
#
# tst_station_handover_roundtrip reports such a time without failing on a
# runner that may be shared, and ctest --output-on-failure hides the output of
# a test that passes. Always exits 0: this reports, and ctest's result is the
# step's result.
log="${1:-Testing/Temporary/LastTest.log}"
if [ -f "$log" ]; then
    { grep -a 'handover .* took [0-9]* ms, over the [0-9]* ms requirement' "$log" || true; } \
        | while IFS= read -r line; do
              printf '::warning::%s\n' "$line"
          done
fi
exit 0
