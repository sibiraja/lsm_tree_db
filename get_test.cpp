#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <vector>
#include "lsm.hh"

using namespace std;

void executeCommand(const string& line, lsm_tree* lsm_tree_obj) {
    istringstream iss(line);
    char command;
    int key, value, startKey, endKey;
    string fileName;
    iss >> command;
    switch (command) {
        case 'p':
            iss >> key >> value;
            lsm_tree_obj->insert({key, value});
            cout << "Inserted (" << key << ", " << value << ")" << endl;
            break;
        case 'g':
            iss >> key;
            // Assuming get method prints the value or result
            lsm_tree_obj->get(key);
            break;
        case 'd':
            iss >> key;
            lsm_tree_obj->delete_key(key);
            cout << "Deleted key: " << key << endl;
            break;
        case 'r':
            iss >> startKey >> endKey;
            lsm_tree_obj->range(startKey, endKey);
            // Assuming range method handles printing itself
            break;
        case 's':
            // TODO: Implement printStats() in lsm_tree as needed
            lsm_tree_obj->printStats();
            break;
        case 'f':
            lsm_tree_obj->flush_buffer();
            cout << "Buffer flushed." << endl;
            break;
        case 'e':
            cout << "Exiting." << endl;
            exit(0);
        default:
            cout << "Unknown command: " << command << endl;
    }
}

void processCommandsFromFile(const string& filePath, lsm_tree* db) {
    ifstream file(filePath);
    string line;

    if (!file.is_open()) {
        cout << "Could not open file " << filePath << endl;
        return;
    }

    while (getline(file, line)) {
        executeCommand(line, db);
    }
    file.close();
}

int main() {
    cout << "=====NEW RUN=====" << endl;
    lsm_tree* db = new lsm_tree();

    // Process commands from file first
    processCommandsFromFile("get_workload.txt", db);

    // Then switch to manual input
    string line;
    while (getline(cin, line)) {
        executeCommand(line, db);
    }

    delete db; // Clean up to prevent memory leak
    return 0;
}