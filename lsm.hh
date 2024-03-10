#include <iostream>
#define INITIAL_LEVEL_CAPACITY  10
#define SIZE_RATIO              2
#define BUFFER_CAPACITY         5

using namespace std;

struct lsm_data {
    int key;
    int value;
};

// TODO: just have declarations in this file and move implementation of each function to a `lsm.cpp` file later after initial testing



class level {
public:
    int capacity_;
    int curr_size_;
    lsm_data* sstable_; 
    level* prev;
    level* next;

    level(int capacity) {
        capacity_ = capacity;
        curr_size_ = 0;
        sstable_ = nullptr; // initialized to nullptr, and we should maintain an invariant `sstable_ = nullptr` if the SSTable is empty for a level
        prev = nullptr;
        next = nullptr;
        sstable_ = new lsm_data[capacity_];
    }
    

    // only called on level1 since other levels never have to merge data directly from the buffer
    bool merge_buffer(lsm_data** buffer_data_ptr) {
        auto buffer_data = *buffer_data_ptr;
        
        // just printing contents for now
        cout << "Printing buffer contents" << endl;
        for (int i = 0; i < BUFFER_CAPACITY; ++i) {
            cout << "Index i: (" << buffer_data[i].key << ", " << buffer_data[i].value << ")" << endl;
        }

        exit(0);
        // CONTINUE FROM HERE
    }
};



class buffer {
public:
    level* level1ptr_;
    int capacity_;
    int curr_size_;
    lsm_data* buffer_; // make this a BST later, but it is array for initial testing purposes

    buffer() {
        // malloc a new level object and assign level1ptr_ to point to that object
        level1ptr_ = new level(INITIAL_LEVEL_CAPACITY);
        capacity_ = BUFFER_CAPACITY;
        curr_size_ = 0;
        buffer_ = nullptr;
        buffer_ = new lsm_data[BUFFER_CAPACITY];
    }
    
    bool insert(lsm_data kv_pair) {
        if (curr_size_ == capacity_) {
            // if (!flush()){
            //     return false;
            // }
            level1ptr_->merge_buffer(&buffer_);
        }

        buffer_[curr_size_] = kv_pair;
        ++curr_size_;
        cout << "Inserted (" << kv_pair.key << ", " << kv_pair.value << ")! curr_size is now " << curr_size_ << endl;
        return true;
    }

    // bool flush() {
    //     // if level1 doesn't have enough space to hold all the elements currently in the buffer, return error
    //     if (level1ptr_->capacity_ - level1ptr_->curr_size_ < curr_size_) {
    //         // TODO: implement flushing/merging functionality with levels later, but for now just testing buffer functionality
    //         cout << "Level 1 is full as well! LSM Tree can't handle any more lsm_data!\n" << endl;
    //         return false;
    //     } else {
    //         // merge 2 sorted arrays by first mallocing a new sorted array that contains merged elements, changing level's lsm_data arr to point to this new sorted array, and deleting the old arr
    //         level1ptr_->merge_buffer(&buffer_);
    //     }


    //     // reset curr_size_
    //     curr_size_ = 0;
    //     return true;
    // }
};