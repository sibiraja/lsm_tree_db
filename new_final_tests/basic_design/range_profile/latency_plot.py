import matplotlib.pyplot as plt
import numpy as np
import os

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
file_labels = []
mean_times = []
std_devs = []

for file in files:
    times = extract_times(file)
    mean_time = np.mean(times)
    std_dev = np.std(times)
    mean_times.append(mean_time / 60)  # Convert mean time to minutes
    std_devs.append(std_dev / 60)  # Convert standard deviation to minutes
    file_labels.append(file.split('_')[1].split('.')[0])  # Extracting the file size from the filename

# Plotting
plt.figure(figsize=(10, 5))
plt.errorbar(file_labels, mean_times, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=15)
plt.xlabel('Data Size')
plt.ylabel('Average Time (minutes)')
plt.title('Latency of Range Operator (Basic Design)')
plt.grid(True)
plt.show()
