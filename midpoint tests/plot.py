import os
import re
import numpy as np
import matplotlib.pyplot as plt

# Function to extract and compute the metrics
def extract_and_compute_metrics(files):
    data = {}  # Dictionary to hold the extracted data
    for file in files:
        with open(file, 'r') as f:
            content = f.read()
        # Extract the number from the filename
        num = re.search(r'\d+', file).group()
        # Extract the times
        times = re.findall(r'\s+(\d+\.\d+) seconds time elapsed', content)
        # Convert times to float
        times = [float(time) for time in times]
        # Store the data
        data[int(num)] = {
            'avg': np.mean(times),
            'std': np.std(times),
        }
    return data

# Function to plot the data
def plot_data(data, title):
    # Sorting the keys to ensure the x-axis is in ascending order
    keys = sorted(data.keys())
    avgs = [data[key]['avg'] for key in keys]
    stds = [data[key]['std'] for key in keys]

    # Mapping our x-axis to 2, 3, 4, 5 instead of using the keys directly
    x_axis_labels = keys  # The labels are the sorted keys from the data
    x_positions = range(2, 2 + len(keys))  # Positions are 2, 3, 4, 5

    plt.errorbar(x_positions, avgs, yerr=stds, fmt='-o', capsize=5)
    plt.title(title)
    plt.xticks(x_positions, x_axis_labels)  # Set the x-ticks and labels explicitly
    plt.xlabel('Size of initial level in number of entries (= 2 to 5 pages)')
    plt.ylabel('Time Elapsed (seconds)')
    plt.grid(True)
    plt.show()

# Get the list of files
put_files = [f for f in os.listdir('.') if f.startswith('put_timings')]
get_files = [f for f in os.listdir('.') if f.startswith('get_timings')]

# Extract and compute the metrics
put_data = extract_and_compute_metrics(put_files)
get_data = extract_and_compute_metrics(get_files)

# Plot the data
plot_data(put_data, 'PUT Operations Performance')
plot_data(get_data, 'GET Operations Performance')
