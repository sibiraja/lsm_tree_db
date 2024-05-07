#!/bin/bash

# Note: this script should be run as `./profile.sh`

# Save the current directory (where the script is located)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Array of workload file names
WORKLOAD_FILENAMES=("900K_put_100K_get.txt" "800K_put_200K_get.txt" "700K_put_300K_get.txt" "600K_put_400K_get.txt" "500K_put_500K_get.txt" "400K_put_600K_get.txt" "300K_put_700K_get.txt" "200K_put_800K_get.txt" "100K_put_900K_get.txt")

# Function to extract the prefix and multiply by 1000 to determine expected entries
get_expected_entries() {
    prefix=$(echo "$1" | grep -oP '^\d+(?=K_)')
    echo $((prefix * 1000))
}

# Function to generate ratio suffix for filenames
get_ratio_suffix() {
    put=$(echo "$1" | grep -oP '^\d+(?=K_put)')
    get=$(echo "$1" | grep -oP '(?<=_)\d+(?=K_get)')
    echo "$((put / 100))_$((get / 100))_ratio"
}

for workload_filename in "${WORKLOAD_FILENAMES[@]}"; do

    ratio_suffix=$(get_ratio_suffix "$workload_filename")
    cache_latency_database_output="${script_dir}/cache_AND_latency_${ratio_suffix}.txt"
    iostat_output="${script_dir}/iostat_${ratio_suffix}.txt"

    # Loop of 5 iterations (for 5 trials)
    for trial in {1..5}; do

        echo "Running trial $trial on $workload_filename"

        # Start iostat to monitor disk IO stats and write to a file
        echo "=====NEW RUN=====" >> "$iostat_output"
        iostat 1 >> "$iostat_output" 2>&1 &
        IOSTAT_PID=$!

        # Need this because the `cd`'ing is from the perspective of the master script's pwd
        cd "${script_dir}"

        # Navigate to the directory of the database executables
        cd ../../../

        # Clean and rebuild the database executable
        make clean
        make

        # Calculate the expected entries from the prefix in the workload filename
        expected_entries=$(get_expected_entries "$workload_filename")

        # Run the C++ program for the database, redirecting output
        # Output cachegrind summary along with the time reported by the `time` command
        { time valgrind --tool=cachegrind ./database_both "$expected_entries" < "${script_dir}/${workload_filename}"; } >> "$cache_latency_database_output" 2>&1

        PROGRAM_EXIT_CODE=$?

        # Kill iostat after the program finishes
        kill $IOSTAT_PID

        # Allow a little delay to ensure disk writes are flushed
        sleep 1

        # Check if the program exited successfully
        if [ $PROGRAM_EXIT_CODE -ne 0 ]; then
            echo "Program exited with error during the ${workload_filename} workload."
        else
            echo "Profiling complete for ${workload_filename} workload."
        fi
    done
done
