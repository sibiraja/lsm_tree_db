import matplotlib.pyplot as plt
import numpy as np

def parse_file(filepath):
    with open(filepath, 'r') as file:
        content = file.readlines()
    
    runs = []
    current_run = []
    for line in content:
        if "=====NEW RUN=====" in line:
            if current_run:
                runs.append(np.mean(current_run))
                current_run = []
        elif "MB/s" in line or "disk0" in line:
            continue
        else:
            values = line.split()
            if len(values) >= 3 and values[2].replace('.', '', 1).isdigit():
                current_run.append(float(values[2]))
    if current_run:
        runs.append(np.mean(current_run))

    return runs

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

# Define the file paths (assuming they are in the current directory for simplicity)
files = [
    "iostat_100mb.txt",
    "iostat_500mb.txt",
    "iostat_1gb.txt",
    "iostat_2gb.txt",
    "iostat_4gb.txt"
]
labels = ['100mb', '500mb', '1gb', '2gb', '4gb']
plotting_averages = []
std_devs = []

for file in files:
    averages_in_file = parse_file(file)
    plotting_averages.append(np.mean(averages_in_file))
    std_devs.append(np.std(averages_in_file))  # Calculate standard deviation for error bars

# Convert labels to bytes
file_sizes = [convert_to_bytes(label) for label in labels]

basic_variant_mean_times = [25.450000000000003, 1.7855, 79.15142857142857, 88.83553846153845, 141.18625641025642]
basic_variant_std_devs = [0.10807404868884997, 1.0472325434209924, 4.611113814713849, 11.492445776124418, 5.300204783605968]


# Plotting with error bars
plt.figure(figsize=(6, 5))
# plt.errorbar(file_sizes, plotting_averages, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=5)

# Plotting your data
plt.errorbar(file_sizes, basic_variant_mean_times , yerr=basic_variant_std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=15, label='FeatherDB with unsorted log')

# Plotting additional data
plt.errorbar(file_sizes, plotting_averages, yerr=std_devs, fmt='-o', color='green', ecolor='orange', elinewidth=3, capsize=15, label='FeatherDB with skiplist')

plt.xscale('log')  # Set x-axis scale to logarithmic for better visualization
plt.xlabel('Data Size')
plt.ylabel('Average Disk MB/s')
plt.title('Throughput of Delete Operator  - Skiplist vs Unsorted Log Buffer')
plt.xticks(file_sizes, [format_data_size(size) for size in file_sizes])  # Set custom x-axis labels
plt.legend()  # Show legend
plt.grid(True)
plt.show()