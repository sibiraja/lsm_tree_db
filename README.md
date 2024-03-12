# lsm_tree_db
Log-structured merge-tree


For now, use `g++ -std=c++11 -o database database.cpp` to compile to get C++ 11 features


Note: we do a lazy deletion, where deletion only truly happens when we merge levels. Currently, `get()` returns not found for deleted keys if they were deleted, but when we print out database contents, original key-value pairs still exist, which is the correct, expected behavior! 


Some udpates on disk storage thus far:
![Alt text](Testing disk storage on put.png?raw=true "Testing disk storage on put")
![Alt text](Testing put functionality.png?raw=true "Testing put functionality")

