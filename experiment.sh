#!/bin/bash

# Set the path to the workload file
WORKLOAD_PATH="final_tests/data_size/100mb_get.txt"

# Start iostat to monitor IO statistics and log them to a file. Run in the background.
iostat -dx 2 > final_tests/data_size/100mb_get_iostat_log.txt &

# Get the PID of the iostat process so we can kill it later
IOSTAT_PID=$!

# Run the database under valgrind with cachegrind tool and redirect its standard output
valgrind --tool=cachegrind ./database < $WORKLOAD_PATH > final_tests/data_size/100mb_get_cachegrind_output.txt 2>&1

# Stop the iostat process
kill $IOSTAT_PID
wait $IOSTAT_PID 2>/dev/null

# Print a message that the profiling and IO monitoring have completed
echo "Profiling and IO monitoring have completed."
echo "Cache statistics output is available in cachegrind_live_output.txt."
echo "IO statistics are logged in iostat_log.txt."

# Optionally, display the iostat results
# cat iostat_log.txt
