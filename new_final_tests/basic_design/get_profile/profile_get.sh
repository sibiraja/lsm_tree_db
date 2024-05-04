#!/bin/bash

# Note: this script should be run as `./profile_get.sh 1gb` to test executing 100K gets on a database with 1gb of data

# Save the current directory (where the script is located)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# This is a string such as 100mb (everything has to be lowercase for put workload file names)
data_size_string=$1
workload_filename="100K_get.txt"

time_database_output="${script_dir}/latency_${data_size_string}.txt"
iostat_output="${script_dir}/iostat_${data_size_string}.txt"

# Start iostat to monitor disk IO stats and write to a file
# Make sure to write "NEW RUN" explictly to iostat_output file so we can separate stats from each trial
echo "=====NEW RUN=====" >> "$iostat_output"
iostat 1 >> "$iostat_output" 2>&1 &
IOSTAT_PID=$!

# Navigate to the directory of the database executable (currently in get_profile subdir)
cd ../../../

# DON'T CLEAN AND REBUILD DATABASE EXECUTABLE BC WE NEED THE DATA WE INSERTED FROM A PREVIOUS PUT/LOAD WORKLOAD
# Run the C++ program for the database, redirecting output to the put_profile subdir
{ time ./database < "${script_dir}/${workload_filename}" ; } >> "$time_database_output" 2>&1
PROGRAM_EXIT_CODE=$?

# Kill iostat after the program finishes
kill $IOSTAT_PID

# Allow a little delay to ensure disk writes are flushed
sleep 1

# Check if the program exited successfully
if [ $PROGRAM_EXIT_CODE -ne 0 ]; then
    echo "Program exited with error during ${data_size_string} get workload."
else
    echo "Profiling complete for ${data_size_string} get workload."
fi