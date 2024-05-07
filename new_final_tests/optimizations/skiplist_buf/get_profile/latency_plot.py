import matplotlib.pyplot as plt
import numpy as np

# Function to convert data sizes to bytes
def convert_to_bytes(size):
    if size.endswith('mb'):
        return float(size[:-2]) * 1024 * 1024
    elif size.endswith('gb'):
        return float(size[:-2]) * 1024 * 1024 * 1024
    else:
        return float(size)

# Function to format data size with appropriate prefix
def format_data_size(size_bytes):
    if size_bytes >= 1024 * 1024 * 1024:
        return f"{size_bytes / (1024 * 1024 * 1024):.1f} GB"
    elif size_bytes >= 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.1f} MB"
    else:
        return f"{size_bytes / 1024:.1f} KB"

# Function to convert time format "10m33.233s" to total seconds
def time_to_seconds(t):
    minutes, seconds = t[:-1].split('m')
    return int(minutes) * 60 + float(seconds)

# Function to read and extract real times from each file
def extract_times(filename):
    times = []
    with open(filename, 'r') as file:
        for line in file:
            if line.startswith('real'):
                time_str = line.split('\t')[1].strip()
                times.append(time_to_seconds(time_str))
    return times

# List of files
files = [
    "latency_100mb.txt",
    "latency_500mb.txt",
    "latency_1gb.txt",
    "latency_2gb.txt",
    "latency_4gb.txt"
]

# Extracting times and calculating statistics
file_sizes = []
mean_times = []
std_devs = []

for file in files:
    times = extract_times(file)
    mean_time = np.mean(times)
    std_dev = np.std(times)
    mean_times.append(mean_time / 60)  # Convert mean time to minutes
    std_devs.append(std_dev / 60)  # Convert standard deviation to minutes
    file_sizes.append(convert_to_bytes(file.split('_')[1].split('.')[0]))  # Extracting the file size from the filename and converting to bytes

# Additional data
basic_variant_mean_times = [0.02953333333333333, 0.06767999999999999, 0.11216999999999998, 0.19907666666666665, 0.3782666666666667]
basic_variant_std_devs = [0.0004549725266430933, 0.0006417337627258059, 0.0011879674518549157, 0.0033939914227613657, 0.004556424036456658]

# print(mean_times)
# print(std_devs)

# Plotting
plt.figure(figsize=(6, 5))

# Plotting your data
plt.errorbar(file_sizes, basic_variant_mean_times , yerr=basic_variant_std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=15, label='FeatherDB with unsorted log')

# Plotting additional data
plt.errorbar(file_sizes, mean_times, yerr=std_devs, fmt='-o', color='green', ecolor='orange', elinewidth=3, capsize=15, label='FeatherDB with skiplist')

plt.xscale('log')  # Set x-axis scale to logarithmic for better visualization
plt.xlabel('Data Size')
plt.ylabel('Average Time (minutes)')
plt.title('Latency of Get Operator - Skiplist vs Unsorted Log Buffer')
plt.xticks(file_sizes, [format_data_size(size) for size in file_sizes])  # Set custom x-axis labels
plt.legend()  # Show legend
plt.grid(True)
plt.show()
