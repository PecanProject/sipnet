#!/bin/bash

# Store the starting directory
START_DIR="$(pwd)"

# Find all immediate subdirectories
DIRECTORIES=()
while IFS= read -r -d '' dir; do
    DIRECTORIES+=("$dir")
done < <(find "$START_DIR" -mindepth 1 -maxdepth 1 -type d -print0)

# Exit immediately if a command fails (optional)
#set -e

# Initialize counters
sipnet_pass_count=0
sipnet_fail_count=0
event_pass_count=0
event_fail_count=0
skip_count=0

# Loop through each directory
for DIR in "${DIRECTORIES[@]}"; do
    echo "Running sipnet in: $DIR"

    if [ ! -d "$DIR" ]; then
        echo "Directory $DIR does not exist. Skipping."
        ((skip_count++))
        continue
    fi

    cd "$DIR" || { echo "Failed to change directory to $DIR"; ((skip_count++)); continue; }

    # Run sipnet
    # TEMP HACK to make this run until run-time options and config are in place.
    # NOTE THAT THIS WILL NOT TEST ANY CHANGES
    if [ -f "./sipnet" ]; then
      # If a pre-compiled version exists in this dir, run it
        ./sipnet -i sipnet.in
    else
      # This is the command that all directories will run when options/config are in
        ../../../sipnet -i sipnet.in
    fi

    # Compare output with expected output; clean up files if no diffs found
    if diff -q sipnet.out sipnet.exp > /dev/null; then
        echo "[$DIR] ✅ No differences found in sipnet output"
        ((sipnet_pass_count++))
        rm sipnet.out
    else
        echo "[$DIR] ❌ Differences found in sipnet output"
        ((sipnet_fail_count++))
    fi
    if diff -q events.out events.exp > /dev/null; then
        echo "[$DIR] ✅ No differences found in event output"
        ((event_pass_count++))
        rm events.out
    else
        echo "[$DIR] ❌ Differences found in event output"
        ((event_fail_count++))
    fi

    echo ""  # Blank line for readability

    # Return to the starting directory
    cd "$START_DIR"
done

# Final return to starting directory (in case of premature exit)
cd "$START_DIR"

# Summary
echo "======================="
echo "SUMMARY:"
echo "Skipped directories: $skip_count"
echo "SIPNET OUTPUT:"
echo "Passed:  $sipnet_pass_count"
echo "Failed:  $sipnet_fail_count"
echo "EVENT OUTPUT:"
echo "Passed:  $event_pass_count"
echo "Failed:  $event_fail_count"
echo "======================="

exit_code=0

# Set a failure exit code if there were either fails or skips
if  [ "$skip_count" -gt 0 ]; then
    echo "The smoke test directory structure has changed. Please fix and re-run."
    (( exit_code |= 1))
fi

if [ "$sipnet_fail_count" -gt 0 ] || [ "$event_fail_count" -gt 0 ]; then
    echo "::error title={Output Changed}::Some outputs have changed. This is expected with some changes to SIPNET. When this happens, assess correctness and then update the reference output files. See the report above for details."
    (( exit_code |= 1))
fi

exit $exit_code
