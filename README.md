# lsm_tree_db
Log-structured merge-tree

##
- Docker for debugging with GDB: `docker build -t lsm_tree`, and `docker run -it --rm -v /Users/sibiraja/Desktop/lsm_tree_db:/usr/src/lsm_tree lsm_tree`
    - Note that Docker containers can mask undefined behavior that I get when running locally on my MacOS, so make sure to run locally first and debug with GDB instead of
    debugging anything with Docker

## Running database
- Running `make` compiles necessary files, running `make clean` removes all the disk files and log file 
- Run `./database < commands.txt > db.log 2>&1` to manual test
- Run `python3 custom_workload.py` to generate the `commands.txt` file
- Can also execute `g++ -std=c++11 -o database database.cpp` to compile to get C++ 11 features
- Make sure to run code on a host Linux OS (not MacOS with a Docker image) when running experiments because I need `perf`. Currently running on an Ubuntu-VM with VirtualBox. Execute the following command while logged in as a root user: `sudo perf stat ./database`
- To test client-server functionality, run `telnet localhost 8081` in a separate terminal to connect to database server as a client

## Notes:
- Need to design a synchronization plan


### Name ideas:
- FeatherDB
- KingDB (King = Raja)
- RajaDB