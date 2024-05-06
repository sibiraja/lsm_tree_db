#!/bin/bash

# Array of data sizes
DATA_SIZES_ARRAY=("10mb" "100mb" "500mb" "1gb" "2gb")

# Inner loop over each data size
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
        num_entries=$((total_bytes / 8))

        # Output the data size, trial number, and calculated insertions
        echo "Running trial $trial with data size: $data_size and $num_entries insertions"

        # Execute original variant profiling script, passing data_size and num_entries
        ./skiplist_original/profile.sh "$data_size" "$num_entries"
        if [ $? -ne 0 ]; then
            echo "Error in original variant's profile.sh for data size: $data_size"
            exit 1
        fi

        # Execute varying fence ptrs variant profiling script, passing data_size and num_entries
        ./skiplist_fenceptrs/profile.sh "$data_size" "$num_entries"
        if [ $? -ne 0 ]; then
            echo "Error in varying fence ptrs variant's profile.sh for data size: $data_size"
            exit 1
        fi

        # Execute varying size ratio variant profiling script, passing data_size and num_entries
        ./skiplist_sizeratio/profile.sh "$data_size" "$num_entries"
        if [ $? -ne 0 ]; then
            echo "Error in varying size ratio variant's profile.sh for data size: $data_size"
            exit 1
        fi

        # Execute varying fence ptr AND varying size ratio variant profiling script, passing data_size and num_entries
        ./skiplist_both/profile.sh "$data_size" "$num_entries"
        if [ $? -ne 0 ]; then
            echo "Error in varying fence ptr AND varying size ratio variant's profile.sh for data size: $data_size"
            exit 1
        fi
    done
done
