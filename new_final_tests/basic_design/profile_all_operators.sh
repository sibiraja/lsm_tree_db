#!/bin/bash

# Define the data size strings
DATA_SIZES_ARRAY=("100mb" "500mb" "1gb" "2gb" "4gb")

# Loop over each data size
for data_size in "${DATA_SIZES_ARRAY[@]}"; do
    # Loop of 5 iterations (for 5 trials)
    for trial in {1..5}; do
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
        num_insertions=$((total_bytes / 8))

        # Output the data size, trial number, and calculated insertions
        echo "Running trial $trial with data size: $data_size and $num_insertions insertions"

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

        ./range_profile/profile_range.sh "$data_size"
        if [ $? -ne 0 ]; then
            echo "Error in profile_range.sh for data size: $data_size"
            exit 1
        fi

        ./delete_profile/profile_delete.sh "$data_size"
        if [ $? -ne 0 ]; then
            echo "Error in profile_delete.sh for data size: $data_size"
            exit 1
        fi

        # Execute load script, passing both data size and num insertions
        ./load_profile/profile_load.sh "$data_size" "$num_insertions"
        if [ $? -ne 0 ]; then
            echo "Error in profile_load.sh for data size: $data_size"
            exit 1
        fi
    done
done

echo "ALL BASIC EXPERIMENTS DONE"