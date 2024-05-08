#!/bin/bash

# Note: this script should be run as `./profile.sh`

# Save the current directory (where the script is located)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Array of workload file names
SKEWNESS_LEVELS=("0" "20" "40" "60" "80" "100")

for skewness in "${SKEWNESS_LEVELS[@]}"; do

    cache_latency_database_output="${script_dir}/latency_${skewness}_skew.txt"
    iostat_output="${script_dir}/iostat_${skewness}_skew.txt"
    workload_filename="${skewness}_skew_100mb_put_100K_get.txt"
    expected_entries=12500000

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

        # Run the C++ program for the database, redirecting output
        # Output cachegrind summary along with the time reported by the `time` command
        { time ./database_both "$expected_entries" < "${script_dir}/${workload_filename}"; } >> "$cache_latency_database_output" 2>&1

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
