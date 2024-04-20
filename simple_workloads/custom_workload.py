# Open the file in write mode
with open('new_commands.txt', 'w') as file:
    # Loop from 1 to 1000
    for x in range(1, 100):
        # Write the put command for (x, -x) directly to the file
        file.write(f'p {x} {-x}\n')
