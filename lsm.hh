#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <set>

#define INITIAL_LEVEL_CAPACITY      341
#define SIZE_RATIO                  2
#define BUFFER_CAPACITY             5
#define MAX_LEVELS                  10
#define FENCE_PTR_EVERY_K_ENTRIES   341 // 341 = 4096 bytes / 12 bytes --> 12 bytes bc lsm_data is 4 + 4 + 1 + 3 bytes for alignment = 12 bytes. THIS CAN BE A EXPERIMENTAL PARAMETER
#define LSM_DATA_SIZE               12

// Global metadata variables
const char* metadata_filename = "level_metadata.data";
const size_t metadata_size = MAX_LEVELS * sizeof(int); // fixed size for metadata since each level will have an int representing it's `curr_size`
int* metadata_file_ptr;

// global array that will contain pointers to each level object that we create so we can access attributes easily 
class level; // forward declaration so compiler knows what level type is before we hit the next line of code
level* levels_[MAX_LEVELS + 1]; // index 0 is going to be empty for simplicty since level 0 is the buffer, so we of size MAX_LEVELS + 1 to have enough space for ptrs for each level

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
    level* prev_ = nullptr;
    level* next_ = nullptr;

    string disk_file_name_;
    int max_file_size;

    level(int capacity, int curr_level) {
        capacity_ = capacity;
        curr_level_ = curr_level;

        // cout << "Creating level " << curr_level_ << " with capacity " << capacity_ << endl;


        // NEW DISK STORAGE IMPLEMENTATION CODE
        disk_file_name_ = "LEVEL" + to_string(curr_level_) + ".data";
        max_file_size = 4096 * curr_level_;

        // in case we already have data for this level, udpate the curr_size count using the level metadata file and re-construct bloom filters and fence pointers
        struct stat file_exists;
        if (stat (disk_file_name_.c_str(), &file_exists) == 0) {
            curr_size_ = metadata_file_ptr[curr_level_];
            // cout << "On database startup, level " << curr_level_ << " already has " << curr_size_ << " data entries!" << endl;
            bf_fp_construct();
        } 
        // else, create a file for this level
        else {
            int fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
            if (fd == -1) {
                cout << "Error in opening / creating " << disk_file_name_ << " file! Exiting program" << endl;
                exit(0);
            }

            /* Moving the file pointer to the end of the file*/
            int rflag = lseek(fd, max_file_size-1, SEEK_SET);
            
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

            lsm_data* curr_level_data = (lsm_data*) mmap(0, max_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (curr_level_data == MAP_FAILED)
            {
                close(fd);
                printf("Mmap failed.\n");
                exit(0);
            }
            memset(curr_level_data, 0, max_file_size);
            rflag = msync(curr_level_data, max_file_size, MS_SYNC);

            if(rflag == -1)
            {
                printf("Unable to msync.\n");
                exit(0);
            }
            rflag = munmap(curr_level_data, max_file_size);
            if(rflag == -1)
            {
                printf("Unable to munmap.\n");
                exit(0);
            }
            close(fd);
        }

    }


    // This function should re-construct bloom filters and fence pointers for a given level whenever we have new data at this level 
    void bf_fp_construct() {
        // TODO: implement this function
    }
    

    // only called on level1 since other levels never have to merge data directly from the buffer
    bool merge(int num_elements_to_merge, int child_level, lsm_data** buffer_ptr = nullptr) {
        // cout << "Need to merge level " << curr_level_ - 1 << " with level " << curr_level_ << endl;

        // check for a potential cascade of merges
        if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ != MAX_LEVELS) {
            // cout << "Need to cascade merge level " << curr_level_ << " with level " << curr_level_ + 1 << endl;
            next_->merge(curr_size_, curr_level_);

            // reset data for this level as it has been merged with another level already
            curr_size_ = 0;

            int curr_fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
            if (curr_fd == -1) {
                cout << "Error in opening / creating " << disk_file_name_ << " file! Exiting program" << endl;
                exit(0);
            }
            
            lsm_data* curr_file_ptr = (lsm_data*) mmap(0,max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
            if (curr_file_ptr == MAP_FAILED)
            {
                cout << "mmap() on the current level's file failed! Exiting program" << endl;
                close(curr_fd);
                exit(0);
            }

            memset(curr_file_ptr, 0, max_file_size);
            int rflag = msync(curr_file_ptr, max_file_size, MS_SYNC);
            rflag = munmap(curr_file_ptr, max_file_size);
            if(rflag == -1)
            {
                printf("Unable to munmap.\n");
            }
            close(curr_fd);
            // cout << "Memset level " << curr_level_ << "'s file to all 0's bc it has been merged with another level!" << endl;
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

        // MMAP CONTENTS OF CHILD DATA AND CURRENT LEVEL DATA HERE, NOT AT TOP OF MERGE FUNCTION SO WE DON'T RUN OUT OF RAM MEMORY WHEN CASCADE MERGING

        int child_fd = -1;
        lsm_data* new_child_data;
        if (child_level == 0) {
            // cout << "child is buffer, no need to mmap()!" << endl;
            assert(buffer_ptr != nullptr);
            new_child_data = *buffer_ptr;
        } 
        
        // mmap child's file if it is a level and not the buffer
        else {
            assert(buffer_ptr == nullptr);
            // cout << "child is a level, mmap()'ing!" << endl;
            // new_child_data = mmap the level's disk file
            child_fd = open(levels_[child_level]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
            if (child_fd == -1) {
                cout << "Error in opening / creating " << levels_[child_level]->disk_file_name_ << " file! Exiting program" << endl;
                exit(0);
            }
            new_child_data = (lsm_data*) mmap(0, levels_[child_level]->max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, child_fd, 0);

            if (new_child_data == MAP_FAILED)
            {
                cout << "mmap() on the child's file failed! Exiting program" << endl;
                close(child_fd);
                exit(0);
            }
        }

        // mmap() current file's contents
        int curr_fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        if (curr_fd == -1) {
            cout << "Error in opening / creating " << disk_file_name_ << " file! Exiting program" << endl;
            exit(0);
        }
        lsm_data* new_curr_sstable = (lsm_data*) mmap(0, max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
        if (new_curr_sstable == MAP_FAILED)
        {
            cout << "mmap() on the current level's file failed! Exiting program" << endl;
            close(child_fd);
            exit(0);
        }
        // cout << "mmap()'ed the current level's file!" << endl;


        // create and mmap a temporary file that we will write the new merged contents to. this file will later be renamed the LEVEL#.data file
        int temp_fd = open("TEMP.data", O_RDWR | O_CREAT, (mode_t)0600);
        if (temp_fd == -1) {
            cout << "Error in opening / creating TEMP.data file! Exiting program" << endl;
            exit(0);
        }

        /* Moving the file pointer to the end of the file*/
        int rflag = lseek(temp_fd, max_file_size-1, SEEK_SET);
        
        if(rflag == -1)
        {
            cout << "Lseek failed! Exiting program" << endl;
            close(temp_fd);
            exit(0);
        }

        /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
        rflag = write(temp_fd, "", 1);
        if(rflag == -1)
        {
            cout << "Writing empty string failed! Exiting program" << endl;
            close(temp_fd);
            exit(0);
        }

        lsm_data* new_temp_sstable = (lsm_data*) mmap(0, max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, temp_fd, 0);
        if (new_temp_sstable == MAP_FAILED)
        {
            cout << "mmap() on the current level's file failed! Exiting program" << endl;
            close(temp_fd);
            exit(0);
        }
        // cout << "mmap()'ed the temp data file!" << endl;


        // merge 2 sorted arrays into 1 sorted array
        int my_ptr = 0;
        int child_ptr = 0;

        int temp_sstable_ptr = 0;

        while (my_ptr < curr_size_ && child_ptr < num_elements_to_merge) {

            // assert(new_child_data[child_ptr].key == child_data[child_ptr].key && new_child_data[child_ptr].value == child_data[child_ptr].value && new_child_data[child_ptr].deleted == child_data[child_ptr].deleted);
            // assert(new_curr_sstable[my_ptr].key == sstable_[my_ptr].key && new_curr_sstable[my_ptr].value == sstable_[my_ptr].value && new_curr_sstable[my_ptr].deleted == sstable_[my_ptr].deleted);
            

            // if both are the same key, and either one is deleted, skip over both
            if (new_curr_sstable[my_ptr].key == new_child_data[child_ptr].key && (new_curr_sstable[my_ptr].deleted || new_child_data[child_ptr].deleted)) {
                ++my_ptr;
                ++child_ptr;
                continue; // so we don't execute the below conditions
            } 

            // if currently at a deleted key at either list, skip over it
            // --> this case is when the lists have unique keys but they have been deleted themselves already, so we should not merge them
            if (new_curr_sstable[my_ptr].deleted) {
                ++my_ptr;
                continue;
            }
            
            if (new_child_data[child_ptr].deleted) {
                ++child_ptr;
                continue;
            }

            if (new_curr_sstable[my_ptr].key < new_child_data[child_ptr].key) {
                new_temp_sstable[temp_sstable_ptr] = {new_curr_sstable[my_ptr].key, new_curr_sstable[my_ptr].value, new_curr_sstable[my_ptr].deleted};
                ++my_ptr;
            } else if (new_curr_sstable[my_ptr].key > new_child_data[child_ptr].key) {
                new_temp_sstable[temp_sstable_ptr] = {new_child_data[child_ptr].key, new_child_data[child_ptr].value, new_child_data[my_ptr].deleted};
                ++child_ptr;
            } else {
                // else, both keys are equal, so pick the key from the smaller level and skip over the key in the larger level since it is older
                new_temp_sstable[temp_sstable_ptr] = {new_child_data[child_ptr].key, new_child_data[child_ptr].value, new_child_data[my_ptr].deleted};
                ++child_ptr;
                ++my_ptr;
            }

            ++temp_sstable_ptr;
        }

        while (my_ptr < curr_size_ ) {
            // skip over deleted elements here
            // assert(new_curr_sstable[my_ptr].key == sstable_[my_ptr].key && new_curr_sstable[my_ptr].value == sstable_[my_ptr].value && new_curr_sstable[my_ptr].deleted == sstable_[my_ptr].deleted);
            if (new_curr_sstable[my_ptr].deleted) {
                ++my_ptr;
                continue;
            }

            // assert(sstable_[my_ptr].deleted == false);

            new_temp_sstable[temp_sstable_ptr] = {new_curr_sstable[my_ptr].key, new_curr_sstable[my_ptr].value, new_curr_sstable[my_ptr].deleted};
            ++my_ptr;
            ++temp_sstable_ptr;
        }

        while (child_ptr < num_elements_to_merge ) {
            // skip over deleted elements here
            // assert(new_child_data[child_ptr].key == child_data[child_ptr].key && new_child_data[child_ptr].value == child_data[child_ptr].value && new_child_data[child_ptr].deleted == child_data[child_ptr].deleted);
            if (new_child_data[child_ptr].deleted) {
                ++child_ptr;
                continue;
            }

            // assert(child_data[child_ptr].deleted == false);

            new_temp_sstable[temp_sstable_ptr] = {new_child_data[child_ptr].key, new_child_data[child_ptr].value, new_child_data[my_ptr].deleted};
            ++child_ptr;
            ++temp_sstable_ptr;
        }


        curr_size_ = temp_sstable_ptr;

        // write curr_size to metadata file
        metadata_file_ptr[curr_level_] = curr_size_;
        rflag = msync(metadata_file_ptr, (MAX_LEVELS * sizeof(int)), MS_SYNC);
        if(rflag == -1)
        {
            printf("Unable to msync to metadata file.\n");
            exit(0);
        }


        // MSYNC temp data file and munmap temp file
        if (new_temp_sstable != NULL && temp_fd != -1)
        {
            /*Syncing the contents of the memory with file, flushing the pages to disk*/

            rflag = msync(new_temp_sstable, max_file_size, MS_SYNC);
            if(rflag == -1)
            {
                printf("Unable to msync.\n");
            }
            rflag = munmap(new_temp_sstable, max_file_size);
            if(rflag == -1)
            {
                printf("Unable to munmap.\n");
            }
            close(temp_fd);
        }

        
        // munmap child
        if (child_fd != -1) {
            int child_rflag = munmap(new_child_data, levels_[child_level]->max_file_size);
            if(child_rflag == -1)
            {
                printf("Unable to munmap child's file.\n");
            }
            // cout << "child is a level, just munmap()'ed!" << endl;
            close(child_fd);
        }

        // munmap curr level file
        rflag = munmap(new_curr_sstable, max_file_size);
        if(rflag == -1)
        {
            printf("Unable to munmap current level's file.\n");
        }
        // cout << "munmap()'ed the current level's file!" << endl;
        close(curr_fd);

        
        // NOTE: KEEP THE BELOW IF-ELSE CHECKS BC THE STATEMENTS INSIDE THE IF CONDITIONS NEED TO EXECUTE!!!!!
        // DELETE original curr level file, rename temp data file
        if (remove(disk_file_name_.c_str()) == 0) {
            // cout << disk_file_name_ << " deleted successfully" << endl;
        } else {
            cout << "Error in deleting " << disk_file_name_ << endl;
            exit(0);
        }

        if (rename("TEMP.data", disk_file_name_.c_str()) == 0) {
            // cout << "Successfully renamed temp file to " << disk_file_name_ << endl;
        } else {
            cout << "Error in renaming temp file" << endl;
            exit(0);
        }

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
            level1ptr_->merge(curr_size_, 0, &buffer_);
            
            curr_size_ = 0;
            delete[] buffer_;
            buffer_ = new lsm_data[BUFFER_CAPACITY];
        }

        buffer_[curr_size_] = kv_pair;
        ++curr_size_;
        // cout << "Inserted (" << kv_pair.key << ", " << kv_pair.value << ")! curr_size is now " << curr_size_ << endl;
        return true;
    }

    void flush() {
        // Sort the buffer before merging
        std::sort(buffer_, buffer_ + curr_size_, [](const lsm_data& a, const lsm_data& b) {
            return a.key < b.key;
        });

        // Merge the sorted buffer into the LSM tree
        level1ptr_->merge(curr_size_, 0, &buffer_);
        
        curr_size_ = 0;
        delete[] buffer_;
        buffer_ = new lsm_data[BUFFER_CAPACITY];
    }
};



class lsm_tree {
public:
    buffer* buffer_ptr_;
    
    lsm_tree() {
        // NEW DISK STORAGE IMPLEMENTATION CODE
        // if `level_metadata.data` file doesn't exist, create it and memset() it to all 0's to represent curr_size is 0 for all levels when we have no data yet
        struct stat metadata_file_exists;
        if (stat (metadata_filename, &metadata_file_exists) != 0) {
            // cout << "global metadata file DNE, so making a new one and starting database from fresh!" << endl;
        
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
            rflag = msync(metadata_file_ptr, metadata_size, MS_SYNC);
            if (rflag == -1) {
                cout << "unable to msync metadata file" << endl;
                exit(0);
            }
        }
        // if file already exists, just mmap it
        else {
            int fd = open(metadata_filename, O_RDWR | O_CREAT, (mode_t)0600);
            if (fd == -1) {
                cout << "Error in opening / creating global `level_metadata` file! Exiting program" << endl;
                exit(0);
            }

            metadata_file_ptr = (int*) mmap(0,(MAX_LEVELS * sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
            if (metadata_file_ptr == MAP_FAILED)
            {
                cout << "mmap() on the metadata file failed! Exiting program" << endl;
                close(fd);
                exit(0);
            }
        }

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

    void flush_buffer() {
        buffer_ptr_->flush();
    }

    // TODO: performance should asymptotically be better than simply querying every key in the range using GET.
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


        // if not found in buffer, then search across each LEVEL#.data file
        bool result_found = false;
        int result;
        for (int i = 1; i <= MAX_LEVELS; ++i) {
            auto curr_level_ptr = levels_[i];

            int curr_fd = open(levels_[i]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
            if (curr_fd == -1) {
                cout << "Error in opening / creating " << levels_[i]->disk_file_name_ << " file! Exiting program" << endl;
                exit(0);
            }
            lsm_data* new_curr_sstable = (lsm_data*) mmap(0, levels_[i]->max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
            if (new_curr_sstable == MAP_FAILED)
            {
                cout << "mmap() on the current level's file failed! Exiting program" << endl;
                close(curr_fd);
                exit(0);
            }

            for (int j = 0; j < levels_[i]->curr_size_; ++j) {
                if (new_curr_sstable[j].key == key) {
                    // check if deleted here, otherwise, return it's value 
                    if (new_curr_sstable[j].deleted) {
                        // cout << "(" << key << ", " << curr_level_ptr->sstable_[midpoint].value << ") was DELETED so NOT FOUND!" << endl;
                        if (!called_from_range) {
                            cout << endl; // print empty line for deleted key
                        }
                        result_found = true;
                        result = -1;
                    } else {
                        // cout << "(" << key << ", " << curr_level_ptr->sstable_[midpoint].value << ") was found at level " << i << endl;
                        if (!called_from_range) {
                            cout << new_curr_sstable[j].value << endl;
                        } else {
                            cout << key << ":" << new_curr_sstable[j].value << " ";
                        }
                        result_found = true;
                        result = new_curr_sstable[j].value;
                    }
                }

                if (result_found) {
                    break;
                }
            }

            // munmap the file we just mmap()'d
            int rflag = munmap(new_curr_sstable, levels_[i]->max_file_size);
            if (rflag == -1)
            {
                printf("Unable to munmap.\n");
                exit(0);
            }
            close(curr_fd);


            if (result_found) {
                return result;
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

    // TODO: figure out logic for print stats function, right now this is just for my own testing purposes
    void printStats() {

        // main logic behind this is to iterate from buffer to larger levels, and keep track of valid and deleted keys using sets to correctly count logical pairs, but
        // this can get out of hand real fast once our database has millions of keys that it has stored, so `TODO: come back to this later to scale better`
        vector<string> terminal_output;
        terminal_output.push_back(""); // push back empty string which we can go back and later edit to store the count of logical key value pairs
        terminal_output.push_back(""); // same logic as above, this stores the count of entries at each level
        set<int> deleted_keys;
        set<int> valid_keys;

        // print contents of buffer
        string buffer_contents = "";
        // cout << "===Buffer contents======" << endl;
        for (int i = 0; i < buffer_ptr_->curr_size_; ++i) {
            auto curr_data = buffer_ptr_->buffer_[i];
            string temp = to_string(curr_data.key) + ":" + to_string(curr_data.value) + ":L0 ";
            buffer_contents += temp;

            // note that buffer will never have duplicate keys, so no need to cross-reference across the deleted and valid sets here
            if (curr_data.deleted) {
                deleted_keys.insert(curr_data.key);
            } else {
                valid_keys.insert(curr_data.key);
            }
            // cout << curr_data.key << ":" << curr_data.value << ":L0" << endl;
        }
        terminal_output[1] += "LVL0: " + to_string(buffer_ptr_->curr_size_) + ", ";
        terminal_output.push_back(buffer_contents);

        // print contents of each level's disk file by mmap and munmap
        for (int i = 1; i <= MAX_LEVELS; ++i) {
            // cout << "===Level " << i << " contents===" << endl;
            auto curr_level_ptr = levels_[i];

            int curr_fd = open(levels_[i]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
            if (curr_fd == -1) {
                cout << "Error in opening / creating " << levels_[i]->disk_file_name_ << " file! Exiting program" << endl;
                exit(0);
            }
            lsm_data* new_curr_sstable = (lsm_data*) mmap(0, levels_[i]->max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
            if (new_curr_sstable == MAP_FAILED)
            {
                cout << "mmap() on the current level's file failed! Exiting program" << endl;
                close(curr_fd);
                exit(0);
            }

            string curr_level_contents = "";
            for (int j = 0; j < levels_[i]->curr_size_; ++j) {
                string temp = to_string(new_curr_sstable[j].key) + ":" + to_string(new_curr_sstable[j].value) + ":L" + to_string(i) + " ";
                curr_level_contents += temp;
                // cout << new_curr_sstable[j].key << ":" << new_curr_sstable[j].value << ":L" << i << endl;

                // if current key-value pair is not deleted and the key has not been previously deleted (due to an earlier level), add it to valid keys set
                if (!new_curr_sstable[j].deleted && deleted_keys.find(new_curr_sstable[j].key) == deleted_keys.end()) {
                    valid_keys.insert(new_curr_sstable[j].key);
                }
                // else if it is deleted, only add it to the deleted set if not already in valid set (due to an earlier level)
                else if (new_curr_sstable[j].deleted && valid_keys.find(new_curr_sstable[j].key) == valid_keys.end()) {
                    deleted_keys.insert(new_curr_sstable[j].key);
                }
            }
            terminal_output[1] += "LVL" + to_string(i) + ": " + to_string(levels_[i]->curr_size_) + ", ";
            terminal_output.push_back(curr_level_contents);

            // munmap the file we just mmap()'d
            int rflag = munmap(new_curr_sstable, levels_[i]->max_file_size);
            if (rflag == -1)
            {
                printf("Unable to munmap.\n");
                exit(0);
            }
            close(curr_fd);
        }

        terminal_output[0] = "Logical Pairs: " + to_string(valid_keys.size());

        for (int i = 0; i < terminal_output.size(); ++i) {
            if (terminal_output[i].length() > 0) {
                cout << terminal_output[i] << endl;
                // empty line to match formatting as per project requirements document
                if (i >= 1) {
                    cout << endl;
                }
            }
        }
    }
};