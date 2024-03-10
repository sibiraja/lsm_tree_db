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
    bool deleted;
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
        
        // // just printing contents for now
        // cout << "Printing buffer contents" << endl;
        // for (int i = 0; i < num_elements_to_merge; ++i) {
        //     cout << "Index i: (" << buffer_data[i].key << ", " << buffer_data[i].value << ")" << endl;
        // }

        // merge 2 sorted arrays into 1 sorted array
        int my_ptr = 0;
        int child_ptr = 0;

        int temp_sstable_ptr = 0;
        lsm_data* temp_sstable = new lsm_data[capacity_];

        while (my_ptr < curr_size_ && child_ptr < num_elements_to_merge) {
            

            // if both are the same key, and either one is deleted, skip over both
            if (sstable_[my_ptr].key == buffer_data[child_ptr].key && (sstable_[my_ptr].deleted || buffer_data[child_ptr].deleted)) {
                ++my_ptr;
                ++child_ptr;
                continue; // so we don't execute the below conditions
            } 

            // if currently at a deleted key at either list, skip over it
            // --> this case is when the lists have unique keys but they have been deleted themselves already, so we should not merge them
            if (sstable_[my_ptr].deleted) {
                ++my_ptr;
                continue;
            }
            
            if (buffer_data[child_ptr].deleted) {
                ++child_ptr;
                continue;
            }

            if (sstable_[my_ptr].key <= buffer_data[child_ptr].key) {
                temp_sstable[temp_sstable_ptr] = {sstable_[my_ptr].key, sstable_[my_ptr].value, sstable_[my_ptr].deleted};
                ++my_ptr;
            } else {
                temp_sstable[temp_sstable_ptr] = {buffer_data[child_ptr].key, buffer_data[child_ptr].value, sstable_[my_ptr].deleted};
                ++child_ptr;
            }

            ++temp_sstable_ptr;
        }

        while (my_ptr < curr_size_ ) {
            // skip over deleted elements here
            if (sstable_[my_ptr].deleted) {
                ++my_ptr;
                continue;
            }

            assert(sstable_[my_ptr].deleted == false);

            temp_sstable[temp_sstable_ptr] = {sstable_[my_ptr].key, sstable_[my_ptr].value, sstable_[my_ptr].deleted};
            ++my_ptr;
            ++temp_sstable_ptr;
        }

        while (child_ptr < num_elements_to_merge ) {
            // skip over deleted elements here
            if (buffer_data[child_ptr].deleted) {
                ++child_ptr;
                continue;
            }

            assert(buffer_data[child_ptr].deleted == false);

            temp_sstable[temp_sstable_ptr] = {buffer_data[child_ptr].key, buffer_data[child_ptr].value, sstable_[my_ptr].deleted};
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



class lsm_tree {
public:
    buffer* buffer_ptr_;
    level* levels_[MAX_LEVELS + 1]; // index 0 is going to be empty for simplicty since level 0 is the buffer, so we of size MAX_LEVELS + 1 to have enough space for ptrs for each level

    lsm_tree() {
        buffer_ptr_ = new buffer();

        // malloc a new level object and assign level1ptr_ to point to that object
        buffer_ptr_->level1ptr_ = new level(INITIAL_LEVEL_CAPACITY, 1);
        levels_[1] = buffer_ptr_->level1ptr_; // ptr to level 1 is stored in index 1
        
        auto curr_level_ptr = levels_[1];
        for (int i = 2; i <= MAX_LEVELS; ++i) {
            levels_[i] = new level(curr_level_ptr->capacity_ * SIZE_RATIO, i);
            curr_level_ptr->next_ = levels_[i];
            curr_level_ptr = levels_[i];
        }
    }


    bool insert(lsm_data kv_pair) {
        buffer_ptr_->insert(kv_pair);
        return true;
    }


    int get(int key) {
        // do linear search on the buffer (since buffer isn't sorted)
        for (int i = 0; i < buffer_ptr_->capacity_; ++i) {
            if (buffer_ptr_->buffer_[i].key == key) {
                // TODO: add deletion logic here later
                if (buffer_ptr_->buffer_[i].deleted) {
                    cout << "(" << key << ", " << buffer_ptr_->buffer_[i].value << ") was DELETED so NOT FOUND!" << endl;
                    return -1;
                }

                cout << "(" << key << ", " << buffer_ptr_->buffer_[i].value << ") was found at buffer!" << endl;
                return buffer_ptr_->buffer_[i].value;
            }
        }


        // if not found in buffer, then do binary search across each level
        for (int i = 1; i <= MAX_LEVELS; ++i) {
            auto curr_level_ptr = levels_[i];

            int l = 0;
            int r = curr_level_ptr->curr_size_ - 1;

            while (l <= r) {
                int midpoint = (l + r) / 2;
                if (curr_level_ptr->sstable_[midpoint].key == key) {

                    // check if deleted here, otherwise, return it's value 
                    if (curr_level_ptr->sstable_[midpoint].deleted) {
                        cout << "(" << key << ", " << curr_level_ptr->sstable_[midpoint].value << ") was DELETED so NOT FOUND!" << endl;
                        return -1;
                    }

                    cout << "(" << key << ", " << curr_level_ptr->sstable_[midpoint].value << ") was found at level " << i << endl;
                    return curr_level_ptr->sstable_[midpoint].value;
                } else if (curr_level_ptr->sstable_[midpoint].key < key) {
                    l = midpoint + 1;
                } else {
                    r = midpoint - 1;
                }
            }
        }

        cout << "Key: " << key << " WAS NOT FOUND!" << endl;
        return -1;
    }


    void delete_key(int key) {
        // first search through buffer, and if it exists, just update the key value struct directly to mark as deleted
        // --> since simply adding a new entry to the buffer for deletion might not cause the old entry to be deleted when merging later on since original comes before the new entry
        // do linear search on the buffer (since buffer isn't sorted)
        for (int i = 0; i < buffer_ptr_->capacity_; ++i) {
            if (buffer_ptr_->buffer_[i].key == key) {
                cout << "(" << key << ", " << buffer_ptr_->buffer_[i].value << ") was found at buffer, MARKING FOR DELETION!" << endl;
                buffer_ptr_->buffer_[i].deleted = true;
                return;
            }
        }


        // if not found in buffer, then just insert a new key value struct with the deleted flag set as true
        insert({key, 0, true});
        return;
    }
};