#include <iostream>
#define INITIAL_LEVEL_CAPACITY  10
#define SIZE_RATIO              2
#define BUFFER_CAPACITY         5
#define MAX_LEVELS              10

using namespace std;

struct lsm_data {
    int key;
    int value;
};

// TODO: just have declarations in this file and move implementation of each function to a `lsm.cpp` file later after initial testing



class level {
public:
    int capacity_;
    int curr_size_ = 0;
    int curr_level_;
    lsm_data* sstable_ = nullptr; // initialized to nullptr, and we should maintain an invariant `sstable_ = nullptr` if the SSTable is empty for a level
    level* prev = nullptr;
    level* next = nullptr;

    level(int capacity, int curr_level) {
        capacity_ = capacity;
        curr_level_ = curr_level;
        sstable_ = new lsm_data[capacity_];
    }
    

    // only called on level1 since other levels never have to merge data directly from the buffer
    bool merge(lsm_data** buffer_data_ptr, int num_elements_to_merge) {
        auto buffer_data = *buffer_data_ptr;
        
        // just printing contents for now
        cout << "Printing buffer contents" << endl;
        for (int i = 0; i < num_elements_to_merge; ++i) {
            cout << "Index i: (" << buffer_data[i].key << ", " << buffer_data[i].value << ")" << endl;
        }

        exit(0);
        // CONTINUE FROM HERE
    }
};



class buffer {
public:
    level* level1ptr_;
    int capacity_ = BUFFER_CAPACITY;
    int curr_size_ = 0;
    lsm_data* buffer_ = nullptr; // make this a BST later, but it is array for initial testing purposes

    buffer() {
        // malloc a new level object and assign level1ptr_ to point to that object
        level1ptr_ = new level(INITIAL_LEVEL_CAPACITY, 1);
        buffer_ = new lsm_data[BUFFER_CAPACITY];
    }
    
    bool insert(lsm_data kv_pair) {
        if (curr_size_ == capacity_) {
            level1ptr_->merge(&buffer_, capacity_);
        }

        buffer_[curr_size_] = kv_pair;
        ++curr_size_;
        cout << "Inserted (" << kv_pair.key << ", " << kv_pair.value << ")! curr_size is now " << curr_size_ << endl;
        return true;
    }
};