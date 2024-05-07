#!/bin/bash

# Save the current directory (where the script is located)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define the data size strings
DATA_SIZES_ARRAY=("100mb" "500mb" "1gb" "2gb" "4gb")

# Loop over each data size
for data_size in "${DATA_SIZES_ARRAY[@]}"; do
    # Extract the numerical digits from the current data size
    num_prefix=$(echo "$data_size" | grep -oE '^[0-9]+')

    # Determine the unit multiplier
    if [[ "$data_size" =~ mb$ ]]; then
        multiplier=1000000
    elif [[ "$data_size" =~ gb$ ]]; then
        multiplier=1000000000
    else
        echo "Unknown unit in data size: $data_size"
        exit 1
    fi

    # Calculate the total bytes and the number of insertions
    total_bytes=$((num_prefix * multiplier))
    num_entries=$((total_bytes / 8))

    # Output the data size and calculated insertions
    echo "Populating database with data size: $data_size and $num_entries entries"

    put_workload_name="${data_size}_put.txt"

    # Navigate to the directory of the database executable (currently in get_profile subdir)
    cd "${script_dir}/../../.."  # adjust path as necessary

    # Clean and build the project
    make clean
    make

    # Run the put workload
    ./database "$num_entries" < "${script_dir}/${put_workload_name}" >> "${script_dir}/log_put_${data_size}.txt" 2>&1

    # Loop of 5 iterations (for 5 trials of the range workload)
    for trial in {1..5}; do
        echo "Running range workload trial $trial with data size: $data_size"

        range_workload_filename="10_range.txt"

        time_database_output="${script_dir}/latency_${data_size}.txt"
        iostat_output="${script_dir}/iostat_${data_size}_${trial}.txt"

        # Start iostat to monitor disk IO stats and write to a file
        echo "=====NEW RUN=====" >> "$iostat_output"
        iostat 1 >> "$iostat_output" 2>&1 &
        IOSTAT_PID=$!

        # Time the database operation
        { time ./database < "${script_dir}/${range_workload_filename}"; } >> "$time_database_output" 2>&1

        # Stop the iostat process
        kill $IOSTAT_PID
    done
done

echo "ALL BASIC EXPERIMENTS DONE"