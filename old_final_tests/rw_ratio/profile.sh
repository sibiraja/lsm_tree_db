#!/bin/bash

# Save the current directory (where the script is located)
script_dir=$(pwd)

# Define the array of read-write ratios
RW_RATIOS=("1_9" "2_8" "3_7" "4_6" "5_5" "6_4" "7_3" "8_2" "9_1")

# Loop over each read-write ratio
for RW in "${RW_RATIOS[@]}"; do

    # Run make clean in the appropriate directory after each put and get pair
    (cd ../.. && make clean)
    
    # Compile the latest version of the database
    (cd ../.. && make)

    # Define the input file based on current read-write ratio
    INPUT_FILE="${script_dir}/rw_${RW}_put_get.txt"
    # Define output files for cachegrind and iostat
    CACHEGRIND_OUT="${script_dir}/cachegrind_rw_${RW}_output.txt"
    IOSTAT_OUT="${script_dir}/iostat_rw_${RW}_output.txt"

    # Start iostat to monitor disk IO stats and write to a file
    iostat -x 1 > "$IOSTAT_OUT" 2>&1 &
    IOSTAT_PID=$!

    # Navigate to the directory of the database executable
    cd ../..

    # Run the C++ program with cachegrind with input redirection from the specific workload file
    valgrind --tool=cachegrind ./database < "$INPUT_FILE" > "$CACHEGRIND_OUT" 2>&1
    PROGRAM_EXIT_CODE=$?

    # Return to the script directory (necessary for correct file paths in the next loop iteration)
    cd "$script_dir"

    # Kill iostat after the program finishes
    kill $IOSTAT_PID

    # Check if the program exited successfully
    if [ $PROGRAM_EXIT_CODE -ne 0 ]; then
        echo "Program exited with error during RW ${RW} workload."
    else
        echo "Profiling complete for RW ${RW} workload."
    fi

    # Allow a little delay to ensure disk writes are flushed
    sleep 1
done

echo "All profiling sessions completed."