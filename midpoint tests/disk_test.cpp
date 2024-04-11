#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

using namespace std;

struct lsm_data {
    int key;
    int value;
    bool deleted;
};

int main() {
    int fd = open("LEVEL1.data", O_RDWR | O_CREAT, (mode_t)0600);
    if (fd == -1) {
        cout << "Error in opening / creating LEVEL1.data file! Exiting program" << endl;
        exit(0);
    }

    // memory map the metadata file contents into process memory so we can access it when inside any level::function()
    lsm_data* ptr = (lsm_data*) mmap(0,(4096), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        cout << "mmap() on the LEVEL1.data file failed! Exiting program" << endl;
        close(fd);
        exit(0);
    }

    for (int i = 0; i < 10; ++i) {
        lsm_data curr_data_entry = ptr[i];
        cout << "(" << curr_data_entry.key << ", " << curr_data_entry.value << ")" << endl;
    }

    close(fd);

    fd = open("level_metadata.data", O_RDWR | O_CREAT, (mode_t)0600);
    if (fd == -1) {
        cout << "Error in opening / creating global `level_metadata` file! Exiting program" << endl;
        exit(0);
    }

    // memory map the metadata file contents into process memory so we can access it when inside any level::function()
    int* metadata_ptr = (int*) mmap(0,100, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (metadata_ptr == MAP_FAILED)
    {
        cout << "mmap() on the metadata file failed! Exiting program" << endl;
        close(fd);
        exit(0);
    }

    for (int i = 0; i < 10; ++i) {
        int curr_size = metadata_ptr[i];
        cout << "Level " << i << "'s size: " << curr_size << endl;
    }

    return 0;
}