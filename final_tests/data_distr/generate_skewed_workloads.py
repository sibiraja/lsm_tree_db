import random

def load_data(filename):
    with open(filename, 'r') as file:
        data = [line.strip() for line in file]
    return data

def generate_skewed_data(data, skew_percent):
    num_entries = len(data)
    num_skewed = int(num_entries * (skew_percent / 100))
    skewed_indices = random.sample(range(num_entries), num_skewed)
    
    # Generate new values for the selected keys and insert them into random positions
    new_entries = []
    for index in skewed_indices:
        key = data[index].split()[1]
        new_value = random.randint(-2**31, 2**31 - 1)
        new_entries.append(f'p {key} {new_value}')
    
    # Insert new entries at random positions in the original data
    for entry in new_entries:
        insert_position = random.randint(0, len(data))
        data.insert(insert_position, entry)

    return data

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
