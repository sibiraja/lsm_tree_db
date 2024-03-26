def generate_get_commands(input_file, output_file):
    with open(input_file, 'r') as infile, open(output_file, 'w') as outfile:
        keys = [line.split()[1] for line in infile if line.startswith('p ')]
        for key in keys:
            outfile.write(f"g {key}\n")

# To use this script, replace 'input_file' with the path to your file containing 'put' commands,
# and 'output_file' with the path where you want the 'get' commands file to be saved.
input_file = "put_workload.txt"
output_file = "get_workload.txt"

generate_get_commands(input_file, output_file)