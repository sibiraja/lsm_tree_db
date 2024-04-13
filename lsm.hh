#ifndef LSM_HH
#define LSM_HH

#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <set>
#include <cassert>
#include "bloom_filter.hh"
#include <climits>

#define INITIAL_LEVEL_CAPACITY      512
#define SIZE_RATIO                  2
// #define BUFFER_CAPACITY             5
#define BUFFER_CAPACITY             50
#define MAX_LEVELS                  10
#define FENCE_PTR_EVERY_K_ENTRIES   512 // 512 = 4096 bytes / 8 bytes --> 8 bytes bc lsm_data is 4 + 4 = 8 bytes. THIS CAN BE A EXPERIMENTAL PARAMETER
#define LSM_DATA_SIZE               8
#define DELETED_FLAG                INT_MIN


using namespace std;

struct lsm_data {
    int key;
    int value;
};


struct fence_ptr {
    int min_key;
    int max_key;
    int offset;
};

// TODO: just have declarations in this file and move implementation of each function to a `lsm.cpp` file later after initial testing


class level {
public:
    int capacity_;
    int curr_size_ = 0;
    int curr_level_;
    level* prev_ = nullptr;
    level* next_ = nullptr;
    fence_ptr* fp_array_ = nullptr;
    int num_fence_ptrs_ = 0; // will store how many fence ptrs we will have by doing curr_size_ / FENCE_PTR_EVERY_K_ENTRIES
    bloom_filter* filter_ = nullptr; // explicitly set to nullptr so calling delete on this if it hasn't been set to an actual bloom filter object won't break our code (deleting nullptr is safe)

    string disk_file_name_;
    int max_file_size;

    level(int capacity, int curr_level);

    // This function should re-construct fence pointers for a given level whenever we have new data at this level 
    void fp_construct();

    // This function should construct bloom filters AND fence pointers --> only called on database startup in case we already have data files that we want to initialize
    // our database with, so we construct bloom filters and fence pointers on startup
    void bf_fp_construct();

    // only called on level1 since other levels never have to merge data directly from the buffer
    bool merge(int num_elements_to_merge, int child_level, lsm_data** buffer_ptr = nullptr);
};



class buffer {
public:
    level* level1ptr_;
    int capacity_ = BUFFER_CAPACITY;
    int curr_size_ = 0;

    // note: can make this a separate data structure later, but tbh an unsorted array that sorts itself at the end is roughly the same performance as
    // a heap or BST. 
    lsm_data* buffer_ = nullptr;

    buffer();
    
    bool insert(lsm_data kv_pair);

    void flush();
};



class lsm_tree {
public:
    buffer* buffer_ptr_;
    
    lsm_tree();

    bool insert(lsm_data kv_pair);

    void flush_buffer();

    void get(int key);

    void range(int start, int end);

    void delete_key(int key);

    void printStats();

    void cleanup();
};

#endif // LSM_HH