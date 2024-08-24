
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

## Domain-Specific Language (DSL) Commands

FeatherDB supports a simple DSL consisting of six commands: `put`, `get`, `range`, `delete`, `load`, and `print stats`. Each command is explained in greater detail below.

### Put

The `put` command inserts a key-value pair into the LSM-Tree. Duplicates are not supported, so a repeated `put` of a key updates the value stored in the tree.

**Syntax:**

`p [INT1] [INT2]`

The `p` indicates a put command with key INT1 and value INT2.

**Example:**

```
p 10 7
p 63 222
p 10 5
```

First, the key 10 is added with value 7. Next, key 63 is added with value 222. The tree now holds two pairs. Finally, the key 10 is updated to have value 5. Note that the tree logically still holds only two pairs. These instructions include only puts; therefore, no output is expected.

### Get

The `get` command retrieves the current value associated with a specified key.

**Syntax:**

`g [INT1]`

The `g` indicates a get command for key INT1. The current value should be printed on a single line if the key exists, and a blank line if the key is not in the tree.

**Example:**

```
p 10 7
p 63 222
g 10
g 15
p 15 5
g 15
```

Output:

```
7

5
```

### Range

The `range` command retrieves values for a sequence of keys.

**Syntax:**

`r [INT1] [INT2]`

The `r` indicates a range request for all the keys from INT1 inclusive to INT2 exclusive. The output should be a space-delineated list of all found pairs (in the format `key:value`); order is irrelevant.

**Example:**

```
p 10 7
p 13 2
p 17 99
p 12 22
r 10 12
r 10 15
r 14 17
r 0 100
```

Output:

```
10:7
10:7 12:22 13:2

10:7 12:22 13:2 17:99
```

### Delete

The `delete` command removes a key-value pair from the LSM-Tree.

**Syntax:**

`d [INT1]`

The `d` indicates a delete command for key INT1.

**Example:**

```
p 10 7
p 12 5
g 10
d 10
g 10
g 12
```

Output:

```
7

5
```

### Load

The `load` command inserts many values into the tree from a specified binary file.

**Syntax:**

`l "/path/to/file_name"`

The `l` command loads key-value pairs from the specified file. The layout of the binary file is continuous `KEYvalueKEYvalue...`.

**Example:**

`l "~/load_file.bin"`

### Print Stats

The `print stats` command provides insights into the current state of the LSM-Tree, helpful for debugging and evaluation.

**Syntax:**

`s`

This command outputs the number of logical key-value pairs, the number of keys in each level, and a dump of the tree showing the key, value, and level.

**Example:**

This command might output:

```
Logical Pairs: 12
LVL1: 3, LVL3: 11
45:56:L1 56:84:L1 91:45:L1
7:32:L3 19:73:L3 32:91:L3 45:64:L3 58:3:L3 61:10:L3 66:4:L3 85:15:L3 91:71:L3 95:87:L3 97:76:L3
```
