#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <vector>

using namespace std;

map<int, int> lsm_tree; // using default map to test parser functionality

void put(int key, int value) {
    lsm_tree[key] = value;
}

void get(int key) {
    if (lsm_tree.find(key) == lsm_tree.end()) {
        cout << "Key not found!" << endl; 
    } else {
        cout << lsm_tree[key] << endl;
    }
}

void del(int key) {
    lsm_tree.erase(key);
}

void range(int startKey, int endKey) {
    for (int key = startKey; key < endKey; ++key) {
        if (lsm_tree.find(key) != lsm_tree.end()) {
            cout << key << ":" << lsm_tree[key] << " ";
        }
    }
    cout << endl;
}

void load(string& fileName) {
    ifstream file;
    string newfileName = "generator/" + fileName;
    file.open(newfileName, ios::binary);
    if (file) {
        while (file.good() && !file.eof()) {
            int key;
            int value;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));
            file.read(reinterpret_cast<char*>(&value), sizeof(value));
            put(key, value);
        }
        file.close();
    } else {
        cout << "File `" << newfileName << "` not found!" << endl;
    }
}

void printStats() {
    cout << "Logical pairs: " << lsm_tree.size() << endl;
    // for now, our lsm tree just has 1 level
    cout << "LVL1: " << lsm_tree.size() << endl;
    for (auto it = lsm_tree.begin(); it != lsm_tree.end(); ++it) {
        cout << it->first << ":" << it->second << ":L1 ";
    }
    cout << endl;
}

int main() {
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
                put(key, value);
                break;
            case 'g':
                iss >> key;
                get(key);
                break;
            case 'd':
                iss >> key;
                del(key);
                break;
            case 'r': {
                int startKey, endKey;
                iss >> startKey >> endKey;
                range(startKey, endKey);
                break;
            }
            case 'l': {
                iss >> fileName;
                load(fileName);
                break;
            }
            case 's':
                printStats();
                break;
            default:
                cout << "Unknown command: " << command << endl;
        }
    }
    return 0;
}
