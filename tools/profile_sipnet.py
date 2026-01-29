"""
SIPNET Automated Profiling Tool
-------------------------------
1. Generates a large climate dataset.
2. Temporarily injects #define PROFILING 1 into sipnet.c.
3. Compiles and runs the model.
4. Cleans up the C code and reports results.
"""

import os
import subprocess
import sys

# Paths relative to the script location
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

CLIM_FILE = os.path.join(REPO_ROOT, "tests/smoke/niwot/sipnet.clim")
LARGE_CLIM = os.path.join(REPO_ROOT, "tests/smoke/niwot/sipnet_large.clim")
CONFIG_FILE = os.path.join(REPO_ROOT, "tests/smoke/niwot/sipnet.in")
SOURCE_FILE = os.path.join(REPO_ROOT, "src/sipnet/sipnet.c")

DEFINE_LINE = "#define PROFILING 1 // Auto-injected by profile_sipnet.py\n"


def generate_large_met(years_target=10):
    print(f"[*] Generating {years_target}-year met series for profiling...")
    if not os.path.exists(CLIM_FILE):
        print(f"Error: Could not find {CLIM_FILE}")
        sys.exit(1)

    with open(CLIM_FILE, "r") as f:
        lines = f.readlines()

    data = [l.split() for l in lines if l.strip()]

    years = []
    for row in data:
        try:
            years.append(int(row[1]))
        except ValueError:
            continue

    if not years:
        print("Error: No valid year data found in climate file.")
        sys.exit(1)

    span = max(years) - min(years) + 1
    cycles = (years_target // span) + 1

    with open(LARGE_CLIM, "w") as f:
        for cycle in range(cycles):
            offset = cycle * span
            for row in data:
                new_row = row[:]
                try:
                    new_row[1] = str(int(new_row[1]) + offset)
                    f.write("\t".join(new_row) + "\n")
                except ValueError:
                    continue  # Skip headers in duplication
    print(f"[+] Scaled dataset created: {LARGE_CLIM}")


def inject_profiling_flag():
    print("[*] Injecting #define PROFILING into source code...")
    with open(SOURCE_FILE, "r") as f:
        lines = f.readlines()

    # Check if already injected to avoid duplicates
    if DEFINE_LINE in lines:
        return

    # Insert at the very top
    lines.insert(0, DEFINE_LINE)

    with open(SOURCE_FILE, "w") as f:
        f.writelines(lines)


def remove_profiling_flag():
    print("[*] Cleaning up source code...")
    try:
        with open(SOURCE_FILE, "r") as f:
            lines = f.readlines()

        # Remove the injected line
        lines = [line for line in lines if line != DEFINE_LINE]

        with open(SOURCE_FILE, "w") as f:
            f.writelines(lines)
    except Exception as e:
        print(f"Warning: Failed to clean up sipnet.c: {e}")


def build_sipnet():
    print("[*] Compiling SIPNET...")

    try:
        subprocess.run(
            ["make", "clean"], cwd=REPO_ROOT, check=True, stdout=subprocess.DEVNULL
        )
        subprocess.run(["make"], cwd=REPO_ROOT, check=True)
    except subprocess.CalledProcessError:
        print("[-] Build failed. Restoring source code...")
        remove_profiling_flag()  # Ensure cleanup on fail
        sys.exit(1)


def run_and_parse():
    print("[*] Running simulation...")
    with open(CONFIG_FILE, "r") as f:
        original_content = f.read()

    try:
        # Point config to large climate file
        with open(CONFIG_FILE, "w") as f:
            f.write(original_content.replace("sipnet.clim", "sipnet_large.clim"))

        # Run from test dir so relative paths work
        cmd = ["../../../sipnet", "-i", "sipnet.in"]
        result = subprocess.run(
            cmd, cwd=os.path.dirname(CONFIG_FILE), capture_output=True, text=True
        )

        if result.returncode != 0:
            print(f"Error running SIPNET: {result.stderr}")

            print(result.stdout)
            return

        # Parse Output
        output = result.stdout
        if "--- SIPNET PERFORMANCE PROFILE ---" in output:
            stats = output.split("--- SIPNET PERFORMANCE PROFILE ---")[1].strip()
            print("\n" + "*" * 40 + "\nMODEL PERFORMANCE REPORT\n" + "*" * 40)
            print(stats)
            print("*" * 40 + "\n")
        else:
            print("[-] Profiling stats not found in output.")
            print("debug output:", output[-500:])  # Show last 500 chars

    finally:
        # Restore config
        with open(CONFIG_FILE, "w") as f:
            f.write(original_content)


if __name__ == "__main__":
    try:
        generate_large_met(years_target=50)
        inject_profiling_flag()
        build_sipnet()
        run_and_parse()
    finally:
        # Always cleanup, even if errors occur
        remove_profiling_flag()
