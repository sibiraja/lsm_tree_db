import matplotlib.pyplot as plt
import numpy as np

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

# List of files and corresponding cache-to-latency ratios
files = [
    "cache_AND_latency_1_9_ratio.txt",
    "cache_AND_latency_2_8_ratio.txt",
    "cache_AND_latency_3_7_ratio.txt",
    "cache_AND_latency_4_6_ratio.txt",
    "cache_AND_latency_5_5_ratio.txt",
    "cache_AND_latency_6_4_ratio.txt",
    "cache_AND_latency_7_3_ratio.txt",
    "cache_AND_latency_8_2_ratio.txt",
    "cache_AND_latency_9_1_ratio.txt"
]
ratios = ["1:9", "2:8", "3:7", "4:6", "5:5", "6:4", "7:3", "8:2", "9:1"]

# Extracting times and calculating statistics
mean_times = []
std_devs = []

for file in files:
    times = extract_times(file)
    mean_time = np.mean(times)
    std_dev = np.std(times)
    mean_times.append(mean_time / 60)  # Convert mean time to minutes
    std_devs.append(std_dev / 60)  # Convert standard deviation to minutes

# Plotting
plt.figure(figsize=(6, 5))
plt.errorbar(ratios, mean_times, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=2, capsize=5, label='Execution Time')
plt.xlabel('Write to Read Ratio (Put:Get) across 1 Million Operations')
plt.ylabel('Average Execution Time (minutes)')
plt.title('Latency for Read-Intensive, Mixed, and Write-Intensive Workloads')
plt.xticks(ratios)  # Set x-axis labels to cache-to-latency ratios
# plt.legend()
plt.grid(True)
plt.show()
