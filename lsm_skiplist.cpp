#include "lsm.hh"

// Global metadata variables
const char* metadata_filename = "data/level_metadata.data";
const size_t metadata_size = MAX_LEVELS * sizeof(int); // fixed size for metadata since each level will have an int representing it's `curr_size`
int* metadata_file_ptr;
int metadata_file_descriptor;

// global array that will contain pointers to each level object that we create so we can access attributes easily 
class level; // forward declaration so compiler knows what level type is before we hit the next line of code
level* levels_[MAX_LEVELS + 1]; // index 0 is going to be empty for simplicty since level 0 is the buffer, so we of size MAX_LEVELS + 1 to have enough space for ptrs for each level

using namespace std;

// implementation of the level class functions

// level::level() constructor
//      This constructor either starts a level from scratch or reuses any data that we have on disk for
//      a level
level::level(uint64_t capacity, int curr_level) {
    lock_guard<mutex> curr_level_lock(this->mutex_);
    capacity_ = capacity;
    curr_level_ = curr_level;

    // cout << "Creating level " << curr_level_ << " with capacity " << capacity_ << endl;


    // NEW DISK STORAGE IMPLEMENTATION CODE
    disk_file_name_ = "data/LEVEL" + to_string(curr_level_) + ".data";
    // max_file_size = 4096 * curr_level_;
    // max_file_size = ((this->capacity_ * sizeof(lsm_data) + 4095) / 4096) * 4096; // this is so each level's disk file is rounded to the nearest page in bytes (size-wise)
    uint64_t calculated_size = this->capacity_ * sizeof(lsm_data);
    
    if (calculated_size / sizeof(lsm_data) != this->capacity_) {
        std::cerr << "Overflow detected when calculating base size!" << std::endl;
        cout << "calculated_size / sizeof(lsm_data) = " << calculated_size / sizeof(lsm_data) << "| this->capacity_: " << this->capacity_ << endl;
        exit(EXIT_FAILURE);
    }

    if (calculated_size % 4096 != 0) {
        cout << "We need to pad to round up to nearest pagesize!" << endl;
        uint64_t padded_size = calculated_size + 4095;
        if (padded_size < calculated_size) {  // Check for overflow
            std::cerr << "Overflow detected after adding padding!" << std::endl;
            exit(EXIT_FAILURE);
        }
        max_file_size = (padded_size / 4096) * 4096;
        // if (max_file_size < padded_size) {  // Check for overflow
        //     std::cerr << "Overflow detected after adjusting to block size!" << std::endl;
        //     cout << "padded_size: " << padded_size << " | max_file_size: " << max_file_size << endl;
        //     exit(EXIT_FAILURE);
        // }
    } else {
        max_file_size = calculated_size;
        // cout << "Max file size for level " << curr_level_ << ": " << max_file_size << endl;
    }


    // in case we already have data for this level, udpate the curr_size count using the level metadata file and re-construct bloom filters and fence pointers
    struct stat file_exists;
    if (stat (disk_file_name_.c_str(), &file_exists) == 0) {
        curr_size_ = metadata_file_ptr[curr_level_];
        // cout << "On database startup, level " << curr_level_ << " already has " << curr_size_ << " data entries but has max_file_size: " << max_file_size << endl;

        // calculate number of fence pointers we need
        num_fence_ptrs_ = curr_size_ / FENCE_PTR_EVERY_K_ENTRIES;
        if (curr_size_ % FENCE_PTR_EVERY_K_ENTRIES != 0) {
            num_fence_ptrs_ += 1;
        }

        bf_fp_construct();
        // cout << "Finished reconstructing level " << curr_level_ << endl;
    } 
    // else, create a file for this level
    else {
        int fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        if (fd == -1) {
            cout << "Error in opening / creating " << disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
            exit(0);
        }

        /* Moving the file pointer to the end of the file*/
        // cout << "Attempting lseek on level " << this->curr_level_ << " with size " << max_file_size << endl;
        int rflag = lseek(fd, max_file_size-1, SEEK_SET);
        
        if(rflag == -1)
        {
            cout << "Lseek failed in creating an empty file for level " << this->curr_level_ << "! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(fd);
            exit(0);
        }

        /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
        // cout << "Going to write empty string to EOF" << endl;
        rflag = write(fd, "", 1);
        if(rflag == -1)
        {
            cout << "Writing empty string failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(fd);
            exit(0);
        }

        // cout << "Going to mmap()" << endl;
        lsm_data* curr_level_data = (lsm_data*) mmap(0, max_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (curr_level_data == MAP_FAILED)
        {
            close(fd);
            cout << "Mmap failed on level " << this->curr_level_ << " 's disk file" << endl; 
            exit(0);
        }
        
        // No longer memset()'ing each level's disk file beacause I noticed that it takes a long time to memset for larger files, and I don't need
        // it for correctness anyways since we only access portions of the file using the capacity_ and/or curr_size_ variables. Since those are
        // correctly maintained -- and metadata file is memset() in LSM tree startup anyways -- we don't need to memset data files on disk.

        // cout << "Going to memset()" << endl;
        // memset(curr_level_data, 0, max_file_size);

        rflag = msync(curr_level_data, max_file_size, MS_SYNC);

        if(rflag == -1)
        {
            printf("Unable to msync.\n");
            exit(0);
        }
        // cout << "Going to fsync()" << endl;
        // fsync the file descriptor to ensure data is written to disk
        // if (fsync(fd) == -1) {
        //     perror("fsync error");
        //     exit(0);
        //     // Handle error
        // }
        // cout << "Going to munmap()" << endl;
        rflag = munmap(curr_level_data, max_file_size);
        if(rflag == -1)
        {
            printf("Unable to munmap.\n");
            exit(0);
        }
        // cout << "Done setting up level " << curr_level_ << "!" << endl;
        close(fd);
    }

}

// level::fp_construct()
//      This function re-constructs fence pointers for a given level whenever the level gets new data (i.e on a merge) 
// void level::fp_construct() {
//     // Open the level's data file
//     int fd = open(disk_file_name_.c_str(), O_RDONLY);
//     if (fd == -1) {
//         cout << "Error opening file: " << disk_file_name_ << endl;
//         return;
//     }

//     // Map the entire file
//     lsm_data* data = (lsm_data*) mmap(NULL, max_file_size, PROT_READ, MAP_SHARED, fd, 0);
//     if (data == MAP_FAILED) {
//         close(fd);
//         cout << "Mmap failed on level " << this->curr_level_ << " 's fp_construct()" << endl; 
//         return;
//     }

//     // dynamically allocate memory for the fence pointers array
//     delete[] fp_array_;
//     fp_array_ = new fence_ptr[num_fence_ptrs_];

//     for (int i = 0; i < num_fence_ptrs_; i++) {
//         uint64_t segment_start_index = i * FENCE_PTR_EVERY_K_ENTRIES;
//         uint64_t segment_end_index = (i + 1) * FENCE_PTR_EVERY_K_ENTRIES - 1;
//         // Adjust for the last segment which might not be full
//         if (segment_end_index >= curr_size_) {
//             segment_end_index = curr_size_ - 1;
//         }

//         uint64_t segment_offset = segment_start_index * LSM_DATA_SIZE;
//         fp_array_[i].min_key = data[segment_start_index].key;
//         fp_array_[i].max_key = data[segment_end_index].key;
//         fp_array_[i].offset = segment_offset;
//     }

//     // Don't forget to unmap and close the file descriptor
//     munmap(data, max_file_size);
//     close(fd);


//     // printing fence ptr contents
//     // for (int i = 0; i < num_fence_ptrs_; ++i) {
//     //     cout << "Fence ptr " << i << endl;
//     //     cout << "min_key: " << fp_array_[i].min_key << endl;
//     //     cout << "max_key: " << fp_array_[i].max_key << endl;
//     //     cout << "offset: " << fp_array_[i].offset << endl;
//     //     cout << endl;
//     // }

// }


// level::bf_fp_construct()
//      This function constructs bloom filters AND fence pointers. This is only called on database startup in case we
//      already have data files that we want to initialize the database with, so we construct bloom filters and fence
//      pointers on startup
void level::bf_fp_construct() {
    // cout << "inside bf_fp_construct() for level " << curr_level_ << endl;
    // when we start up the database and we have files for levels but those files contain no data, we don't want to construct bloom filters or fence pointers,
    // so simply return --> this was also giving me an error since having `parameters.projected_element_count = 0` was not handled correctly in bloom filter library
    if (curr_size_ == 0) {
        // cout << "level " << curr_level_ << " has curr_size_ == 0, so no need to construct anything" << endl;
        
        // remember to set BF and FPs to nullptr just to be safe
        filter_ = nullptr;
        num_fence_ptrs_ = 0;
        fp_array_ = nullptr;
        return;
    }

    // Bloom filter set up
    bloom_parameters parameters;
    parameters.projected_element_count = curr_size_; // How many elements roughly do we expect to insert?
    parameters.false_positive_probability = 0.0001; // Maximum tolerable false positive probability? (0,1) --> 1 in 10000
    parameters.random_seed = 0xA5A5A5A5; // Simple randomizer (optional)
    if (!parameters)
    {
        assert(curr_size_ == 0);
        cout << "Bloom filter error on startup is happening on level " << this->curr_level_ << endl;
        std::cout << "Error - Invalid set of bloom filter parameters!" << std::endl;
        exit(0);
    }
    parameters.compute_optimal_parameters();
    if (filter_) {
        // cout << "inside bf_fp_construct(), going to delete BF for level " << curr_level_ << endl;
        delete filter_; // delete previous bloom filter we may have had
    }
    // delete filter_; // delete previous bloom filter we may have had
    // cout << "inside bf_fp_construct(), going to allocate new BF for level " << curr_level_ << endl;
    filter_ = new bloom_filter(parameters);

    // Open the level's data file
    int fd = open(disk_file_name_.c_str(), O_RDONLY);
    if (fd == -1) {
        cout << "Error opening file: " << disk_file_name_ << endl;
        return;
    }

    // Map the entire file
    lsm_data* data = (lsm_data*) mmap(NULL, max_file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        cout << "Mmap failed on level " << this->curr_level_ << " 's bf_fp_construct()" << endl; 
        return;
    }

    // dynamically allocate memory for the fence pointers array
    if (fp_array_) {
        // cout << "inside bf_fp_construct(), going to delete fp_array for level " << curr_level_ << endl;
        delete[] fp_array_;
    }
    // delete fp_array_;
    // cout << "inside bf_fp_construct(), going to allocate new fp_array for level " << curr_level_ << endl;

    // TODO: this line is prolly wrong because we have no idea how many fence ptrs we will need when restarting database after
    // startup, it is initialized to 0 in lsm.hh. need to set it to curr_size / EVERY_K, and then add 1 if we have any remainders
    num_fence_ptrs_ = curr_size_ / FENCE_PTR_EVERY_K_ENTRIES;
    if (curr_size_ % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        ++num_fence_ptrs_;
    }
    // cout << "Inside bf_fp_construct(), set level " << curr_level_ << "'s num_fence_ptrs_: " << num_fence_ptrs_ << endl;
    fp_array_ = new fence_ptr[num_fence_ptrs_];

    // Loop to construct fence pointers
    for (int i = 0; i < num_fence_ptrs_; i++) {
        uint64_t segment_start_index = i * FENCE_PTR_EVERY_K_ENTRIES;
        uint64_t segment_end_index = (i + 1) * FENCE_PTR_EVERY_K_ENTRIES - 1;
        // Adjust for the last segment which might not be full
        if (segment_end_index >= curr_size_) {
            segment_end_index = curr_size_ - 1;
        }

        uint64_t segment_offset = segment_start_index * LSM_DATA_SIZE;
        fp_array_[i].min_key = data[segment_start_index].key;
        fp_array_[i].max_key = data[segment_end_index].key;
        fp_array_[i].offset = segment_offset;
    }


    // Loop to insert all elements into bloom filter
    for (uint64_t i = 0; i < curr_size_; ++i) {
        filter_->insert(data[i].key);
    }

    // Don't forget to unmap and close the file descriptor
    munmap(data, max_file_size);
    close(fd);
}

// level::merge() --> this function overload strictly merges the buffer's skiplist with level 1 disk data
bool level::merge(uint64_t num_elements_to_merge, int child_level, skipList** buffer_skiplist) {
    assert(child_level == 0);
    lock_guard<mutex> curr_level_lock(this->mutex_);

    if (num_elements_to_merge <= 0) {
        return true;
    }

    // check for a potential cascade of merges
    if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ != MAX_LEVELS) {
        // cout << "Need to cascade merge level " << curr_level_ << " with level " << curr_level_ + 1 << endl;
        next_->merge(curr_size_, curr_level_);

        // reset data for this level as it has been merged with another level already
        curr_size_ = 0;
        // cout << "Bc we cascade merged, Just set level " << this->curr_level_ << "'s curr_size_: " << curr_size_ << endl;
    } else if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ == MAX_LEVELS) {
        cout << "ERROR: CAN'T CASCADE MERGE, DATABASE IS FULL!" << endl;
        exit(0);
        // return false;
    }

    // mmap() current file's contents
    int curr_fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
    if (curr_fd == -1) {
        cout << "Error in opening / creating " << disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }
    lsm_data* new_curr_sstable = (lsm_data*) mmap(0, max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
    if (new_curr_sstable == MAP_FAILED)
    {
        cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(curr_fd);
        exit(0);
    }
    // cout << "mmap()'ed the current level's file!" << endl;


    // create and mmap a temporary file that we will write the new merged contents to. this file will later be renamed the LEVEL#.data file
    int temp_fd = open("data/TEMP.data", O_RDWR | O_CREAT, (mode_t)0600);
    if (temp_fd == -1) {
        cout << "Error in opening / creating TEMP.data file! Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }

    /* Moving the file pointer to the end of the file*/
    int rflag = lseek(temp_fd, max_file_size-1, SEEK_SET);
    
    if(rflag == -1)
    {
        cout << "Lseek failed in creating a temporary file for level " << this->curr_level_ << " in merge()! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(temp_fd);
        exit(0);
    }

    /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
    rflag = write(temp_fd, "", 1);
    if(rflag == -1)
    {
        cout << "Writing empty string failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(temp_fd);
        exit(0);
    }

    lsm_data* new_temp_sstable = (lsm_data*) mmap(0, max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, temp_fd, 0);
    if (new_temp_sstable == MAP_FAILED)
    {
        cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(temp_fd);
        exit(0);
    }
    // cout << "mmap()'ed the temp data file!" << endl;


    // Bloom filter set up
    bloom_parameters parameters;
    parameters.projected_element_count = curr_size_ + num_elements_to_merge; // How many elements roughly do we expect to insert?
    parameters.false_positive_probability = 0.0001; // Maximum tolerable false positive probability? (0,1) --> 1 in 10000
    parameters.random_seed = 0xA5A5A5A5; // Simple randomizer (optional)
    if (!parameters)
    {
        std::cout << "Error - Invalid set of bloom filter parameters!" << std::endl;
        exit(0);
    }
    parameters.compute_optimal_parameters();
    if (filter_) {
        // cout << "inside merge(), going to delete BF for level " << curr_level_ << endl;
        delete filter_; // delete previous bloom filter we may have had
    }
    // delete filter_; // delete previous bloom filter we may have had
    // cout << "inside merge(), going to allocate new BF for level " << curr_level_ << endl;
    filter_ = new bloom_filter(parameters);

    // cout << "Initialized bf and fp's" << endl;

    // dynamically allocate memory for the fence pointers array
    if (this->fp_array_) {
        // cout << "inside merge(), going to delete fp_array for level " << curr_level_ << endl;
        delete[] this->fp_array_;
        this->fp_array_ = nullptr;
    }
    this->num_fence_ptrs_ = (this->curr_size_ + num_elements_to_merge) / FENCE_PTR_EVERY_K_ENTRIES;
    if ((this->curr_size_ + num_elements_to_merge) % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        ++this->num_fence_ptrs_;
    }
    assert(num_fence_ptrs_ > 0);
    // cout << "inside merge(), going to allocate new fp_array for level " << curr_level_ << endl;
    fp_array_ = new fence_ptr[num_fence_ptrs_];


    // ===SORTING LOGIC STARTS NOW====

    // merge 2 sorted arrays into 1 sorted array
    uint64_t my_ptr = 0;
    uint64_t temp_sstable_ptr = 0;

    // get the head node of the top most level
    auto curr_node = (*buffer_skiplist)->head;
    // move curr_node to point to the beginning of the lowest level linked list so we can iterate through all the nodes in sorted order
    while (curr_node->below) {
        curr_node = curr_node->below;
    }
    // because curr_node currently points to the head element, move it to the first item in the skiplist
    curr_node = curr_node->next;
    // DON'T DO THIS: WEIRD BEHAVIOR::::the condition to break out will be if the curr_node is equal to the tail element, bc that means we are at the end 
    uint64_t buffer_index = 0;
    // cout << "About to merge 2 sorted arrays..." << endl;

    // while (my_ptr < curr_size_ && curr_node != (*buffer_skiplist)->tail) {
    while (my_ptr < curr_size_ && buffer_index < num_elements_to_merge) {

        // if both are the same key, and child buffer element is deleted, skip over both
        if (new_curr_sstable[my_ptr].key == curr_node->key && (curr_node->value == DELETED_FLAG)) {
            ++my_ptr;
            ++buffer_index;
            curr_node = curr_node->next;
            continue; // so we don't execute the below conditions
        } 
    
        // cout << "new_curr_sstable[my_ptr].key: " << new_curr_sstable[my_ptr].key << " | new_child_data[child_ptr].key: " << new_child_data[child_ptr].key << endl;

        if (new_curr_sstable[my_ptr].key < curr_node->key) {
            new_temp_sstable[temp_sstable_ptr] = {new_curr_sstable[my_ptr].key, new_curr_sstable[my_ptr].value};
            filter_->insert(new_curr_sstable[my_ptr].key);
            // cout << "(" << new_curr_sstable[my_ptr].key << ", " << new_curr_sstable[my_ptr].value << ") is merged into level " << this->curr_level_ << endl;
            ++my_ptr;
        } else if (new_curr_sstable[my_ptr].key > curr_node->key) {
            new_temp_sstable[temp_sstable_ptr] = {curr_node->key, curr_node->value};
            filter_->insert(curr_node->key);
            // cout << "(" << new_child_data[child_ptr].key << ", " << new_child_data[child_ptr].value << ") is merged into level " << this->curr_level_ << endl;
            curr_node = curr_node->next;
            ++buffer_index;
        } else {
            // else, both keys are equal, so pick the key from the smaller level and skip over the key in the larger level since it is older
            new_temp_sstable[temp_sstable_ptr] = {curr_node->key, curr_node->value};
            filter_->insert(curr_node->key);
            curr_node = curr_node->next;
            ++my_ptr;
            ++buffer_index;

            // cout << "WE SHOULD NOT HAVE 2 KEYS THAT ARE EQUAL IN MY CUSTOM COMMANDS WORKLOAD" << endl;
        }

        // cout << "new_temp_sstable[temp_sstable_ptr].key: " << new_temp_sstable[temp_sstable_ptr].key << endl;


        // if we are at the start of a new segment, begin a new fence ptr
        if (temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES == 0) {
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].min_key = new_temp_sstable[temp_sstable_ptr].key;
            // offset should be how many entries are stored so far * the data size
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].offset = temp_sstable_ptr * LSM_DATA_SIZE;


            // update the max value of the prev fence ptr to the be the element right behind this current element
            if (temp_sstable_ptr > 0) {
                fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
            }
        }

        ++temp_sstable_ptr;
    }

    while (my_ptr < curr_size_ ) {

        // cout << "new_curr_sstable[my_ptr].key: " << new_curr_sstable[my_ptr].key << endl;

        // assert(sstable_[my_ptr].deleted == false);

        auto curr_key = new_curr_sstable[my_ptr].key;
        auto curr_value = new_curr_sstable[my_ptr].value;

        lsm_data struct_to_merge = {curr_key, curr_value};

        new_temp_sstable[temp_sstable_ptr] = struct_to_merge;
        // cout << "new_temp_sstable[temp_sstable_ptr].key: " << new_temp_sstable[temp_sstable_ptr].key << endl;  
        // new_temp_sstable[temp_sstable_ptr] = {new_curr_sstable[my_ptr].key, new_curr_sstable[my_ptr].value, new_curr_sstable[my_ptr].deleted};
        filter_->insert(new_curr_sstable[my_ptr].key);
        // cout << "(" << new_curr_sstable[my_ptr].key << ", " << new_curr_sstable[my_ptr].value << ") is merged into level " << this->curr_level_ << endl;

        // if we are at the start of a new segment, begin a new fence ptr
        if (temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES == 0) {
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].min_key = new_temp_sstable[temp_sstable_ptr].key;
            // offset should be how many entries are stored so far * the data size
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].offset = temp_sstable_ptr * LSM_DATA_SIZE;


            // update the max value of the prev fence ptr to the be the element right behind this current element
            if (temp_sstable_ptr > 0) {
                fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
            }
        }

        ++my_ptr;
        ++temp_sstable_ptr;
    }


    // while (curr_node != (*buffer_skiplist)->tail ) {
    while (buffer_index < num_elements_to_merge ) {
        // cout << "new_child_data[child_ptr].key: " << new_child_data[child_ptr].key << endl;

        // assert(child_data[child_ptr].deleted == false);

        new_temp_sstable[temp_sstable_ptr] = {curr_node->key, curr_node->value};
        // cout << "new_temp_sstable[temp_sstable_ptr].key: " << new_temp_sstable[temp_sstable_ptr].key << endl;  
        filter_->insert(curr_node->key);
        // cout << "(" << new_child_data[child_ptr].key << ", " << new_child_data[child_ptr].value << ") is merged into level " << this->curr_level_ << endl;

        // if we are at the start of a new segment, begin a new fence ptr
        if (temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES == 0) {
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].min_key = new_temp_sstable[temp_sstable_ptr].key;
            // offset should be how many entries are stored so far * the data size
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].offset = temp_sstable_ptr * LSM_DATA_SIZE;


            // update the max value of the prev fence ptr to the be the element right behind this current element
            if (temp_sstable_ptr > 0) {
                fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
            }
        }
        curr_node = curr_node->next;
        ++temp_sstable_ptr;
        ++buffer_index;
    }

    curr_size_ = temp_sstable_ptr;

    // update this->num_fence_ptrs to be based on curr_size --> we originally may have allocated more fence ptrs than we needed
    // because of duplicates and stale deleted entries, so we want to update this to correctly represent how many fence ptrs we 
    // actually will use to navigate through this level
    this->num_fence_ptrs_ = this->curr_size_ / FENCE_PTR_EVERY_K_ENTRIES;
    if (this->curr_size_ % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        ++this->num_fence_ptrs_;
    }

    // update the max key for the current fence ptr we are on after merging has finished using the last element that was stored on disk
    // make sure to round up temp_sstable_ptr to the nearest multiple of FENCE_PTR_EVERY_K_ENTRIES --> otherwise we access incorrect index (work out case where temp_sstable_ptr = 1 after we have merged just 1 element in total)
    if (this->num_fence_ptrs_ > 0) {
        fp_array_[this->num_fence_ptrs_ - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
    }


    // write curr_size to metadata file
    metadata_file_ptr[curr_level_] = curr_size_;
    if (curr_level_ > 1) {
        metadata_file_ptr[curr_level_ - 1] = 0;
    }
    rflag = msync(metadata_file_ptr, (MAX_LEVELS * sizeof(int)), MS_SYNC);
    if(rflag == -1)
    {
        printf("Unable to msync to metadata file.\n");
        exit(0);
    }
    // fsync the file descriptor to ensure data is written to disk
    // if (fsync(metadata_file_descriptor) == -1) {
    //     perror("fsync error");
    //     // Handle error
    // }


    // MSYNC temp data file and munmap temp file
    if (new_temp_sstable != NULL && temp_fd != -1)
    {
        /*Syncing the contents of the memory with file, flushing the pages to disk*/

        rflag = msync(new_temp_sstable, max_file_size, MS_SYNC);
        if(rflag == -1)
        {
            printf("Unable to msync.\n");
        }
        // fsync the file descriptor to ensure data is written to disk
        // if (fsync(temp_fd) == -1) {
        //     perror("fsync error");
        //     // Handle error
        // }
        rflag = munmap(new_temp_sstable, max_file_size);
        if(rflag == -1)
        {
            printf("Unable to munmap.\n");
        }
        close(temp_fd);
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
        cout << "Error in deleting " << disk_file_name_ << ", Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }

    if (rename("data/TEMP.data", disk_file_name_.c_str()) == 0) {
        // cout << "Successfully renamed temp file to " << disk_file_name_ << endl;
    } else {
        cout << "Error in renaming temp file" << " , Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }

    struct stat file_exists;
    while (stat (disk_file_name_.c_str(), &file_exists) != 0) {
        // busy wait to ensure that renaming of temp file to disk file is done
    }


    // calculate number of fence pointers we need
    num_fence_ptrs_ = curr_size_ / FENCE_PTR_EVERY_K_ENTRIES;
    if (curr_size_ % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        num_fence_ptrs_ += 1;
    }
    
    return true;
}
    

// level::merge() --> this function overload strictly merges 2 levels's disk files together
bool level::merge(uint64_t num_elements_to_merge, int child_level) {
    assert(child_level > 0); // bc the buffer will never call this function overload
    // cout << endl;
    // cout << endl;
    // cout << "====== INSIDE NEW MERGE! Need to merge level " << curr_level_ - 1 << " with level " << curr_level_ << endl;
    lock_guard<mutex> curr_level_lock(this->mutex_);

    if (num_elements_to_merge <= 0) {
        return true;
    }

    // check for a potential cascade of merges
    if (capacity_ - curr_size_ < num_elements_to_merge && curr_level_ != MAX_LEVELS) {
        // cout << "Need to cascade merge level " << curr_level_ << " with level " << curr_level_ + 1 << endl;
        next_->merge(curr_size_, curr_level_);

        // reset data for this level as it has been merged with another level already
        curr_size_ = 0;
        // cout << "Bc we cascade merged, Just set level " << this->curr_level_ << "'s curr_size_: " << curr_size_ << endl;

        // int curr_fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        // if (curr_fd == -1) {
        //     cout << "Error in opening / creating " << disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
        //     exit(0);
        // }
        
        // lsm_data* curr_file_ptr = (lsm_data*) mmap(0,max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
        // if (curr_file_ptr == MAP_FAILED)
        // {
        //     cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        //     close(curr_fd);
        //     exit(0);
        // }

        // memset(curr_file_ptr, 0, max_file_size); --> recall that we don't need to do this as long as we correctly maintain the curr_size value
        // int rflag = msync(curr_file_ptr, max_file_size, MS_SYNC);
        // if(rflag == -1) {
        //     printf("Unable to msync.\n");
        //     exit(0);
        // }

        // // fsync the file descriptor to ensure data is written to disk
        // if (fsync(curr_fd) == -1) {
        //     perror("fsync error");
        //     exit(0);
        //     // Handle error
        // }
        // int rflag = munmap(curr_file_ptr, max_file_size);
        // if(rflag == -1)
        // {
        //     printf("Unable to munmap.\n");
        // }
        // close(curr_fd);
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
    // cout << "child is a level, mmap()'ing!" << endl;
    // new_child_data = mmap the level's disk file
    // child_fd = open(levels_[child_level]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
    child_fd = open(levels_[child_level]->disk_file_name_.c_str(), O_RDONLY);
    if (child_fd == -1) {
        cout << "Error in opening / creating " << levels_[child_level]->disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }
    // new_child_data = (lsm_data*) mmap(0, levels_[child_level]->max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, child_fd, 0);
    new_child_data = (lsm_data*) mmap(0, levels_[child_level]->max_file_size, PROT_READ, MAP_SHARED, child_fd, 0);

    if (new_child_data == MAP_FAILED)
    {
        cout << "mmap() on the child's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(child_fd);
        exit(0);
    }

    // mmap() current file's contents
    int curr_fd = open(disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
    if (curr_fd == -1) {
        cout << "Error in opening / creating " << disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }
    lsm_data* new_curr_sstable = (lsm_data*) mmap(0, max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
    if (new_curr_sstable == MAP_FAILED)
    {
        cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(child_fd);
        exit(0);
    }
    // cout << "mmap()'ed the current level's file!" << endl;


    // create and mmap a temporary file that we will write the new merged contents to. this file will later be renamed the LEVEL#.data file
    int temp_fd = open("data/TEMP.data", O_RDWR | O_CREAT, (mode_t)0600);
    if (temp_fd == -1) {
        cout << "Error in opening / creating TEMP.data file! Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }

    /* Moving the file pointer to the end of the file*/
    int rflag = lseek(temp_fd, max_file_size-1, SEEK_SET);
    
    if(rflag == -1)
    {
        cout << "Lseek failed in creating a temporary file for level " << this->curr_level_ << " in merge()! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(temp_fd);
        exit(0);
    }

    /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
    rflag = write(temp_fd, "", 1);
    if(rflag == -1)
    {
        cout << "Writing empty string failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(temp_fd);
        exit(0);
    }

    lsm_data* new_temp_sstable = (lsm_data*) mmap(0, max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, temp_fd, 0);
    if (new_temp_sstable == MAP_FAILED)
    {
        cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
        close(temp_fd);
        exit(0);
    }
    // cout << "mmap()'ed the temp data file!" << endl;


    // Bloom filter set up
    bloom_parameters parameters;
    parameters.projected_element_count = curr_size_ + num_elements_to_merge; // How many elements roughly do we expect to insert?
    parameters.false_positive_probability = 0.0001; // Maximum tolerable false positive probability? (0,1) --> 1 in 10000
    parameters.random_seed = 0xA5A5A5A5; // Simple randomizer (optional)
    if (!parameters)
    {
        std::cout << "Error - Invalid set of bloom filter parameters!" << std::endl;
        exit(0);
    }
    parameters.compute_optimal_parameters();
    if (filter_) {
        // cout << "inside merge(), going to delete BF for level " << curr_level_ << endl;
        delete filter_; // delete previous bloom filter we may have had
    }
    // delete filter_; // delete previous bloom filter we may have had
    // cout << "inside merge(), going to allocate new BF for level " << curr_level_ << endl;
    filter_ = new bloom_filter(parameters);

    // cout << "Initialized bf and fp's" << endl;

    // merge 2 sorted arrays into 1 sorted array
    uint64_t my_ptr = 0;
    uint64_t child_ptr = 0;

    uint64_t temp_sstable_ptr = 0;

    // cout << "About to merge 2 sorted arrays..." << endl;

    set<int> deleted_keys;

    // dynamically allocate memory for the fence pointers array
    if (this->fp_array_) {
        // cout << "inside merge(), going to delete fp_array for level " << curr_level_ << endl;
        delete[] this->fp_array_;
        this->fp_array_ = nullptr;
    }
    this->num_fence_ptrs_ = (this->curr_size_ + num_elements_to_merge) / FENCE_PTR_EVERY_K_ENTRIES;
    if ((this->curr_size_ + num_elements_to_merge) % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        ++this->num_fence_ptrs_;
    }
    assert(num_fence_ptrs_ > 0);
    // cout << "inside merge(), going to allocate new fp_array for level " << curr_level_ << endl;
    fp_array_ = new fence_ptr[num_fence_ptrs_];

    while (my_ptr < curr_size_ && child_ptr < num_elements_to_merge) {

        // assert(new_child_data[child_ptr].key == child_data[child_ptr].key && new_child_data[child_ptr].value == child_data[child_ptr].value && new_child_data[child_ptr].deleted == child_data[child_ptr].deleted);
        // assert(new_curr_sstable[my_ptr].key == sstable_[my_ptr].key && new_curr_sstable[my_ptr].value == sstable_[my_ptr].value && new_curr_sstable[my_ptr].deleted == sstable_[my_ptr].deleted);
        
        // // if child's current key is deleted, add to a deleted set and move to next child entry
        // if (new_child_data[child_ptr].value == DELETED_FLAG) {
        //     deleted_keys.insert(new_child_data[child_ptr].key);
        //     ++child_ptr;
        //     continue;
        // }

        // if (deleted_keys.find(new_curr_sstable[my_ptr].key) != deleted_keys.end()) {
        //     deleted_keys.erase(new_curr_sstable[my_ptr].key);
        //     ++my_ptr;
        //     continue;
        // }

        // if both are the same key, and either one is deleted, skip over both
        if (new_curr_sstable[my_ptr].key == new_child_data[child_ptr].key && (new_child_data[child_ptr].value == DELETED_FLAG)) {
            ++my_ptr;
            ++child_ptr;
            continue; // so we don't execute the below conditions
        } 

        // // if currently at a deleted key at either list, skip over it
        // // --> this case is when the lists have unique keys but they have been deleted themselves already, so we should not merge them
        // if (new_curr_sstable[my_ptr].value == DELETED_FLAG || deleted_keys.find(new_curr_sstable[my_ptr].key) != deleted_keys.end()) {
        //     deleted_keys.erase(new_curr_sstable[my_ptr].key);
        //     ++my_ptr;
        //     continue;
        // }
        
        

        // cout << "new_curr_sstable[my_ptr].key: " << new_curr_sstable[my_ptr].key << " | new_child_data[child_ptr].key: " << new_child_data[child_ptr].key << endl;

        if (new_curr_sstable[my_ptr].key < new_child_data[child_ptr].key) {
            new_temp_sstable[temp_sstable_ptr] = {new_curr_sstable[my_ptr].key, new_curr_sstable[my_ptr].value};
            filter_->insert(new_curr_sstable[my_ptr].key);
            // cout << "(" << new_curr_sstable[my_ptr].key << ", " << new_curr_sstable[my_ptr].value << ") is merged into level " << this->curr_level_ << endl;
            ++my_ptr;
        } else if (new_curr_sstable[my_ptr].key > new_child_data[child_ptr].key) {
            new_temp_sstable[temp_sstable_ptr] = {new_child_data[child_ptr].key, new_child_data[child_ptr].value};
            filter_->insert(new_child_data[child_ptr].key);
            // cout << "(" << new_child_data[child_ptr].key << ", " << new_child_data[child_ptr].value << ") is merged into level " << this->curr_level_ << endl;
            ++child_ptr;
        } else {
            // else, both keys are equal, so pick the key from the smaller level and skip over the key in the larger level since it is older
            new_temp_sstable[temp_sstable_ptr] = {new_child_data[child_ptr].key, new_child_data[child_ptr].value};
            filter_->insert(new_child_data[child_ptr].key);
            ++child_ptr;
            ++my_ptr;

            // cout << "WE SHOULD NOT HAVE 2 KEYS THAT ARE EQUAL IN MY CUSTOM COMMANDS WORKLOAD" << endl;
        }

        // cout << "new_temp_sstable[temp_sstable_ptr].key: " << new_temp_sstable[temp_sstable_ptr].key << endl;


        // if we are at the start of a new segment, begin a new fence ptr
        if (temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES == 0) {
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].min_key = new_temp_sstable[temp_sstable_ptr].key;
            // offset should be how many entries are stored so far * the data size
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].offset = temp_sstable_ptr * LSM_DATA_SIZE;


            // update the max value of the prev fence ptr to the be the element right behind this current element
            if (temp_sstable_ptr > 0) {
                fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
            }
        }

        ++temp_sstable_ptr;
    }

    // cout << "Finished first merging loop" << endl;

    // if (my_ptr < curr_size_) {
    //     cout << "Copying leftover elements from current level data over to new merged data..." << endl;
    // }

    while (my_ptr < curr_size_ ) {
        // skip over deleted elements here
        // assert(new_curr_sstable[my_ptr].key == sstable_[my_ptr].key && new_curr_sstable[my_ptr].value == sstable_[my_ptr].value && new_curr_sstable[my_ptr].deleted == sstable_[my_ptr].deleted);
        // if (new_curr_sstable[my_ptr].value == DELETED_FLAG || deleted_keys.find(new_curr_sstable[my_ptr].key) != deleted_keys.end()) {
        //     deleted_keys.erase(new_curr_sstable[my_ptr].key);
        //     ++my_ptr;
        //     continue;
        // }

        // cout << "new_curr_sstable[my_ptr].key: " << new_curr_sstable[my_ptr].key << endl;

        // assert(sstable_[my_ptr].deleted == false);

        auto curr_key = new_curr_sstable[my_ptr].key;
        auto curr_value = new_curr_sstable[my_ptr].value;

        lsm_data struct_to_merge = {curr_key, curr_value};

        new_temp_sstable[temp_sstable_ptr] = struct_to_merge;
        // cout << "new_temp_sstable[temp_sstable_ptr].key: " << new_temp_sstable[temp_sstable_ptr].key << endl;  
        // new_temp_sstable[temp_sstable_ptr] = {new_curr_sstable[my_ptr].key, new_curr_sstable[my_ptr].value, new_curr_sstable[my_ptr].deleted};
        filter_->insert(new_curr_sstable[my_ptr].key);
        // cout << "(" << new_curr_sstable[my_ptr].key << ", " << new_curr_sstable[my_ptr].value << ") is merged into level " << this->curr_level_ << endl;

        // if we are at the start of a new segment, begin a new fence ptr
        if (temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES == 0) {
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].min_key = new_temp_sstable[temp_sstable_ptr].key;
            // offset should be how many entries are stored so far * the data size
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].offset = temp_sstable_ptr * LSM_DATA_SIZE;


            // update the max value of the prev fence ptr to the be the element right behind this current element
            if (temp_sstable_ptr > 0) {
                fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
            }
        }

        ++my_ptr;
        ++temp_sstable_ptr;
    }

    // cout << "Finished second merging loop" << endl;

    // if (child_ptr < num_elements_to_merge) {
    //     cout << "Copying leftover elements from child data over to new merged data..." << endl;
    // }

    while (child_ptr < num_elements_to_merge ) {
        // skip over deleted elements here
        // assert(new_child_data[child_ptr].key == child_data[child_ptr].key && new_child_data[child_ptr].value == child_data[child_ptr].value && new_child_data[child_ptr].deleted == child_data[child_ptr].deleted);
        // if (new_child_data[child_ptr].value == DELETED_FLAG) {
        //     cout << "WE SHOULD NOT BE HERE" << endl;
        //     ++child_ptr;
        //     continue;
        // }

        // cout << "new_child_data[child_ptr].key: " << new_child_data[child_ptr].key << endl;

        // assert(child_data[child_ptr].deleted == false);

        new_temp_sstable[temp_sstable_ptr] = {new_child_data[child_ptr].key, new_child_data[child_ptr].value};
        // cout << "new_temp_sstable[temp_sstable_ptr].key: " << new_temp_sstable[temp_sstable_ptr].key << endl;  
        filter_->insert(new_child_data[child_ptr].key);
        // cout << "(" << new_child_data[child_ptr].key << ", " << new_child_data[child_ptr].value << ") is merged into level " << this->curr_level_ << endl;

        // if we are at the start of a new segment, begin a new fence ptr
        if (temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES == 0) {
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].min_key = new_temp_sstable[temp_sstable_ptr].key;
            // offset should be how many entries are stored so far * the data size
            fp_array_[temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES].offset = temp_sstable_ptr * LSM_DATA_SIZE;


            // update the max value of the prev fence ptr to the be the element right behind this current element
            if (temp_sstable_ptr > 0) {
                fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
            }
        }
        ++child_ptr;
        ++temp_sstable_ptr;
    }

    curr_size_ = temp_sstable_ptr;
    // cout << "Just set level " << this->curr_level_ << "'s curr_size_: " << curr_size_ << endl;


    // update this->num_fence_ptrs to be based on curr_size --> we originally may have allocated more fence ptrs than we needed
    // because of duplicates and stale deleted entries, so we want to update this to correctly represent how many fence ptrs we 
    // actually will use to navigate through this level
    this->num_fence_ptrs_ = this->curr_size_ / FENCE_PTR_EVERY_K_ENTRIES;
    if (this->curr_size_ % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        ++this->num_fence_ptrs_;
    }

    // update the max key for the current fence ptr we are on after merging has finished using the last element that was stored on disk
    // make sure to round up temp_sstable_ptr to the nearest multiple of FENCE_PTR_EVERY_K_ENTRIES --> otherwise we access incorrect index (work out case where temp_sstable_ptr = 1 after we have merged just 1 element in total)
    if (this->num_fence_ptrs_ > 0) {
        fp_array_[this->num_fence_ptrs_ - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
    }
    // if (temp_sstable_ptr > 0) {
    //     int nearest_multiple_of_fence_ptr_every_k = temp_sstable_ptr;
    //     nearest_multiple_of_fence_ptr_every_k += temp_sstable_ptr % FENCE_PTR_EVERY_K_ENTRIES;
    //     int last_fence_ptr_index = nearest_multiple_of_fence_ptr_every_k / FENCE_PTR_EVERY_K_ENTRIES;
    //     cout << "last_fence_ptr_index: " << last_fence_ptr_index << " | num_fence_ptrs_ : " << num_fence_ptrs_ <<  endl;
    //     assert(last_fence_ptr_index == this->num_fence_ptrs_);
    //     fp_array_[last_fence_ptr_index].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
    //     // fp_array_[(temp_sstable_ptr / FENCE_PTR_EVERY_K_ENTRIES) - 1].max_key = new_temp_sstable[temp_sstable_ptr - 1].key;
    // }

    // cout << "Finished third merging loop" << endl;

    // // THIS IS FOR THE COMMANDS AND NEW_COMMANDS WORKLOADS ONLY SINCE THERE ARE NO DELETIONS OR OVERWRITES:
    // if(curr_size_ + num_elements_to_merge != temp_sstable_ptr) {
    //     cout << endl;
    //     cout << "Old current size of level " << this->curr_level_ << ": " << curr_size_ << " | num_elements_to_merge: " << num_elements_to_merge << endl;
    //     cout << "temp_sstable_ptr: " << temp_sstable_ptr << endl;
    // }
    // if (child_level >= 1) {
    //     assert(levels_[child_level]->curr_size_ == num_elements_to_merge);
    // }
    // assert(curr_size_ + num_elements_to_merge == temp_sstable_ptr);


    
    // cout << "Level " << this->curr_level_ << " has " << this->num_fence_ptrs_ << " fence ptrs" << endl;
    // // printing fence ptr contents
    // for (int i = 0; i < this->num_fence_ptrs_; ++i) {
    //     cout << "Fence ptr " << i << endl;
    //     cout << "min_key: " << fp_array_[i].min_key << endl;
    //     cout << "max_key: " << fp_array_[i].max_key << endl;
    //     cout << "offset: " << fp_array_[i].offset << endl;
    //     cout << endl;
    // }

    // write curr_size to metadata file
    metadata_file_ptr[curr_level_] = curr_size_;
    if (curr_level_ > 1) {
        metadata_file_ptr[curr_level_ - 1] = 0;
    }
    rflag = msync(metadata_file_ptr, (MAX_LEVELS * sizeof(int)), MS_SYNC);
    if(rflag == -1)
    {
        printf("Unable to msync to metadata file.\n");
        exit(0);
    }
    // fsync the file descriptor to ensure data is written to disk
    // if (fsync(metadata_file_descriptor) == -1) {
    //     perror("fsync error");
    //     // Handle error
    // }


    // MSYNC temp data file and munmap temp file
    if (new_temp_sstable != NULL && temp_fd != -1)
    {
        /*Syncing the contents of the memory with file, flushing the pages to disk*/

        rflag = msync(new_temp_sstable, max_file_size, MS_SYNC);
        if(rflag == -1)
        {
            printf("Unable to msync.\n");
        }
        // fsync the file descriptor to ensure data is written to disk
        // if (fsync(temp_fd) == -1) {
        //     perror("fsync error");
        //     // Handle error
        // }
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
        cout << "Error in deleting " << disk_file_name_ << ", Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }

    if (rename("data/TEMP.data", disk_file_name_.c_str()) == 0) {
        // cout << "Successfully renamed temp file to " << disk_file_name_ << endl;
    } else {
        cout << "Error in renaming temp file" << " , Error message: " << strerror(errno) << " | Exiting program" << endl;
        exit(0);
    }

    struct stat file_exists;
    while (stat (disk_file_name_.c_str(), &file_exists) != 0) {
        // busy wait to ensure that renaming of temp file to disk file is done
    }


    // calculate number of fence pointers we need
    num_fence_ptrs_ = curr_size_ / FENCE_PTR_EVERY_K_ENTRIES;
    if (curr_size_ % FENCE_PTR_EVERY_K_ENTRIES != 0) {
        num_fence_ptrs_ += 1;
    }
    // cout << "A merge just happened in level " << curr_level_ << ", there are num_fence_ptrs_: " << num_fence_ptrs_ << endl;

    // fp_construct();


    // remember to reset child level's bloom filter and fence ptr array (if child is an actual level and not the buffer)
    if (child_level != 0) {
        // cout << "inside merge(), going to delete BF for child level " << child_level << endl;
        delete levels_[child_level]->filter_;
        levels_[child_level]->filter_ = nullptr;
        // cout << "inside merge(), going to delete fp_array for child level " << child_level << endl;
        delete[] levels_[child_level]->fp_array_;
        levels_[child_level]->fp_array_ = nullptr;
        levels_[child_level]->num_fence_ptrs_ = 0;
    }

    return true;
}


// implementation of buffer class functions

// buffer::buffer() constructor
//      Just sets the ptr of the buffer's data to fresh place in memory, ready to store data
buffer::buffer() {
    lock_guard<mutex> buffer_lock(this->mutex_);
    buffer_skiplist = new skipList(BUFFER_CAPACITY);
}
    

// buffer::insert(lsm_data kv_pair)
//      The function inserts a key value pair to the buffer's data, which merges with the levels on disk if the buffer's
//      capacity is reached
bool buffer::insert(lsm_data kv_pair) {
    // first search through buffer, and if it exists, just update the key value struct directly to be the new value
    // --> since simply adding a new entry to the buffer for updating might not cause the old entry to be deleted when merging later on since original comes before the new entry
    // do linear search on the buffer (since buffer isn't sorted)

    // GET LOCK ON BUFFER HERE
    lock_guard<mutex> buffer_lock(this->mutex_);

    // first check if key already exists in buffer. if so, just update its value directly
    auto node = buffer_skiplist->skipSearch(kv_pair.key);
    if (node->key == kv_pair.key) {
        cout << "DUPLICATE ENTRY FOUND FOR: (" << node->key << ", " << node->value << "), UPDATING VALUE TO BE " << kv_pair.value << endl;
        node->value = kv_pair.value;
        return true;
    }

    // if buffer is at capacity already, merge buffer data with level 1 data before putting this into the buffer
    if (curr_size_ == capacity_) {

        // Merge the skiplist into the LSM tree
        level1ptr_->merge(curr_size_, 0, &buffer_skiplist);
        
        curr_size_ = 0;
        delete buffer_skiplist;
        buffer_skiplist = new skipList(BUFFER_CAPACITY);
    }

    // insert into skip list
    buffer_skiplist->skipInsert(kv_pair.key, kv_pair.value);
    ++curr_size_;
    // cout << "Inserted (" << kv_pair.key << ", " << kv_pair.value << ")! curr_size is now " << curr_size_ << endl;
    return true;
}

// buffer::flush()
//      This function is called at the lsm_tree object's flush() function. It causes the buffer to merge it's data with
//      the data at Level 1, using the level's merge() function, which triggers a cascade merge if necessary
void buffer::flush() {
    // GET LOCK ON BUFFER HERE
    lock_guard<mutex> buffer_lock(this->mutex_);

    // Merge the skiplist into the LSM tree
    level1ptr_->merge(curr_size_, 0, &buffer_skiplist);
    
    curr_size_ = 0;
    delete buffer_skiplist;
    buffer_skiplist = new skipList(BUFFER_CAPACITY);
}


// implementations for functions of the lsm_tree class start here

lsm_tree::lsm_tree() {
    // cout << "SIZE RATIO: " << SIZE_RATIO << endl;
    // NEW DISK STORAGE IMPLEMENTATION CODE
    // if `level_metadata.data` file doesn't exist, create it and memset() it to all 0's to represent curr_size is 0 for all levels when we have no data yet
    struct stat metadata_file_exists;
    if (stat (metadata_filename, &metadata_file_exists) != 0) {
        // cout << "global metadata file DNE, so making a new one and starting database from fresh!" << endl;
    
        int fd = open(metadata_filename, O_RDWR | O_CREAT, (mode_t)0600);
        if (fd == -1) {
            cout << "Error in opening / creating global `level_metadata` file! Error message: " << strerror(errno) << " | Exiting program" << endl;
            exit(0);
        }

        /* Moving the file pointer to the end of the file*/
        int rflag = lseek(fd, (MAX_LEVELS * sizeof(int))-1, SEEK_SET);
        
        if(rflag == -1)
        {
            cout << "Lseek failed when seeking in metadata file! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(fd);
            exit(0);
        }

        /*Writing an empty string to the end of the file so that file is actually created and space is reserved on the disk*/
        rflag = write(fd, "", 1);
        if(rflag == -1)
        {
            cout << "Writing empty string failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(fd);
            exit(0);
        }

        // memory map the metadata file contents into process memory so we can access it when inside any level::function()
        metadata_file_ptr = (int*) mmap(0,(MAX_LEVELS * sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (metadata_file_ptr == MAP_FAILED)
        {
            cout << "mmap() on the metadata file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(fd);
            exit(0);
        }

        memset(metadata_file_ptr, 0, metadata_size);
        rflag = msync(metadata_file_ptr, metadata_size, MS_SYNC);
        if (rflag == -1) {
            cout << "unable to msync metadata file" << endl;
            exit(0);
        }
        // fsync the file descriptor to ensure data is written to disk
        // if (fsync(metadata_file_descriptor) == -1) {
        //     perror("fsync error");
        //     // Handle error
        // }

        metadata_file_descriptor = fd;
    }
    // if file already exists, just mmap it
    else {
        int fd = open(metadata_filename, O_RDWR | O_CREAT, (mode_t)0600);
        if (fd == -1) {
            cout << "Error in opening / creating global `level_metadata` file! Error message: " << strerror(errno) << " | Exiting program" << endl;
            exit(0);
        }

        metadata_file_ptr = (int*) mmap(0,(MAX_LEVELS * sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (metadata_file_ptr == MAP_FAILED)
        {
            cout << "mmap() on the metadata file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(fd);
            exit(0);
        }

        metadata_file_descriptor = fd;
    }

    // END NEW DISK STORAGE IMPLEMENTATION CODE

    buffer_ptr_ = new buffer();

    // malloc a new level object and assign level1ptr_ to point to that object
    buffer_ptr_->level1ptr_ = new level(INITIAL_LEVEL_CAPACITY, 1);
    levels_[1] = buffer_ptr_->level1ptr_; // ptr to level 1 is stored in index 1
    
    auto curr_level_ptr = levels_[1];
    for (int i = 2; i <= MAX_LEVELS; ++i) {
        // cout << endl;
        // cout << "LEVEL " << i - 1 << " has capacity: " << curr_level_ptr->capacity_ << endl;
        // cout << "SIZE RATIO: " << SIZE_RATIO << endl;
        // cout << "LEVEL " << i << " can store " << curr_level_ptr->capacity_ * SIZE_RATIO << " entries total" << endl;
        levels_[i] = new level(curr_level_ptr->capacity_ * SIZE_RATIO, i);
        curr_level_ptr->next_ = levels_[i];
        curr_level_ptr = levels_[i];
    }
}


// lsm_tree::insert()
//      This function inserts a key value pair into the database by directly calling the buffer's insert() method. All
//      insertions to the database occur at this lsm_tree object first
bool lsm_tree::insert(lsm_data kv_pair) {
    buffer_ptr_->insert(kv_pair);
    return true;
}

// lsm_tree::flush_buffer()
//      This function forcefully merges the data currently in the in-memory buffer to level 1's data on disk,
//      effectively causing all data to live on disk. This is useful when the database is about to be shutdown
//      so all data is on disk and nothing will be wiped when the program exits
void lsm_tree::flush_buffer() {
    buffer_ptr_->flush();
}

// lsm_tree::merge_level()
//      This function forcefully merges the data in level i with the data in level i + 1. No changes occur if
//      the input i is outside the range of levels 1 to MAX_LEVELS - 1 because the largest level has no level to
//      merge its data with
void lsm_tree::merge_level(int i) {
    if (i < 1 || i >= MAX_LEVELS) {
        return;
    }
    
    auto curr_level_ptr = levels_[i];
    lock_guard<mutex> buffer_lock(curr_level_ptr->mutex_);
    levels_[i+1]->merge(curr_level_ptr->curr_size_, i);

    // reset data for this level as it has been merged with another level already
    curr_level_ptr->curr_size_ = 0;
    // cout << "Bc we cascade merged, Just set level " << this->curr_level_ << "'s curr_size_: " << curr_size_ << endl;

    // No need for the code below since all we need to do is set curr_size_ = 0, we no longer memset() and such

    // int curr_fd = open(curr_level_ptr->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
    // if (curr_fd == -1) {
    //     cout << "Error in opening / creating " << curr_level_ptr->disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
    //     exit(0);
    // }
    
    // lsm_data* curr_file_ptr = (lsm_data*) mmap(0,curr_level_ptr->max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
    // if (curr_file_ptr == MAP_FAILED)
    // {
    //     cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
    //     close(curr_fd);
    //     exit(0);
    // }

    // // memset(curr_file_ptr, 0, max_file_size); // --> recall that we don't need to do this as long as we correctly maintain the curr_size value
    // int rflag = msync(curr_file_ptr, curr_level_ptr->max_file_size, MS_SYNC);
    // if(rflag == -1) {
    //     printf("Unable to msync.\n");
    //     exit(0);
    // }

    // // fsync the file descriptor to ensure data is written to disk
    // // if (fsync(curr_fd) == -1) {
    // //     perror("fsync error");
    // //     exit(0);
    // //     // Handle error
    // // }
    // rflag = munmap(curr_file_ptr, curr_level_ptr->max_file_size);
    // if(rflag == -1)
    // {
    //     printf("Unable to munmap.\n");
    // }
    // close(curr_fd);
}

    
// lsm_tree::get()
//      This function searches the LSM tree for a specified key, starting its search from smaller levels to larger levels
//      to ensure that the most recent data is searched through first. Once the key is found, we immediately return
//      After initially searching through the buffer naively, we then query each level's bloom filters and fence ptrs
//      to optimize our search by only doing 1 read on disk.
string lsm_tree::get(int key) {
    stringstream ss;
    // do linear search on the buffer (since buffer isn't sorted)
    // cout << "[get()] targetkey: " << key << endl;

    // GET LOCK ON BUFFER HERE, in a code snippet with separate scope to make sure the buffer's lock is not
    // held when we search through each level's data
    {
        lock_guard<mutex> buffer_lock(buffer_ptr_->mutex_);

        auto node = buffer_ptr_->buffer_skiplist->skipSearch(key);
        if (node->key == key) {
            if (node->value == DELETED_FLAG) {
                // cout << "(" << key << ", " << node->value << ") was DELETED so NOT FOUND!" << endl;
                // cout << endl;
                ss << endl;
                return ss.str();
            }

            // cout << "(" << key << ", " << node->value << ") was found at buffer!" << endl;
            // cout << node->value << endl;
            ss << node->value << endl;
            return ss.str();
        }
    }

    // cout << "not found in buffer" << endl;
    


    // if not found in buffer, then search across each LEVEL#.data file
    // int result;
    for (int i = 1; i <= MAX_LEVELS; ++i) {
        auto curr_level_ptr = levels_[i];
        lock_guard<mutex> curr_level_lock(curr_level_ptr->mutex_); // GET LOCK ON CURR LEVEL HERE
        auto curr_bloom_filter = levels_[i]->filter_;
        // bloom filter might not have been created yet, so check if it's nullptr as well as if the key is not in the bloom filter
        if (!curr_bloom_filter || !curr_bloom_filter->contains(key)) {
            // cout << "Level " << i << "BF == nullptr OR BF does not contain " << key << endl;
            continue;
        }
        // cout << "Level " << i << "'s bloom filter contains " << key << endl;

        // binary search on fence pointers to find which fence pointer, if any, would contain the target key
        int low = 0;
        int high = curr_level_ptr->num_fence_ptrs_ - 1;
        bool avoid_infinite_loop = false;
        while (low <= high ) {
            int mid = low + (high - low) / 2;
            // if (key == -901585839) {
            //     cout << "low: " << low << "| high: " << high << " | mid: " << mid << endl;
            // }
            if (key < curr_level_ptr->fp_array_[mid].min_key) {
                high = mid - 1;
            } else if (key > curr_level_ptr->fp_array_[mid].max_key) {
                low = mid + 1;
            } else {
                if (avoid_infinite_loop) {
                    // cout << "AVOIDING INFINITE LOOP" << endl;
                    break;
                }
                // int curr_fd = open(levels_[i]->disk_file_name_.c_str(), O_RDWR, (mode_t)0600);
                errno = 0;
                struct stat file_exists;
                while (stat (levels_[i]->disk_file_name_.c_str(), &file_exists) != 0) {
                    // busy wait to ensure that the file exists
                }
                int curr_fd = open(levels_[i]->disk_file_name_.c_str(), O_RDONLY);

                if (curr_fd < 0) {
                    cout << "Error in opening / creating " << levels_[i]->disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
                    exit(0);
                }

                lsm_data segment_buffer[FENCE_PTR_EVERY_K_ENTRIES];

                // Determine the number of entries to read for this segment
                int entriesToRead = FENCE_PTR_EVERY_K_ENTRIES;
                // If this is the last segment, adjust the number of entries to read
                if (mid == curr_level_ptr->num_fence_ptrs_ - 1) {
                    int remainingEntries = curr_level_ptr->curr_size_ - (mid * FENCE_PTR_EVERY_K_ENTRIES);
                    entriesToRead = remainingEntries;
                }

                ssize_t bytesToRead = entriesToRead * sizeof(lsm_data);
                ssize_t bytesRead = pread(curr_fd, segment_buffer, bytesToRead, curr_level_ptr->fp_array_[mid].offset);
                if (bytesRead < 0) {
                    cout << "Error reading from file" << endl;
                    exit(EXIT_FAILURE);
                }

                if (bytesRead % sizeof(lsm_data) != 0) {
                    cout << "Did not read a full data entry in pread()!" << endl;
                    exit(EXIT_FAILURE);
                }

                int num_entries_in_segment = bytesRead / sizeof(lsm_data);
                // cout << "Read " << num_entries_in_segment << " in curr segment" << endl;

                // using binary search instead of naive linear search
                int left = 0;
                int right = num_entries_in_segment - 1;
                while (left <= right) {
                    int midpoint = (left + right) / 2;
                    // if (key == -901585839) {
                    //     cout << "left: " << left << "| right: " << right << " | midpoint: " << midpoint << endl;
                    // }
                    // cout << "left: " << left << "| right: " << right << " | midpoint: " << midpoint << endl;
                    if (segment_buffer[midpoint].key == key) {

                        if (segment_buffer[midpoint].value == DELETED_FLAG) {
                            // cout << "KEY WAS DELETED" << endl;
                            // cout << endl;
                            // result = -1;
                            // ss << DELETED_FLAG << endl; // don't do this bc we don't want to print anything if key was deleted
                            ss << endl;
                        }

                        // else, key exists and is not deleted, so we print the value
                        else {
                            // result = segment_buffer[midpoint].value;
                            // cout << result << endl;
                            ss << to_string(segment_buffer[midpoint].value) << endl;
                        }

                        close(curr_fd); // close fd before returning
                        return ss.str();

                    } else if (segment_buffer[midpoint].key < key) {
                        left = midpoint + 1;
                    } else {
                        right = midpoint - 1;
                    }
                }

                avoid_infinite_loop = true;

                close(curr_fd); // make sure to close the fd that we opened
            }
        }
    }

    // cout << "Key: " << key << " WAS NOT FOUND!" << endl;
    ss << endl;
    return ss.str();
}

// lsm_tree::range()
//      This function returns all the keys in the lsm tree from [startKey, endKey). After doing a linear search
//      on the buffer to find all keys that exist within the given range, we query each level's fence pointer and iterate
//      through the corresponding segment for each fence ptr if the range covered by the fence ptr overlaps with the 
//      given range. Note that we search from lower levels to higher levels and maintain a set with keys that we have found
//      already to ensure we have the most recent values for each key.
string lsm_tree::range(int start, int end) {
    // for (int i = start; i < end; ++i) {
    //     get(i, true);
    // }
    // cout << endl; // to end the line that the space delineated list of all the found pairs in the given range were printed on inside `get()`

    stringstream ss;
    mutex result_mutex;
    map<int, pair<int, string>> found_keys;

    // GET LOCK ON BUFFER HERE, in a code snippet with separate scope to make sure the buffer's lock is not
    // held when we search through each level's data
    {
        lock_guard<mutex> buffer_lock(buffer_ptr_->mutex_);
        
        // iterate through skiplist linearly bc there is no guarentee which key in the given range is the smallest one in the skiplist
        // remember to start with the node that comes right after the head node on level 0

        // get the head node of the top most level
        auto curr_node = this->buffer_ptr_->buffer_skiplist->head;
        // move curr_node to point to the beginning of the lowest level linked list so we can iterate through all the nodes in sorted order
        while (curr_node->below) {
            curr_node = curr_node->below;
        }
        // because curr_node currently points to the head element, move it to the first item in the skiplist
        curr_node = curr_node->next;
        // the condition to break out will be if the curr_node is equal to the tail element, bc that means we are at the end
        // while (curr_node != this->buffer_ptr_->buffer_skiplist->tail) {
        for (int i = 0; i < this->buffer_ptr_->curr_size_; ++i) {
            // once we are at the first node in the skiplist that has a higher key than the endKey in the range, we know that there will
            // be no other nodes in the skiplist that fall within the given range as the nodes are sorted in increasing order by their keys
            if (curr_node->key >= end) {
                break;
            }
            
            if (curr_node->key >= start && curr_node->key < end) {
                // put all keys in the found_keys map, even if they are deleted values bc that is how we avoid from avoiding stale keys in larger levels
                found_keys[curr_node->key] = make_pair(0, to_string(curr_node->value));
            }

            curr_node = curr_node->next;
        }
    }

    // lambda function for use of multi-threads or just serial execution, this function is called later below
    // keeping this as lambda for now so i don't have to pass in all the parameters into a helper function lol
    auto process_level = [&](int level) {
        auto curr_level_ptr = levels_[level];
        lock_guard<mutex> current_level_lock(curr_level_ptr->mutex_);

        if (!curr_level_ptr->fp_array_) {
            assert(curr_level_ptr->num_fence_ptrs_ == 0);
            return;
        }

        for (int j = 0; j < curr_level_ptr->num_fence_ptrs_; ++j) {
            if (curr_level_ptr->fp_array_[j].min_key < end && start <= curr_level_ptr->fp_array_[j].max_key) {
                // int curr_fd = open(levels_[level]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
                int curr_fd = open(levels_[level]->disk_file_name_.c_str(), O_RDONLY);
                if (curr_fd == -1) {
                    cout << "Error in opening / creating " << levels_[level]->disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
                    exit(0);
                }

                lsm_data segment_buffer[FENCE_PTR_EVERY_K_ENTRIES];
                int entriesToRead = (j == curr_level_ptr->num_fence_ptrs_ - 1) ? curr_level_ptr->curr_size_ - (j * FENCE_PTR_EVERY_K_ENTRIES) : FENCE_PTR_EVERY_K_ENTRIES;
                ssize_t bytesToRead = entriesToRead * sizeof(lsm_data);
                ssize_t bytesRead = pread(curr_fd, segment_buffer, bytesToRead, curr_level_ptr->fp_array_[j].offset);

                if (bytesRead < 0 || bytesRead % sizeof(lsm_data) != 0) {
                    cout << "Error or incomplete read from file" << endl;
                    exit(EXIT_FAILURE);
                }

                int num_entries_in_segment = bytesRead / sizeof(lsm_data);
                for (int k = 0; k < num_entries_in_segment; ++k) {
                    // if (start <= segment_buffer[k].key && segment_buffer[k].key < end && segment_buffer[k].value != DELETED_FLAG) {
                    if (start <= segment_buffer[k].key && segment_buffer[k].key < end) {
                        lock_guard<mutex> guard(result_mutex);
                        auto it = found_keys.find(segment_buffer[k].key);
                        if (it == found_keys.end() || it->second.first > level) {
                            found_keys[segment_buffer[k].key] = make_pair(level, to_string(segment_buffer[k].value));
                        }
                    }
                }
                close(curr_fd);
            }
        }
    };

    if (PARALLEL_RANGE) {
        vector<thread> threads;
        for (int i = 1; i < MAX_LEVELS; ++i) {
            threads.push_back(thread(process_level, i));
        }
        for (auto& t : threads) {
            t.join();
        }
    } else {
        for (int i = 1; i < MAX_LEVELS; ++i) {
            process_level(i);
        }
    }

    for (const auto& kv : found_keys) {
        if (kv.second.second != to_string(DELETED_FLAG)) {
            ss << kv.first << ":" << kv.second.second << " ";
        }
    }
    ss << endl;
    return ss.str();

    // vector<thread> threads;
    // for (int i = 1; i < MAX_LEVELS; ++i) {
    //     threads.push_back(thread([&](int level) {
            
    //         auto curr_level_ptr = levels_[level];
    //         lock_guard<mutex> current_level_lock(curr_level_ptr->mutex_);

    //         for (int j = 0; j < curr_level_ptr->num_fence_ptrs_; ++j) {

    //             if (curr_level_ptr->fp_array_[j].min_key < end && start <= curr_level_ptr->fp_array_[j].max_key) {

    //                 int curr_fd = open(levels_[level]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);

    //                 if (curr_fd == -1) {
    //                     cout << "Error in opening / creating " << levels_[i]->disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
    //                     exit(0);
    //                 }

    //                 lsm_data segment_buffer[FENCE_PTR_EVERY_K_ENTRIES];

    //                 // Determine the number of entries to read for this segment
    //                 int entriesToRead = FENCE_PTR_EVERY_K_ENTRIES;
    //                 // If this is the last segment, adjust the number of entries to read
    //                 if (j == curr_level_ptr->num_fence_ptrs_ - 1) {
    //                     int remainingEntries = curr_level_ptr->curr_size_ - (j * FENCE_PTR_EVERY_K_ENTRIES);
    //                     entriesToRead = remainingEntries;
    //                 }

    //                 ssize_t bytesToRead = entriesToRead * sizeof(lsm_data);
    //                 ssize_t bytesRead = pread(curr_fd, segment_buffer, bytesToRead, curr_level_ptr->fp_array_[j].offset);
    //                 if (bytesRead < 0) {
    //                     cout << "Error reading from file" << endl;
    //                     exit(EXIT_FAILURE);
    //                 }

    //                 if (bytesRead % sizeof(lsm_data) != 0) {
    //                     cout << "Did not read a full data entry in pread()!" << endl;
    //                     exit(EXIT_FAILURE);
    //                 }

    //                 int num_entries_in_segment = bytesRead / sizeof(lsm_data);

    //                 for (int k = 0; k < num_entries_in_segment; ++k) {

    //                     if (start <= segment_buffer[k].key && segment_buffer[k].key < end) {
    //                         string value = to_string(segment_buffer[k].value);
    //                         if (segment_buffer[k].value != DELETED_FLAG) {
    //                             lock_guard<mutex> guard(result_mutex);
    //                             auto it = found_keys.find(segment_buffer[k].key);
    //                             if (it == found_keys.end() || it->second.first > level) {
    //                                 found_keys[segment_buffer[k].key] = make_pair(level, value);
    //                             }
    //                         }
    //                     }
    //                 }
    //                 close(curr_fd);
    //             }
    //         }
    //     }, i));
    // }

    // // Wait for all threads to complete
    // for (auto& t : threads) {
    //     t.join();
    // }

    // // Combine results into the stringstream
    // for (const auto& kv : found_keys) {
    //     if (kv.second.second != to_string(DELETED_FLAG)) {
    //         ss << kv.first << ":" << kv.second.second << " ";
    //     }
    // }
    // ss << endl;
    // return ss.str();
}

// lsm_tree::delete_key()
//      This function deletes a specified key from the database by inserting a new key value pair but
//      setting the delete flag to be true, so when a compaction occurs when a level calls merge(), the
//      we will skip over the deleted key when creating the new merged data
void lsm_tree::delete_key(int key) {

    // NOTE THAT WE DON'T HAVE TO LOOP THROUGH THE BUFFER HERE TO CHECK IF THE KEY IS ALR IN THE BUFFER BECAUSE BUFFER::INSERT()
    // ALREADY DOES THAT
    
    // if not found in buffer, then just insert a new key value struct with the deleted flag set as true
    insert({key, DELETED_FLAG});
    return;
}


// lsm_tree::printStats()
//      This function prints the number of logical key-value pairs in the lsm tree (keys that are not deleted or
//      stale keys to be deleted on a merge), the number of keys in each level of the tree, and a dump of the 
//      entire tree that includes the key, value, and which level it is on 
string lsm_tree::printStats() {

    // acquire all necessary locks at beginning to ensure state of LSM tree does not change while we print
    buffer_ptr_->mutex_.lock();
    for (int i = 1; i < MAX_LEVELS; ++i) {
        levels_[i]->mutex_.lock();
    }

    // main logic behind this is to iterate from buffer to larger levels, and keep track of valid and deleted keys using sets to correctly count logical pairs, but
    // this can get out of hand real fast once our database has millions of keys that it has stored, so `TODO: come back to this later to scale better`
    vector<string> terminal_output;
    terminal_output.push_back(""); // push back empty string which we can go back and later edit to store the count of logical key value pairs
    terminal_output.push_back(""); // same logic as above, this stores the count of entries at each level
    set<int> deleted_keys;
    set<int> valid_keys;

    stringstream ss;

    // unsigned long naive_sum = 0;

    // naive_sum += buffer_ptr_->curr_size_;

    // print contents of buffer
    string buffer_contents = "";
    // cout << "===Buffer contents======" << endl;

    // get the head node of the top most level
    auto curr_node = this->buffer_ptr_->buffer_skiplist->head;
    // move curr_node to point to the beginning of the lowest level linked list so we can iterate through all the nodes in sorted order
    while (curr_node->below) {
        curr_node = curr_node->below;
    }
    assert(curr_node != NULL);
    // because curr_node currently points to the head element, move it to the first item in the skiplist
    curr_node = curr_node->next;
    assert(curr_node != NULL);

    // while (curr_node != this->buffer_ptr_->buffer_skiplist->tail) {
    //     assert(curr_node != NULL);        
    //     string temp = to_string(curr_node->key) + ":" + to_string(curr_node->value) + ":L0 ";
    //     buffer_contents += temp;

    //     // note that buffer will never have duplicate keys, so no need to cross-reference across the deleted and valid sets here
    //     if (curr_node->value == DELETED_FLAG) {
    //         deleted_keys.insert(curr_node->key);
    //     } else {
    //         valid_keys.insert(curr_node->key);
    //     }

    //     // cout << curr_data.key << ":" << curr_data.value << ":L0" << endl;
    //     curr_node = curr_node->next;
    // }

    for (int i  = 0; i < this->buffer_ptr_->curr_size_; ++i) {
        assert(curr_node != NULL);        
        string temp = to_string(curr_node->key) + ":" + to_string(curr_node->value) + ":L0 ";
        buffer_contents += temp;

        // note that buffer will never have duplicate keys, so no need to cross-reference across the deleted and valid sets here
        if (curr_node->value == DELETED_FLAG) {
            deleted_keys.insert(curr_node->key);
        } else {
            valid_keys.insert(curr_node->key);
        }

        // cout << curr_data.key << ":" << curr_data.value << ":L0" << endl;
        curr_node = curr_node->next;
    }

    // if (naive_sum != valid_keys.size()) {
    //     // note that this can occur if buffer contains deleted elements, so don't freak out immediately if this prints
    //     cout << "MISMATCH BETWEEN BUFFER SIZE AND VALID KEYS SET" << endl;
    // }
    
    terminal_output[1] += "LVL0: " + to_string(buffer_ptr_->curr_size_) + ", ";
    terminal_output.push_back(buffer_contents);

    // print contents of each level's disk file by mmap and munmap
    for (int i = 1; i <= MAX_LEVELS; ++i) {
        // naive_sum += levels_[i]->curr_size_;
        // cout << "===Level " << i << " contents===" << endl;
        // auto curr_level_ptr = levels_[i];

        // int curr_fd = open(levels_[i]->disk_file_name_.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        int curr_fd = open(levels_[i]->disk_file_name_.c_str(), O_RDONLY);
        if (curr_fd == -1) {
            cout << "Error in opening / creating " << levels_[i]->disk_file_name_ << " file! Error message: " << strerror(errno) << " | Exiting program" << endl;
            exit(0);
        }
        // lsm_data* new_curr_sstable = (lsm_data*) mmap(0, levels_[i]->max_file_size, PROT_READ|PROT_WRITE, MAP_SHARED, curr_fd, 0);
        lsm_data* new_curr_sstable = (lsm_data*) mmap(0, levels_[i]->max_file_size, PROT_READ, MAP_SHARED, curr_fd, 0);
        if (new_curr_sstable == MAP_FAILED)
        {
            cout << "mmap() on the current level's file failed! Error message: " << strerror(errno) << " | Exiting program" << endl;
            close(curr_fd);
            exit(0);
        }

        string curr_level_contents = "";
        for (uint64_t j = 0; j < levels_[i]->curr_size_; ++j) {
            string temp = to_string(new_curr_sstable[j].key) + ":" + to_string(new_curr_sstable[j].value) + ":L" + to_string(i) + " ";
            curr_level_contents += temp;
            // cout << new_curr_sstable[j].key << ":" << new_curr_sstable[j].value << ":L" << i << endl;

            // if current key-value pair is not deleted and the key has not been previously deleted (due to an earlier level), add it to valid keys set
            if (new_curr_sstable[j].value != DELETED_FLAG && deleted_keys.find(new_curr_sstable[j].key) == deleted_keys.end()) {
                valid_keys.insert(new_curr_sstable[j].key);
            }
            // else if it is deleted, only add it to the deleted set if not already in valid set (due to an earlier level)
            else if (new_curr_sstable[j].value == DELETED_FLAG && valid_keys.find(new_curr_sstable[j].key) == valid_keys.end()) {
                deleted_keys.insert(new_curr_sstable[j].key);
            } else {
                // cout << "WE SHOULD NOT GET TO THIS ELSE CASE, happening on (" << new_curr_sstable[j].key << ", " << new_curr_sstable[j].value << ")" << endl;
                // assert(false);

                // ACTUALLY, WE SHOULD GET IN THIS ELSE CASE IF WE INITIALLY HAVE AN OLD KEY, VALUE PAIR IN A LARGER LEVEL ON DISK, AND RECENTLY DELETED THAT KEY,
                // BC WHEN WE ITERATE OVER EACH LEVEL'S CONTENTS AND GET TO THE OUTDATED KEY,VALUE PAIR, THAT KEY DOES NOT HAVE THE DELETED FLAG BUTTTTT IT 
                // IS THE DELETED KEYS SET SINCE WHEN WE ITERATED OVER THE NEW ENTRY MARKING DELETION, WE ADDED THE KEY TO THE DELETED KEYS SET.
                // thus, simply just don't do anything 
            }

            // if (new_curr_sstable[j].deleted) {
            //     cout << "Key " << new_curr_sstable[j].key << " is DELETED, SO NOT COUNTING IN PRINTSTATS" << endl;
            // }

            // This doesn't trigger mismatch even though there was a run with only 993 keys
            // naive_sum += 1;
            // if (naive_sum != valid_keys.size()) {
            //     cout << "MISMATCH BETWEEN LEVEL " << i << "'s SIZE AND VALID KEYS SET" << endl;
            // }
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

        // if (naive_sum != valid_keys.size()) {
        //     cout << "MISMATCH BETWEEN LEVEL " << i << "'s SIZE AND VALID KEYS SET" << endl;
        // }
    }

    // unlock all the locks we acquired at the beginning of this function
    buffer_ptr_->mutex_.unlock();
    for (int i = 1; i < MAX_LEVELS; ++i) {
        levels_[i]->mutex_.unlock();
    }

    // cout << "NAIVE SUM OF ALL CURR_SIZE_ VARIABLES: " << naive_sum << endl;

    terminal_output[0] = "Logical Pairs: " + to_string(valid_keys.size());

    for (unsigned long i = 0; i < terminal_output.size(); ++i) {
        if (terminal_output[i].length() > 0) {
            // cout << terminal_output[i] << endl;
            ss << terminal_output[i] << endl;
            // empty line to match formatting as per project requirements document
            if (i >= 1) {
                // cout << endl;
                ss << endl;
            }
        }
    }

    return ss.str();

    // for (int i = 1; i <= 1000; ++i) {
    //     if (valid_keys.find(i) == valid_keys.end()) {
    //         cout << "Key " << i << " IS NOT IN VALID" << endl;
    //     }

    //     if (deleted_keys.find(i) != deleted_keys.end()) {
    //         cout << "Key " << i << " IS IN DELETED" << endl;
    //     }
    // }
}


// lsm_tree::cleanup()
//      This function is called upon the user issuing the shutdown command (`e` on the keyboard), and it takes care of memory cleanup
void lsm_tree::cleanup() {
    // delete memory allocated for buffer (buffer object and skiplist for buffer's data)
    delete this->buffer_ptr_->buffer_skiplist;
    delete this->buffer_ptr_;

    // delete each level's fence ptr array, bloom filters, and level object itself
    for (int i = 1; i <= MAX_LEVELS; ++i) {
        if (levels_[i]->filter_) {
            // cout << "inside cleanup(), going to delete BF for level " << i << endl;
            delete levels_[i]->filter_;
        }
        if (levels_[i]->fp_array_) {
            // cout << "inside cleanup(), going to delete fp_array for level " << i << endl;
            delete[] levels_[i]->fp_array_;
        }
        // delete levels_[i]->filter_;
        // delete[] levels_[i]->fp_array_;
        delete levels_[i];
    }

    return;
}