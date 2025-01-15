#!/bin/bash
# Run this script from the root SIPNET directory with the command:
# > tests/update_model_structures.sh

# Name of the file containing #define compiler switches
STRUCTURES_FILE="modelStructures.h"

# Get the path to the current directory's STRUCTURES_FILE - that is,
# the production version
PROD_MODEL_STRUCTURES="./${STRUCTURES_FILE}"

# Check if the current directory has the base model_structures.h
if [[ ! -f $PROD_MODEL_STRUCTURES ]]; then
    echo "Error: ${STRUCTURES_FILE} not found in the current directory."
    exit 1
fi

# Define the awk script to update #define values; this will be called on each test
# version of STRUCTURES_FILE
read -r -d '' AWK_SCRIPT << 'EOF'
BEGIN {
    FS = "[ \t]+";  # Set field separator to handle spaces or tabs
    define_regex = "^#define";
    skip_regex = "MODEL_STRUCTURES_H";
}

# Process the test structures file and store the #define values in an associative array
FNR == NR {
    if ($1 ~ define_regex && $2 !~ skip_regex) {
        name = $2;   # The second field is the name of the #define
        value = $3;  # The third field is the value of the #define
        defines[name] = value;
    }
    next;
}

# Update the new file with the old #define values
{
    if ($1 ~ define_regex && $2 !~ skip_regex) {
        name = $2;
        if (name in defines) {
            printf "#define %s %s\n", name, defines[name];
            next;
        }
    }
    # Print the line as-is if no overwrite is needed
    print $0;
}
EOF

# Find and process all target files in subdirectories
find . -type f -name "$STRUCTURES_FILE" -mindepth 2 | while read -r file; do
    echo "Processing: $file"

    # Create a backup of the original file
    cp "$file" "${file}.bak"

    # Add a warning at the top, then update defines
    {
        echo "//"
        echo "// DO NOT ADD CUSTOM CODE TO THIS FILE (other than #define overrides for unit tests)"
        echo "// IT MAY BE OVERWRITTEN BY update_model_structures.sh"
        cat "$PROD_MODEL_STRUCTURES"
    } | awk "$AWK_SCRIPT" "$file" - > "${file}.tmp"

    # Overwrite the target file with the updated content
    mv "${file}.tmp" "$file"

    echo "Updated: $file"
done
