#!/bin/bash

# Server IP address
SERVER_IP="128.105.145.157"

# Temporary file to store latencies
LATENCY_FILE="latencies.txt"
> "$LATENCY_FILE"  # Clear any existing content

# Function to calculate percentiles
calculate_percentiles() {
    # Sort the latencies and calculate the required percentiles
    sorted_latencies=$(sort -n "$LATENCY_FILE")
    
    # Count the number of latencies
    num_latencies=$(wc -l < "$LATENCY_FILE")
    
    # Function to calculate percentile
    percentile() {
        local p=$1
        local index=$(echo "$num_latencies * $p / 100" | bc)
        echo "$sorted_latencies" | sed -n "${index}p"
    }

    echo "50th percentile latency: $(percentile 50)"
    echo "90th percentile latency: $(percentile 90)"
    echo "95th percentile latency: $(percentile 95)"
    echo "99th percentile latency: $(percentile 99)"
}

run() {
    local id serverpath clientpath start_time end_time latency
    # Generate the file paths
    id=$(($i))
    serverpath="/data/testdata/${id}.dat"
    clientpath="./data/${id}.dat"
    
    # Record the start time
    start_time=$(date +%s%N)

    # Run the client command in the background
    ./client "$SERVER_IP" -c "READ $serverpath $clientpath"

    # Record the end time and calculate latency
    end_time=$(date +%s%N)
    
    # Calculate latency in milliseconds
    latency=$((($end_time - $start_time) / 1000000))
    
    # Save the latency to the file
    echo "$latency" >> "$LATENCY_FILE"
}

for i in $(seq 1 200); do
    run &
done

wait
# Calculate and print percentiles
calculate_percentiles
