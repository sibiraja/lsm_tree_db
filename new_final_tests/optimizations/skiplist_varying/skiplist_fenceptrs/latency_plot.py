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
        return f"{int(size_bytes / (1024 * 1024 * 1024))} GB"
    elif size_bytes >= 1024 * 1024:
        return f"{int(size_bytes / (1024 * 1024))} MB"
    else:
        return f"{int(size_bytes / 1024)} KB"

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
    "latency_10mb.txt",
    "latency_100mb.txt",
    "latency_500mb.txt",
    "latency_1gb.txt",
    "latency_2gb.txt",
    # "latency_4gb.txt"
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
basic_variant_mean_times = [0.13867666666666667, 0.8372233333333334, 4.07025, 8.286406666666666, 16.789853333333333]
basic_variant_std_devs = [0.002031135424119012, 0.0018407003498065002, 0.025994852054461427, 0.13851585492883675, 0.0639156348460826]

# Plotting
plt.figure(figsize=(6, 5))

# Plotting your data
plt.errorbar(file_sizes, basic_variant_mean_times , yerr=basic_variant_std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=15, label='Constant FENCE_PTR_EVERY_K')

# Plotting additional data
plt.errorbar(file_sizes, mean_times, yerr=std_devs, fmt='-o', color='green', ecolor='orange', elinewidth=3, capsize=15, label='Varying fptrs_every_k')

plt.xscale('log')  # Set x-axis scale to logarithmic for better visualization
plt.xlabel('Data Size')
plt.ylabel('Average Time (minutes)')
plt.title('Latency - Constant vs Varying Fence Ptr Frequency')
plt.xticks(file_sizes, [format_data_size(size) for size in file_sizes])  # Set custom x-axis labels
plt.legend()  # Show legend
plt.grid(True)
plt.show()
