#!/bin/bash

# Define the data size strings
DATA_SIZES_ARRAY=("100mb" "500mb" "1gb" "2gb" "4gb")

# Loop over each data size
for data_size in "${DATA_SIZES_ARRAY[@]}"; do
    # Loop of 5 iterations (for 5 trials)
    for trial in {1..5}; do

        # Output the data size, trial number
        echo "Running skiplist buffer trial $trial with data size: $data_size"

        # Execute profile scripts and check for errors
        ./put_profile/profile_put.sh "$data_size"
        if [ $? -ne 0 ]; then
            echo "Error in profile_put.sh for data size: $data_size"
            exit 1
        fi

        ./get_profile/profile_get.sh "$data_size"
        if [ $? -ne 0 ]; then
            echo "Error in profile_get.sh for data size: $data_size"
            exit 1
        fi

        ./delete_profile/profile_delete.sh "$data_size"
        if [ $? -ne 0 ]; then
            echo "Error in profile_delete.sh for data size: $data_size"
            exit 1
        fi

    done
done

echo "ALL SKIPLIST BUFFER EXPERIMENTS DONE"