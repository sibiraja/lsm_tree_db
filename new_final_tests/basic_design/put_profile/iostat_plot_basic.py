import matplotlib.pyplot as plt
import numpy as np
import os

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

# Plotting with error bars
plt.figure(figsize=(10, 5))
plt.errorbar(labels, plotting_averages, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=5)
plt.xlabel('Data Size')
plt.ylabel('Average MB/s')
plt.title('Average Disk MB/s Across 5 Trials of Put Operator (Basic Design)')
plt.grid(True)
plt.show()
