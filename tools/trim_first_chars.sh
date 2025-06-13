#!/bin/bash
# Trims the first num_chars character from each row of input_file, and
# writes the output to output_file. Overwrites output_file if it exists.

# Check if the correct number of arguments are provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 num_chars input_file output_file"
    exit 1
fi

num_chars="$1"
input_file="$2"
output_file="$3"

# Validate that num_chars is a positive integer
if ! [[ "$num_chars" =~ ^[0-9]+$ ]]; then
    echo "Error: num_chars must be a non-negative integer."
    exit 1
fi

# Clear the output file if it exists, or create it
> "$output_file"

# Process the file
while IFS= read -r line; do
    echo "${line:$num_chars}" >> "$output_file"
done < "$input_file"

echo "Processed file saved as '$output_file'."
