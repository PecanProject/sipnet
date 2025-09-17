#!/bin/bash

# Store the starting directory
START_DIR="$(pwd)"

# Change to the script directory
cd "$(dirname "$0")" || { echo "Failed to change to script directory to $SCRIPT_DIR"; exit 1; }
SCRIPT_DIR="$(pwd)"
echo "Changed to script directory $SCRIPT_DIR"

# Initialize counters
num_tests=0
sipnet_pass_count=0
sipnet_fail_count=0
event_pass_count=0
event_fail_count=0
config_pass_count=0
config_fail_count=0
skip_count=0
forced_pass_count=0

# Find all immediate subdirectories
DIRECTORIES=()
while IFS= read -r -d '' dir; do
    DIRECTORIES+=("$dir")
    ((num_tests++))
done < <(find . -mindepth 1 -maxdepth 1 -type d -print0)

# Sort the directories
IFS=$'\n' SORTED_DIRS=($(sort <<<"${DIRECTORIES[*]}")); unset IFS

# Exit immediately if a command fails (optional)
#set -e

# Loop through each directory
for DIR in "${SORTED_DIRS[@]}"; do
    echo "Running sipnet in: $DIR"

    if [ ! -d "$DIR" ]; then
        echo "Directory $DIR does not exist. Skipping."
        ((skip_count++))
        continue
    fi

    cd "$DIR" || { echo "Failed to change directory to $DIR"; ((skip_count++)); continue; }

    # Run sipnet
    # Note that we have a mechanism here to force pass a test; please use
    # hesitantly, and as a temporary measure while hopefully investigations
    # are proceeding as to why this is needed.
    if [ -f "./skip" ]; then
        echo "FORCED PASS for this test"
        echo ""
        ((sipnet_pass_count++))
        ((event_pass_count++))
        ((config_pass_count++))
        ((forced_pass_count++))
        cd "$SCRIPT_DIR" || { echo "Failed to change directories, exiting"; exit 1; }
        continue
    else
        pwd
        # This is the command that all directories will run when options/config are in
        ../../../sipnet -i sipnet.in

        # Make sure the program actually ran successfully
        if [ $? -ne 0 ]; then
          echo "Error: The sipnet run failed. Marking everything as ❌ for this test."
          # Mark everything as failed for this test, the continue
          ((sipnet_fail_count++))
          ((event_fail_count++))
          ((config_fail_count++))
          cd "$SCRIPT_DIR" || { echo "Failed to change directories, exiting"; exit 1; }
          echo ""
          continue
        fi
    fi

    # Compare output with expected output; clean up files if no diffs found
    if git diff --exit-code sipnet.out > /dev/null; then
        echo "[$DIR] ✅ No differences found in sipnet output"
        ((sipnet_pass_count++))
        #rm sipnet.out
    else
        echo "[$DIR] ❌ Differences found in sipnet output"
        ((sipnet_fail_count++))
    fi
    if git diff --exit-code events.out > /dev/null; then
        echo "[$DIR] ✅ No differences found in event output"
        ((event_pass_count++))
        #rm events.out
    else
        echo "[$DIR] ❌ Differences found in event output"
        ((event_fail_count++))
    fi
    # For the config check, exclude the first line with the timestamp
    if git diff --exit-code -I "Final config for SIPNET run" sipnet.config > /dev/null; then
        echo "[$DIR] ✅ No differences found in config output"
        ((config_pass_count++))
        # The file will appear different to git due to timestamp, so revert
        # it if we've passed
        git restore sipnet.config
    else
        echo "[$DIR] ❌ Differences found in config output"
        ((config_fail_count++))
    fi

    echo ""  # Blank line for readability

    # Return to the starting directory
    cd "$SCRIPT_DIR" || { echo "Failed to change directories, exiting"; exit 1; }
done

# Final return to starting directory (in case of premature exit)
cd "$START_DIR" || { echo "Failed to change directories (ignorable)"; }

if [ "$forced_pass_count" -gt 0 ]; then
  forced_str=" ($forced_pass_count forced)"
else
  forced_str=""
fi

# Summary
echo "======================="
echo "SUMMARY:"
echo "Skipped directories: $skip_count"
echo "SIPNET OUTPUT:"
echo "Passed:  $sipnet_pass_count/$num_tests$forced_str"
echo "Failed:  $sipnet_fail_count"
echo "EVENT OUTPUT:"
echo "Passed:  $event_pass_count/$num_tests$forced_str"
echo "Failed:  $event_fail_count"
echo "CONFIG OUTPUT:"
echo "Passed:  $config_pass_count/$num_tests$forced_str"
echo "Failed:  $config_fail_count"
echo "======================="

exit_code=0

# Set a failure exit code if there were either fails or skips
if  [ "$skip_count" -gt 0 ]; then
    echo "The smoke test directory structure has changed. Please fix and re-run."
    (( exit_code |= 1))
fi

if [ "$sipnet_fail_count" -gt 0 ] || [ "$event_fail_count" -gt 0 ] || [ "$config_fail_count" -gt 0 ]; then
    echo "Some outputs have changed. This is expected with some changes to SIPNET. When this happens, assess correctness and then update the reference output files. See the report above for details."
    (( exit_code |= 1))
fi

exit $exit_code
