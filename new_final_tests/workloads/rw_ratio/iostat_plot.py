# DONT DO IOSTAT PLOT BC IOSTAT OUTPUT IS WEIRD BC I RAN IT ON DOCKER, SO JUST USE CACHE STATS AND EXCUTION TIME IN PAPER

# import matplotlib.pyplot as plt
# import numpy as np

# def parse_file(filepath):
#     with open(filepath, 'r') as file:
#         content = file.readlines()
    
#     runs = []
#     current_run = []
#     for line in content:
#         if "=====NEW RUN=====" in line:
#             if current_run:
#                 runs.append(np.mean(current_run))
#                 current_run = []
#         elif "MB/s" in line or "disk0" in line:
#             continue
#         else:
#             values = line.split()
#             if len(values) >= 3 and values[2].replace('.', '', 1).isdigit():
#                 current_run.append(float(values[2]))
#     if current_run:
#         runs.append(np.mean(current_run))

#     return runs

# # Define the file paths and ratios
# files = [
#     "iostat_1_9_ratio.txt",
#     "iostat_2_8_ratio.txt",
#     "iostat_3_7_ratio.txt",
#     "iostat_4_6_ratio.txt",
#     "iostat_5_5_ratio.txt",
#     "iostat_6_4_ratio.txt",
#     "iostat_7_3_ratio.txt",
#     "iostat_8_2_ratio.txt",
#     "iostat_9_1_ratio.txt",
# ]
# ratios = ["1:9", "2:8", "3:7", "4:6", "5:5", "6:4", "7:3", "8:2", "9:1"]

# plotting_averages = []
# std_devs = []

# for file in files:
#     averages_in_file = parse_file(file)
#     plotting_averages.append(np.mean(averages_in_file))
#     std_devs.append(np.std(averages_in_file))  # Calculate standard deviation for error bars

# # Plotting with error bars
# plt.figure(figsize=(6, 5))
# plt.errorbar(ratios, plotting_averages, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=2, capsize=5, label='Disk Throughput')
# plt.xlabel('Write to Read Ratio (Put:Get)')
# plt.ylabel('Average Disk Throughput (MB/s)')
# plt.title('Throughput for Read-Intensive, Mixed, and Write-Intensive Workloads')
# plt.xticks(ratios)  # Set x-axis labels to cache-to-latency ratios
# plt.legend()
# plt.grid(True)
# plt.show()


# # import matplotlib.pyplot as plt
# # import numpy as np

# # def parse_file(filepath):
# #     with open(filepath, 'r') as file:
# #         content = file.readlines()
    
# #     runs = []
# #     current_run = []
# #     for line in content:
# #         if "=====NEW RUN=====" in line:
# #             if current_run:
# #                 runs.append(np.mean(current_run))
# #                 current_run = []
# #         elif "MB/s" in line or "disk0" in line:
# #             continue
# #         else:
# #             values = line.split()
# #             if len(values) >= 3 and values[2].replace('.', '', 1).isdigit():
# #                 current_run.append(float(values[2]))
# #     if current_run:
# #         runs.append(np.mean(current_run))

# #     return runs


# # # Define the file paths (assuming they are in the current directory for simplicity)
# # files = [
# #     "iostat_1_9_ratio.txt",
# #     "iostat_2_8_ratio.txt",
# #     "iostat_3_7_ratio.txt",
# #     "iostat_4_6_ratio.txt",
# #     "iostat_5_5_ratio.txt",
# #     "iostat_6_4_ratio.txt",
# #     "iostat_7_3_ratio.txt",
# #     "iostat_8_2_ratio.txt",
# #     "iostat_9_1_ratio.txt",
# # ]

# # plotting_averages = []
# # std_devs = []

# # for file in files:
# #     averages_in_file = parse_file(file)
# #     plotting_averages.append(np.mean(averages_in_file))
# #     std_devs.append(np.std(averages_in_file))  # Calculate standard deviation for error bars

# # # NEW PLOT SCRIPT HERE PLEASE


# # # # Plotting with error bars
# # # plt.figure(figsize=(6, 5))
# # # # plt.errorbar(file_sizes, plotting_averages, yerr=std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=5)

# # # # Plotting your data
# # # plt.errorbar(file_sizes, basic_variant_mean_times , yerr=basic_variant_std_devs, fmt='-o', color='blue', ecolor='red', elinewidth=3, capsize=15, label='Constant FENCE_PTR_EVERY_K')

# # # # Plotting additional data
# # # plt.errorbar(file_sizes, plotting_averages, yerr=std_devs, fmt='-o', color='green', ecolor='orange', elinewidth=3, capsize=15, label='Varying fptrs_every_k')

# # # plt.xscale('log')  # Set x-axis scale to logarithmic for better visualization
# # # plt.xlabel('Data Size')
# # # plt.ylabel('Average Disk MB/s')
# # # plt.title('Throughput - Constant vs Varying Fence Ptr Frequency')
# # # plt.xticks(file_sizes, [format_data_size(size) for size in file_sizes])  # Set custom x-axis labels
# # # plt.legend()  # Show legend
# # # plt.grid(True)
# # # plt.show()