#!/bin/bash

# Files to store perf outputs
put_output_file="put_timings.txt"
get_output_file="get_timings.txt"

# Ensure the output files are empty
> "$put_output_file"
> "$get_output_file"

for i in {1..5}; do
    echo "=== Run $i ===" | tee -a "$put_output_file"
    echo "=== Run $i ===" | tee -a "$get_output_file"
    
    # Run make clean and make
    make clean
    make
    
    # Run perf stat for put workload and append output to put_timings.txt
    sudo perf stat ./database < put_workload.txt 2>&1 | tee -a "$put_output_file"
    echo "" >> "$put_output_file" # Add a newline for readability between runs
    
    # Run perf stat for get workload and append output to get_timings.txt
    sudo perf stat ./database < get_workload.txt 2>&1 | tee -a "$get_output_file"
    echo "" >> "$get_output_file" # Add a newline for readability between runs
done

echo "Experiment completed."
