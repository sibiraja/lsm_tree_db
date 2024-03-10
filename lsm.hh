#include <iostream>
#include <algorithm>

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
    level* prev_ = nullptr;
    level* next_ = nullptr;

    level(int capacity, int curr_level) {
        capacity_ = capacity;
        curr_level_ = curr_level;
        sstable_ = new lsm_data[capacity_];

        // only create a next level if we are not at the max level
        if (curr_level_ != MAX_LEVELS) {
            next_ = new level(capacity * SIZE_RATIO, curr_level_ + 1);
        }

        cout << "Created level " << curr_level_ << " with capacity " << capacity_ << endl;
    }
    

    // only called on level1 since other levels never have to merge data directly from the buffer
    bool merge(lsm_data** buffer_data_ptr, int num_elements_to_merge) {
        cout << "Need to merge level " << curr_level_ - 1 << " with level " << curr_level_ << endl;
        auto buffer_data = *buffer_data_ptr;

        // check for a potential cascade of merges
        if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ != MAX_LEVELS) {
            cout << "Need to cascade merge level " << curr_level_ << " with level " << curr_level_ + 1 << endl;
            next_->merge(&sstable_, curr_size_);
            curr_size_ = 0;
            delete[] sstable_;
            sstable_ = new lsm_data[capacity_];
        } else if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ == MAX_LEVELS) {
            cout << "ERROR: CAN'T CASCADE MERGE, DATABASE IS FULL!" << endl;
            return false;
        }
        
        // just printing contents for now
        cout << "Printing buffer contents" << endl;
        for (int i = 0; i < num_elements_to_merge; ++i) {
            cout << "Index i: (" << buffer_data[i].key << ", " << buffer_data[i].value << ")" << endl;
        }

        // merge 2 sorted arrays into 1 sorted array
        int my_ptr = 0;
        int child_ptr = 0;

        int temp_sstable_ptr = 0;
        lsm_data* temp_sstable = new lsm_data[capacity_];

        while (my_ptr < curr_size_ && child_ptr < num_elements_to_merge) {
            if (sstable_[my_ptr].key <= buffer_data[child_ptr].key) {
                temp_sstable[temp_sstable_ptr] = {sstable_[my_ptr].key, sstable_[my_ptr].value};
                ++my_ptr;
            } else {
                temp_sstable[temp_sstable_ptr] = {buffer_data[child_ptr].key, buffer_data[child_ptr].value};
                ++child_ptr;
            }

            ++temp_sstable_ptr;
        }

        while (my_ptr < curr_size_ ) {
            temp_sstable[temp_sstable_ptr] = {sstable_[my_ptr].key, sstable_[my_ptr].value};
            ++my_ptr;
            ++temp_sstable_ptr;
        }

        while (child_ptr < num_elements_to_merge ) {
            temp_sstable[temp_sstable_ptr] = {buffer_data[child_ptr].key, buffer_data[child_ptr].value};
            ++child_ptr;
            ++temp_sstable_ptr;
        }

        delete[] sstable_;
        sstable_ = temp_sstable;
        curr_size_ = temp_sstable_ptr;

        return true;
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
            // Sort the buffer before merging
            std::sort(buffer_, buffer_ + curr_size_, [](const lsm_data& a, const lsm_data& b) {
                return a.key < b.key;
            });

            // Merge the sorted buffer into the LSM tree
            level1ptr_->merge(&buffer_, curr_size_);
            
            curr_size_ = 0;
            delete[] buffer_;
            buffer_ = new lsm_data[BUFFER_CAPACITY];
        }

        buffer_[curr_size_] = kv_pair;
        ++curr_size_;
        cout << "Inserted (" << kv_pair.key << ", " << kv_pair.value << ")! curr_size is now " << curr_size_ << endl;
        return true;
    }
};



void print_database(buffer** buff_ptr) {
    cout << "====Printing database contents!===" << endl;
    int total_elements = 0;
    int curr_element = 0;
    auto buff = *buff_ptr;

    cout << "Starting at buffer" << endl;
    
    while(curr_element < buff->curr_size_) {
        cout << "Element " << total_elements << ": (" << buff->buffer_[curr_element].key << ", " << buff->buffer_[curr_element].value << ")" << endl;
        ++curr_element;
        ++total_elements;
    }

    curr_element = 0;
    level* curr_level = buff->level1ptr_;
    for (int i = 0; i < MAX_LEVELS; ++i) {
        cout << "=====Printing data at level " << i + 1 << " =======" << endl;

        while (curr_element < curr_level->curr_size_) {
            cout << "Element " << total_elements << ": (" << curr_level->sstable_[curr_element].key << ", " << curr_level->sstable_[curr_element].value << ")" << endl;
            ++curr_element;
            ++total_elements;
        }

        curr_level = curr_level->next_;
        curr_element = 0;
    }
}