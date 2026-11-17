#!/usr/bin/env bash
# Exercise precipitation routing through the production CLI and output files.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
RUN_DIR="$(mktemp -d)"
trap 'rm -rf "$RUN_DIR"' EXIT

# Start with 3 cm soil water, no snow, and no immediate evaporation.
sed -E 's/^snowInit .*/snowInit 0/; s/^soilWFracInit .*/soilWFracInit 0.25/; s/^immedEvapFrac .*/immedEvapFrac 0/' \
    "$ROOT_DIR/tests/smoke/russell_1/sipnet.param" > "$RUN_DIR/sipnet.param"
touch "$RUN_DIR/sipnet.in"
cd "$RUN_DIR"

status=0
for temperature in -5 0 5; do
    for flag in --no-snow --snow; do
        snow_enabled=0
        soil_water=4
        snow_water=0
        if [[ "$flag" == --snow ]]; then
            snow_enabled=1
            if (( temperature <= 0 )); then
                soil_water=3
                snow_water=1
            fi
        fi

        echo "2016 1 0 1 $temperature 0 0 10 500 0 600 1" > sipnet.clim
        if ! "$ROOT_DIR/sipnet" "$flag" --no-events --dump-config > run.log 2>&1; then
            cat run.log
            echo "FAIL: $flag at $temperature C (run failed)"
            status=1
            continue
        fi

        if awk -v expected="$snow_enabled" '
            $1 == "SNOW" {count++; if ($2 != "COMMAND_LINE" || $3 != expected) bad=1}
            END {exit (count != 1 || bad)}' sipnet.config &&
            awk -v soil="$soil_water" -v snow="$snow_water" '
            NR == 1 {
                for (i=1; i<=NF; i++) col[$i]=i
                if (!col["soilWater"] || !col["snow"]) {bad=1; exit}
                next
            }
            {if ($(col["soilWater"]) != soil || $(col["snow"]) != snow) bad=1}
            END {exit (bad || NR != 2)}' sipnet.out; then
            echo "PASS: $flag at $temperature C (soil=$soil_water cm, snow=$snow_water cm)"
        else
            echo "FAIL: $flag at $temperature C (expected soil=$soil_water cm, snow=$snow_water cm)"
            cat sipnet.out
            status=1
        fi
    done
done
exit "$status"
