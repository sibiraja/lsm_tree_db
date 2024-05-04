#!/bin/bash

# Save the current directory (where the script is located)
script_dir=$(pwd)

# Define the array of skewness levels
SKEWNESS_LEVELS=(0 20 40 60 80 100)

# Loop over each skewness level
for SKEW in "${SKEWNESS_LEVELS[@]}"; do

    # Run make clean in the appropriate directory after each put and get pair
    (cd ../.. && make clean)
    
    # Compile the latest version of the database
    (cd ../.. && make)

    # Loop for each type of workload: put and get
    for TYPE in "put" "get"; do
        # Define the input file based on current skewness level and workload type
        INPUT_FILE="${script_dir}/skewed_${SKEW}_${TYPE}.txt"
        # Define output files for cachegrind and iostat
        CACHEGRIND_OUT="${script_dir}/cachegrind_${SKEW}_${TYPE}_output.txt"
        IOSTAT_OUT="${script_dir}/iostat_${SKEW}_${TYPE}_output.txt"

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
            echo "Program exited with error during skew ${SKEW}% ${TYPE} workload."
        else
            echo "Profiling complete for skew ${SKEW}% ${TYPE} workload."
        fi

        # Allow a little delay to ensure disk writes are flushed
        sleep 1
    done
done

echo "All profiling sessions completed."
