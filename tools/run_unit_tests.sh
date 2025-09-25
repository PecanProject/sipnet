#!/usr/bin/env bash
#
# Run `make test` once, then iterate over ./test/sipnet subdirectories:
#   (i) skip if no Makefile
#   (ii) get TEST_CFILES from Makefile
#   (iii) strip ".c"
#   (iv) cd into dir, run each executable, capture last line + exit code
#   (v) summarize results at the end (colorized)
#

# ANSI colors
RED="\033[0;31m"
GREEN="\033[0;32m"
YELLOW="\033[1;33m"
RESET="\033[0m"

# Step 1: Build everything first
echo "Running make testbuild..."
if ! make testbuild; then
    echo -e "${RED}Build failed, exiting.${RESET}"
    exit 1
fi
echo -e "${GREEN}Build succeeded.${RESET}"

# Arrays to store results
declare -a results

# Step 2: Search only under ./test/sipnet
for d in ./tests/sipnet/*/ ; do
    [ -d "$d" ] || continue

    if [ ! -f "$d/Makefile" ]; then
        echo "Skipping $d (no Makefile)"
        continue
    fi

    echo "Processing $d"
    pushd "$d" > /dev/null || continue

    # Extract TEST_CFILES from Makefile
    files=$(make -pn | awk -F'[:=]' '/^TEST_CFILES[ \t]*[+:]?=/ {print $2}' | xargs)

    if [ -z "$files" ]; then
        echo "  No TEST_CFILES found"
        popd > /dev/null
        continue
    fi

    # Strip .c extension
    executables=$(echo "$files" | sed 's/\.c//g')

    for exe in $executables; do
        if [ ! -x "./$exe" ]; then
            echo "  Skipping $exe (not found or not executable)"
            results+=("$d/$exe|MISSING|N/A")
            continue
        fi

        echo "  Running $exe"
        output=$(./"$exe" 2>&1)
        exit_code=$?
        last_line=$(echo "$output" | tail -n 1)

        echo "    Last line: $last_line"
        echo "    Exit code: $exit_code"

        # Save for summary
        results+=("$d/$exe|$exit_code|$last_line")
    done

    popd > /dev/null
done

# Step 3: Print summary
echo
echo "======================= TEST SUMMARY =========================="
printf "| %-50s | %-6s |\n" "Executable" "Result"
echo "---------------------------------------------------------------"

for entry in "${results[@]}"; do
    IFS="|" read -r path code last <<< "$entry"
    exe_name=$(basename "$path")
    dir_name=$(dirname "$path")
    test_dir_name=$(basename "$dir_name")

    if [[ "$code" == "MISSING" ]]; then
        result="${YELLOW}MISSING${RESET}"
    elif [[ "$code" -eq 0 ]]; then
        result="${GREEN}PASS${RESET}"
    else
        result="${RED}FAIL ($code)${RESET}"
    fi

    printf "| %-50s |  %-8b  |\n" "$test_dir_name/$exe_name" "$result"
    # printf "%-50s | %-7b | %s\n" "$test_dir_name/$exe_name" "$result" "$last"
done

echo "==============================================================="
