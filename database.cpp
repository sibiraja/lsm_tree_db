#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <vector>
#include "lsm.hh"

using namespace std;

void load(string& fileName, lsm_tree* lsm_tree_obj) {
    ifstream file;
    string newfileName = "generator/" + fileName;
    file.open(newfileName, ios::binary);
    if (file) {
        while (file.good() && !file.eof()) {
            int key;
            int value;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));
            file.read(reinterpret_cast<char*>(&value), sizeof(value));
            // cout << "File: " << newfileName << " wants to insert (" << key << ", " << value << ")" << endl;
            lsm_tree_obj->insert({key, value});
            // put(key, value);
        }
        file.close();
    } else {
        cout << "File `" << newfileName << "` not found!" << endl;
    }
}

int main() {
    lsm_tree* db = new lsm_tree();
    
    string line;
    while (getline(cin, line)) {
        istringstream iss(line);
        char command;
        int key, value, startKey, endKey;
        string fileName;
        iss >> command;
        switch (command) {
            case 'p':
                iss >> key >> value;
                db->insert({key, value});
                cout << "Inserted (" << key << ", " << value << ")" << endl; 
                // put(key, value);
                break;
            case 'g':
                iss >> key;
                db->get(key);
                // get(key);
                break;
            case 'd':
                iss >> key;
                db->delete_key(key);
                // del(key);
                break;
            case 'r': {
                int startKey, endKey;
                iss >> startKey >> endKey;
                db->range(startKey, endKey);
                // range(startKey, endKey);
                break;
            }
            case 'l': {
                iss >> fileName;
                load(fileName, db);
                break;
            }
            case 's':
                // TODO: need to implement printStats() function in `lsm.hh` once bloom filters and fence pointers are integrated
                db->printStats();
                break;
            case 'f':
                db->flush_buffer();
                break;
            case 'e':
                exit(0);
            default:
                cout << "Unknown command: " << command << endl;
        }
    }
    return 0;
}
