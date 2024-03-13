# lsm_tree_db
Log-structured merge-tree


For now, use `g++ -std=c++11 -o database database.cpp` to compile to get C++ 11 features


Run `./database < commands.txt > db.log 2>&1` to manual test
Run `python3 custom_workload.py` to generate the `commands.txt` file


Notes:
- there is a (probabilistic because it only occurs sometimes -- makes no sense since I have no multi-process or multi-threaded capability yet) bug where the number of logical pairs is incorrect during my initial test of 1000 insertions, where it sometimes is under 1,000. However, when inspecting the contents of each level, all the entries seem to be there, so I'm not sure what is going on, but reminder to self: `TODO: come back to this!`. but might just be in the `printStats()` function of calculting number of logical pairs, so not a priority right now 