# lsm_tree_db
Log-structured merge-tree

##
- Docker for debugging with GDB: `docker build -t lsm_tree .`, and `docker run --privileged -it --rm -v /Users/sibiraja/Desktop/lsm_tree_db:/usr/src/lsm_tree lsm_tree`
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
- **profile using iostat and cachegrind for now**
- if i can use perf on FAS Cluster: `salloc --exclusive --nodes=1 --cpus-per-task=8 --mem=32G -p test -t 0-00:10` --> this gets exclusive access to a node
    - need to verify if perf is possible on cluster though
- Profiling cache statistics: `valgrind --tool=cachegrind ./database` and `cg_annotate cachegrind.out.[PID HERE]`
- iostat: `iostat -dx 2` to report on read/write IOs. Can report averages or minimum/maximum across the metrics for each time interval. Perhaps maximum IOs would be more insightful to see how expensive performance can be.
- figure out how to measure overall throughput


### Name ideas:
- FeatherDB
- KingDB (King = Raja)
- RajaDB