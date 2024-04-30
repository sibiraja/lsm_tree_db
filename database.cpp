#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <vector>
#include "lsm.hh"

using namespace std;

uint64_t lines_processed;

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
            lsm_tree_obj->insert({key, value});
        }
        file.close();
    } else {
        cout << "File `" << newfileName << "` not found!" << endl;
    }
}

void processCommands(istream& in, lsm_tree* db) {
    string line;
    while (getline(in, line)) {
        istringstream iss(line);
        char command;
        int key, value, startKey, endKey, level;
        string fileName, return_string;
        iss >> command;
        switch (command) {
            case 'p':
                iss >> key >> value;
                db->insert({key, value});
                break;
            case 'g':
                iss >> key;
                return_string = db->get(key);
                // cout << return_string;
                break;
            case 'd':
                iss >> key;
                db->delete_key(key);
                break;
            case 'r':
                iss >> startKey >> endKey;
                return_string = db->range(startKey, endKey);
                // cout << return_string;
                break;
            case 'l':
                iss >> fileName;
                load(fileName, db);
                break;
            case 's':
                return_string = db->printStats();
                cout << return_string;
                break;
            case 'f':
                db->flush_buffer();
                break;
            case 'm':
                iss >> level;
                db->merge_level(level);
                break;
            case 'w':
            {
                iss >> fileName;
                ifstream file(fileName);
                if (file) {
                    processCommands(file, db);
                    file.close();
                } else {
                    cout << "File `" << fileName << "` not found!" << endl;
                }
                break;
            }
            case 'e':
                db->flush_buffer();
                db->cleanup();
                delete db;
                exit(0);
            default:
                cout << "Unknown command: " << command << endl;
        }

        ++lines_processed;
        if (lines_processed % 10000 == 0) {
            cout << "Lines processed: " << lines_processed << endl;
        }
    }
}

int main() {
    cout << endl << endl;
    cout << "=====NEW RUN=====" << endl << endl;
    lsm_tree* db = new lsm_tree();

    processCommands(cin, db);  // Process commands from standard input
    // if db hasn't already been cleaned up yet
    if (db) {
        db->flush_buffer();
        db->cleanup();
        delete db;
    }
    return 0;
}
