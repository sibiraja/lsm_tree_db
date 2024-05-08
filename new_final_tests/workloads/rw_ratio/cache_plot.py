import matplotlib.pyplot as plt
import numpy as np

# Function to parse cache misses from each file
def parse_cache_misses(filename):
    d1_misses = []
    lld_misses = []
    with open(filename, 'r') as file:
        for line in file:
            # if line.startswith('D1  misses:'):
            if 'D1  misses:' in line:
                misses = int(line.split()[3].replace(',', ''))
                d1_misses.append(misses)
            # elif line.startswith('LLd misses:'):
            elif 'LLd misses:' in line:
                misses = int(line.split()[3].replace(',', ''))
                lld_misses.append(misses)

    total_misses = [d + l for d, l in zip(d1_misses, lld_misses)]
    return total_misses

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

# Extracting cache misses and calculating statistics
mean_misses = []
std_devs = []

for file in files:
    total_misses = parse_cache_misses(file)
    print(total_misses)
    mean_miss = np.mean(total_misses)
    std_dev = np.std(total_misses)
    mean_misses.append(mean_miss)  # Keeping as count of misses
    std_devs.append(std_dev)       # Keeping as count of misses

print(mean_misses)
print(std_devs)

# Plotting
plt.figure(figsize=(6, 5))
plt.errorbar(ratios, mean_misses, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=2, capsize=10)
plt.xlabel('Write to Read Ratio (Put:Get) across 1 Million Operations')
plt.ylabel('Average Total Cache Misses (D1 + LLd)')
plt.title('Cache Misses for Read-Intensive, Mixed, and Write-Intensive Workloads')
plt.xticks(ratios)  # Set x-axis labels to cache-to-latency ratios
plt.grid(True)
plt.show()
