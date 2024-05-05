#!/bin/bash

# Note: this script should be run as `./profile_put.sh 0.000001mb` to test putting 1 key value pair. 

# Save the current directory (where the script is located)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# This is a string such as 100mb (everything has to be lowercase for put workload file names)
data_size_string=$1
workload_filename="${data_size_string}_put.txt"

time_database_output="${script_dir}/latency_${data_size_string}.txt"
iostat_output="${script_dir}/iostat_${data_size_string}.txt"

# Start iostat to monitor disk IO stats and write to a file
# Make sure to write "NEW RUN" explictly to iostat_output file so we can separate stats from each trial
echo "=====NEW RUN=====" >> "$iostat_output"
iostat 1 >> "$iostat_output" 2>&1 &
IOSTAT_PID=$!

# need this because the `cd`'ing that I do is from the perspective of the master script's pwd. this fix works for now
cd "${script_dir}"

# Echo the present working directory (PWD)
# echo "Current working directory after changing directories: $(pwd)"

# Navigate to the directory of the database executable, note we are one level deeper into a subdir in the skiplist experiments
cd ../../../../

# Clean and rebuild the database executable
make clean
make

# Run the C++ program for the database, redirecting output to the put_profile subdir
{ time ./database < "${script_dir}/${workload_filename}" ; } >> "$time_database_output" 2>&1
PROGRAM_EXIT_CODE=$?

# Kill iostat after the program finishes
kill $IOSTAT_PID

# Allow a little delay to ensure disk writes are flushed
sleep 1

# Check if the program exited successfully
if [ $PROGRAM_EXIT_CODE -ne 0 ]; then
    echo "Program exited with error during ${data_size_string} put workload."
else
    echo "Profiling complete for ${data_size_string} put workload."
fi