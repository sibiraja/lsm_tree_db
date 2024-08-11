# FeatherDB

FeatherDB is a high-performance NoSQL database designed using a Log-Structured Merge-tree (LSM-tree) architecture. It supports up to 200 million updates per second by leveraging advanced techniques such as write buffering, deferred persistence, and a leveling merge policy. Optimizations such as Bloom filters and fence pointers are also integrated to enhance read operations and overall query speed.

## Features

- **High Throughput**: Capable of handling 200M updates/second.
- **LSM-tree Architecture**: Implements write buffering and deferred persistence to optimize write throughput.
- **Read Optimizations**: Uses Bloom filters and fence pointers to enhance read performance and speed.
- **Client-Server Model**: Supports interactions through a basic telnet interface.

## Getting Started

### Prerequisites

- Linux OS (Recommended: Ubuntu via VirtualBox for full functionality)
- Docker (Optional for debugging)
- Python 3.x
- GCC with support for C++11

### Building the Project

1. **Compile the Database**
   ```bash
   make
   ```
   To clean the build:
   ```bash
   make clean
   ```

2. **Running the Database**
   - For manual testing:
     ```bash
     ./database < commands.txt > db.log 2>&1
     ```
   - Generate `commands.txt`:
     ```bash
     python3 custom_workload.py
     ```

3. **Client-Server Functionality**
   - To test the server functionality, run:
     ```bash
     telnet localhost 8081
     ```

### Debugging

- **Local Debugging with GDB**
  - Build and run a Docker container:
    ```bash
    docker build -t lsm_tree .
    docker run --privileged -it --rm -v /path/to/lsm_tree_db:/usr/src/lsm_tree lsm_tree
    ```
  - Note: Running the database natively on Linux is recommended for accurate performance testing.

- **Profiling Tools**
  - Using `valgrind` for cache statistics:
    ```bash
    valgrind --tool=cachegrind ./database
    cg_annotate cachegrind.out.<PID>
    ```
  - Disk I/O monitoring with `iostat`:
    ```bash
    iostat -dx 2
    ```
