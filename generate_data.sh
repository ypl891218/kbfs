#!/bin/bash

# Directory to store the generated files
OUTPUT_DIR="/data/testdata/"
mkdir -p "$OUTPUT_DIR"  # Create the directory if it doesn't exist

# Loop to generate 1000 random files
for i in $(seq 100 200); do
    # Define the output file path
    output_file="$OUTPUT_DIR/$i.dat"
    
    # Generate the random file using dd
    dd if=/dev/urandom of="$output_file" bs=1M count=10 status=progress
    
    # Optional: Add a small delay to avoid overwhelming the system (adjust as needed)
    sleep 0.1
done

echo "100 random files generated in the $OUTPUT_DIR directory."
