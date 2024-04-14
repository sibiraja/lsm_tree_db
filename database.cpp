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
    cout << endl;
    cout << endl;
    cout << "=====NEW RUN=====" << endl;
    cout << endl;
    lsm_tree* db = new lsm_tree();
    
    string line;
    while (getline(cin, line)) {
        istringstream iss(line);
        char command;
        int key, value, startKey, endKey;
        string fileName, return_string;
        iss >> command;
        switch (command) {
            case 'p':
                iss >> key >> value;
                db->insert({key, value});
                // cout << "Inserted (" << key << ", " << value << ")" << endl; 
                // put(key, value);
                break;
            case 'g':
                iss >> key;
                return_string = db->get(key);
                cout << return_string;
                // get(key);
                break;
            case 'd':
                iss >> key;
                db->delete_key(key);
                // del(key);
                break;
            case 'r': {
                iss >> startKey >> endKey;
                return_string = db->range(startKey, endKey);
                cout << return_string;
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
                return_string = db->printStats();
                cout << return_string;
                break;
            case 'f':
                db->flush_buffer();
                break;
            case 'e':
                db->flush_buffer();
                db->cleanup();
                delete db;
                return 0;
            default:
                cout << "Unknown command: " << command << endl;
        }
    }
    return 0;
}
