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
    ../../../sipnet -i sipnet.in

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

# Set a failure exit code if there were either fails or skips
if [ "$sipnet_fail_count" -gt 0 ] || [ "$event_fail_count" -gt 0 ] || [ "$skip_count" -gt 0 ]; then
    exit 1
else
    exit 0
fi
