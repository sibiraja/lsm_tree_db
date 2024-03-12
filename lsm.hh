#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

#define INITIAL_LEVEL_CAPACITY  10
#define SIZE_RATIO              2
#define BUFFER_CAPACITY         5
#define MAX_LEVELS              10
#define NUM_ENTRIES_PER_PAGE    341 // 341 = 4096 bytes / 12 bytes --> 12 bytes bc lsm_data is 4 + 4 + 1 + 3 bytes for alignment = 12 bytes 
#define LSM_DATA_SIZE           12

// Global metadata variables
const char* metadata_filename = "level_metadata.data";
const size_t metadata_size = MAX_LEVELS * sizeof(int); // fixed size for metadata since each level will have an int representing it's `curr_size`
int* metadata_file_ptr;

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

    string disk_file_name_;
    int file_capacity_bytes_;

    level(int capacity, int curr_level) {
        capacity_ = capacity;
        curr_level_ = curr_level;
        sstable_ = new lsm_data[capacity_];

        // cout << "Created level " << curr_level_ << " with capacity " << capacity_ << endl;


        // NEW DISK STORAGE IMPLEMENTATION CODE
        disk_file_name_ = "LEVEL" + to_string(curr_level_) + ".data";
        file_capacity_bytes_ = 4096 * curr_level_;

        // in case we already have data for this level, udpate the curr_size count using the level metadata file and re-construct bloom filters and fence pointers
        struct stat file_exists;
        if (stat (disk_file_name_.c_str(), &file_exists) == 0) {
            curr_size_ = metadata_file_ptr[curr_level_];
            bf_fp_construct();
            // cout << disk_file_name_ << " exists!" << endl; 
        } else {
            // cout << disk_file_name_ << " DNE!" << endl;
        }

        // open LEVEL#.data file (or create it if it DNE) -- Note that we need this line of code in both cases, so it's fine to have it below the if-else statement above

        int fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        if (fd == -1) {
            cout << "Error in opening / creating " << disk_file_name_ << " file! Exiting program" << endl;
            exit(0);
        }

        /* Moving the file pointer to the end of the file*/
        int rflag = lseek(fd, file_capacity_bytes_-1, SEEK_SET);
        
        if(rflag == -1)
        {
            cout << "Lseek failed! Exiting program" << endl;
            close(fd);
            exit(0);
        }

        /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
        rflag = write(fd, "", 1);
        if(rflag == -1)
        {
            cout << "Writing empty string failed! Exiting program" << endl;
            close(fd);
            exit(0);
        }

        // TODO: mmap the LEVEL#.data file and assign sstable_ to it. --> maybe create a new_sstable_ variable and first test it to make sure I don't break any old functionality?
        

        // i think i don't need to memset right now, but perhaps might need to. obv is good practice, but i think it is not needed since it might
        // mess up if i memset the file when it already contains old data from previous database runs.

        
        // END NEW DISK STORAGE IMPLEMENTATION CODE

    }


    // This function should re-construct bloom filters and fence pointers for a given level whenever we have new data at this level 
    void bf_fp_construct() {
        // TODO: implement this function
    }
    

    // only called on level1 since other levels never have to merge data directly from the buffer
    bool merge(lsm_data** child_data_ptr, int num_elements_to_merge) {
        // cout << "Need to merge level " << curr_level_ - 1 << " with level " << curr_level_ << endl;
        auto child_data = *child_data_ptr;

        // check for a potential cascade of merges
        if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ != MAX_LEVELS) {
            // cout << "Need to cascade merge level " << curr_level_ << " with level " << curr_level_ + 1 << endl;
            next_->merge(&sstable_, curr_size_);
            curr_size_ = 0;
            delete[] sstable_;
            sstable_ = new lsm_data[capacity_];
        } else if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ == MAX_LEVELS) {
            cout << "ERROR: CAN'T CASCADE MERGE, DATABASE IS FULL!" << endl;
            exit(0);
            // return false;
        }
        
        // // just printing contents for now
        // cout << "Printing buffer contents" << endl;
        // for (int i = 0; i < num_elements_to_merge; ++i) {
        //     cout << "Index i: (" << child_data[i].key << ", " << child_data[i].value << ")" << endl;
        // }

        // merge 2 sorted arrays into 1 sorted array
        int my_ptr = 0;
        int child_ptr = 0;

        int temp_sstable_ptr = 0;
        lsm_data* temp_sstable = new lsm_data[capacity_];

        while (my_ptr < curr_size_ && child_ptr < num_elements_to_merge) {
            

            // if both are the same key, and either one is deleted, skip over both
            if (sstable_[my_ptr].key == child_data[child_ptr].key && (sstable_[my_ptr].deleted || child_data[child_ptr].deleted)) {
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
            
            if (child_data[child_ptr].deleted) {
                ++child_ptr;
                continue;
            }

            if (sstable_[my_ptr].key < child_data[child_ptr].key) {
                temp_sstable[temp_sstable_ptr] = {sstable_[my_ptr].key, sstable_[my_ptr].value, sstable_[my_ptr].deleted};
                ++my_ptr;
            } else if (sstable_[my_ptr].key > child_data[child_ptr].key) {
                temp_sstable[temp_sstable_ptr] = {child_data[child_ptr].key, child_data[child_ptr].value, sstable_[my_ptr].deleted};
                ++child_ptr;
            } else {
                // else, both keys are equal, so pick the key from the smaller level and skip over the key in the larger level since it is older
                temp_sstable[temp_sstable_ptr] = {child_data[child_ptr].key, child_data[child_ptr].value, sstable_[my_ptr].deleted};
                ++child_ptr;
                ++my_ptr;
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
            if (child_data[child_ptr].deleted) {
                ++child_ptr;
                continue;
            }

            assert(child_data[child_ptr].deleted == false);

            temp_sstable[temp_sstable_ptr] = {child_data[child_ptr].key, child_data[child_ptr].value, sstable_[my_ptr].deleted};
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

    // note: can make this a separate data structure later, but tbh an unsorted array that sorts itself at the end is roughly the same performance as
    // a heap or BST. 
    lsm_data* buffer_ = nullptr;

    buffer() {
        buffer_ = new lsm_data[BUFFER_CAPACITY];
    }
    
    bool insert(lsm_data kv_pair) {
        // first search through buffer, and if it exists, just update the key value struct directly to be the new value
        // --> since simply adding a new entry to the buffer for updating might not cause the old entry to be deleted when merging later on since original comes before the new entry
        // do linear search on the buffer (since buffer isn't sorted)
        for (int i = 0; i < curr_size_; ++i) {
            if (buffer_[i].key == kv_pair.key) {
                // cout << "DUPLICATE ENTRY FOUND FOR: (" << buffer_[i].key << ", " << buffer_[i].value << "), UPDATING VALUE TO BE " << kv_pair.value << endl;
                buffer_[i].value = kv_pair.value;
                return true;
            }
        }

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
        // cout << "Inserted (" << kv_pair.key << ", " << kv_pair.value << ")! curr_size is now " << curr_size_ << endl;
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
        // NEW DISK STORAGE IMPLEMENTATION CODE
        // if `level_metadata.data` file doesn't exist, create it and memset() it to all 0's to represent curr_size is 0 for all levels when we have no data yet
        int fd = open(metadata_filename, O_RDWR | O_CREAT, (mode_t)0600);
        if (fd == -1) {
            cout << "Error in opening / creating global `level_metadata` file! Exiting program" << endl;
            exit(0);
        }

        /* Moving the file pointer to the end of the file*/
        int rflag = lseek(fd, (MAX_LEVELS * sizeof(int))-1, SEEK_SET);
        
        if(rflag == -1)
        {
            cout << "Lseek failed! Exiting program" << endl;
            close(fd);
            exit(0);
        }

        /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
        rflag = write(fd, "", 1);
        if(rflag == -1)
        {
            cout << "Writing empty string failed! Exiting program" << endl;
            close(fd);
            exit(0);
        }

        // memory map the metadata file contents into process memory so we can access it when inside any level::function()
        metadata_file_ptr = (int*) mmap(0,(MAX_LEVELS * sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (metadata_file_ptr == MAP_FAILED)
        {
            cout << "mmap() on the metadata file failed! Exiting program" << endl;
            close(fd);
            exit(0);
        }

        memset(metadata_file_ptr, 0, metadata_size);

        // END NEW DISK STORAGE IMPLEMENTATION CODE

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


    int get(int key, bool called_from_range = false) {
        // do linear search on the buffer (since buffer isn't sorted)
        for (int i = 0; i < buffer_ptr_->curr_size_; ++i) {
            if (buffer_ptr_->buffer_[i].key == key) {
                if (buffer_ptr_->buffer_[i].deleted) {
                    // cout << "(" << key << ", " << buffer_ptr_->buffer_[i].value << ") was DELETED so NOT FOUND!" << endl;
                    if (!called_from_range) {
                        cout << endl; // print empty line for deleted key
                    }
                    return -1;
                }

                // cout << "(" << key << ", " << buffer_ptr_->buffer_[i].value << ") was found at buffer!" << endl;
                if (!called_from_range) {
                    cout << buffer_ptr_->buffer_[i].value << endl;
                } else {
                    cout << key << ":" << buffer_ptr_->buffer_[i].value << " ";
                }
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
                        // cout << "(" << key << ", " << curr_level_ptr->sstable_[midpoint].value << ") was DELETED so NOT FOUND!" << endl;
                        if (!called_from_range) {
                            cout << endl; // print empty line for deleted key
                        }
                        return -1;
                    }

                    // cout << "(" << key << ", " << curr_level_ptr->sstable_[midpoint].value << ") was found at level " << i << endl;
                    if (!called_from_range) {
                        cout << curr_level_ptr->sstable_[midpoint].value << endl;
                    } else {
                        cout << key << ":" << curr_level_ptr->sstable_[midpoint].value << " ";
                    }
                    return curr_level_ptr->sstable_[midpoint].value;
                } else if (curr_level_ptr->sstable_[midpoint].key < key) {
                    l = midpoint + 1;
                } else {
                    r = midpoint - 1;
                }
            }
        }

        // cout << "Key: " << key << " WAS NOT FOUND!" << endl;
        if (!called_from_range) {
            cout << endl; // print empty line to signify no key was found
        }
        return -1;
    }


    void range(int start, int end) {
        for (int i = start; i < end; ++i) {
            get(i, true);
        }
        cout << endl; // to end the line that the space delineated list of all the found pairs in the given range were printed on inside `get()`
    }


    void delete_key(int key) {
        // first search through buffer, and if it exists, just update the key value struct directly to mark as deleted
        // --> since simply adding a new entry to the buffer for deletion might not cause the old entry to be deleted when merging later on since original comes before the new entry
        // do linear search on the buffer (since buffer isn't sorted)
        for (int i = 0; i < buffer_ptr_->curr_size_; ++i) {
            if (buffer_ptr_->buffer_[i].key == key) {
                // cout << "(" << key << ", " << buffer_ptr_->buffer_[i].value << ") was found at buffer, MARKING FOR DELETION!" << endl;
                buffer_ptr_->buffer_[i].deleted = true;
                return;
            }
        }


        // if not found in buffer, then just insert a new key value struct with the deleted flag set as true
        insert({key, 0, true});
        return;
    }
};