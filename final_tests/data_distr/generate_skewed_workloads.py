import random

def load_data(filename):
    with open(filename, 'r') as file:
        data = [line.strip() for line in file]
    return data

def generate_skewed_data(data, skew_percent):
    num_entries = len(data)
    num_skewed = int(num_entries * (skew_percent / 100))
    skewed_indices = set(random.sample(range(num_entries), num_skewed))

    # Determine how many duplicates to add and how many originals to keep
    num_duplicates_needed = num_skewed // 2
    num_originals_needed = num_entries - num_duplicates_needed

    # Create a new list to store the modified data
    new_data = []

    # Add duplicates for half of the skewed entries
    skewed_list = list(skewed_indices)  # Convert back to list to access by index
    for i in range(num_duplicates_needed):
        index = skewed_list[i]
        key = data[index].split()[1]
        # Original entry is added
        new_data.append(data[index])
        # Duplicate entry with a new random value
        new_value = random.randint(-2**31, 2**31 - 1)
        new_data.append(f'p {key} {new_value}')

    # Add the remaining non-duplicated entries to fill up to the required number
    remaining_indices = [i for i in range(num_entries) if i not in skewed_indices]
    random.shuffle(remaining_indices)  # Shuffle to randomize which entries are added
    new_data.extend(data[i] for i in remaining_indices[:num_originals_needed])

    # Shuffle new_data to mix duplicates and non-duplicates randomly
    random.shuffle(new_data)

    return new_data

def write_data(data, filename):
    with open(filename, 'w') as file:
        for line in data:
            file.write(line + '\n')
        file.write('e\n')

def main():
    original_filename = '0skew_put.txt'  # Change this to your original file name
    original_data = load_data(original_filename)
    
    skew_levels = [0, 20, 40, 60, 80, 100]
    for level in skew_levels:
        data_copy = original_data.copy()
        skewed_data = generate_skewed_data(data_copy, level)
        skewed_filename = f'skewed_{level}_put.txt'
        write_data(skewed_data, skewed_filename)
        print(f"Generated {skewed_filename} with {level}% skew.")

if __name__ == "__main__":
    main()