# lsm_tree_db
Log-structured merge-tree


For now, use `g++ -std=c++11 -o database database.cpp` to compile to get C++ 11 features


Note: we do a lazy deletion, where deletion only truly happens when we merge levels. Currently, `get()` returns not found for deleted keys if they were deleted, but when we print out database contents, original key-value pairs still exist, which is the correct, expected behavior! 


Run `./database < commands.txt > db.log 2>&1` to manual test
Run `python3 custom_workload.py` to generate the `commands.txt` file