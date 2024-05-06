#!/bin/bash

# Note: this script should be run as `./profile_put.sh 0.000001mb` to test putting 1 key value pair. 

# Save the current directory (where the script is located)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"


# Array of workload file names
WORKLOAD_FILENAMES=("900K_put_100K_get" "800K_put_200K_get" "700K_put_300K_get" "600K_put_400K_get" "500K_put_500K_get" "400K_put_600K_get" "300K_put_700K_get" "200K_put_800K_get" "100K_put_900K_get")

for workload_filename in "${WORKLOAD_FILENAMES[@]}"; do

    cache_latency_database_output="${script_dir}/cache_AND_latency_${workload_filename}.txt"
    iostat_output="${script_dir}/iostat_${workload_filename}.txt"

    # Loop of 5 iterations (for 5 trials)
    for trial in {1..5}; do

        echo "Running trial $trial on $workload_filename"

        # Start iostat to monitor disk IO stats and write to a file
        # Make sure to write "NEW RUN" explictly to iostat_output file so we can separate stats from each trial
        echo "=====NEW RUN=====" >> "$iostat_output"
        iostat 1 >> "$iostat_output" 2>&1 &
        IOSTAT_PID=$!

        # need this because the `cd`'ing that I do is from the perspective of the master script's pwd. this fix works for now
        cd "${script_dir}"

        # Navigate to the directory of the database executables
        cd ../../../

        # Clean and rebuild the database executable
        make clean
        make

        # Run the C++ program for the database, redirecting output to the put_profile subdir
        # MAKE SURE TO RUN WITH CACHEGRIND AS WELL!!!!!!!!!
        
        # MAKE SURE TO OUTPUT CACHEGRIND SUMMARY TOO
        # ALSO MAKE SURE TO RUN THE OPTIMIZED VARIANT THAT HAS THE BEST PERFORMANCE ACCORDING TO SECTION 3 EXPERIMENT RESULTS

        expected_entries= [TODO: JUST EXTRACT THE PREFIX OF THE WORKLOAD FILE, BEFORE THE PUT_ AND MULTIPLY BY 1000 FOR THE K FACTOR]

        # { time valgrind --tool=cachegrind ./database_both "$expected_entries" < "${script_dir}/${workload_filename}" ; } >> "$cache_latency_database_output" 2>&1


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