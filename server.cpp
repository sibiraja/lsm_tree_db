#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <vector>

#include "lsm.hh"

// libraries for networking
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h> // for open()
#include <sys/stat.h> // to get size of files efficiently with stat structs
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h> // ftok()
#include <sys/shm.h>

#define PORT 8081

using namespace std;

void load(string& fileName, lsm_tree* lsm_tree_obj) {
    ifstream file;

    std::string cleanFileName;
    for (char c : fileName) {
        if (c != '"' && c != '\'') {
            cleanFileName += c;
        }
    }
    fileName = cleanFileName;

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

bool processCommands(istream& in, lsm_tree* db, int client_socket_fd, uint64_t lines_processed = 0) {
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
                send(client_socket_fd, return_string.c_str(), return_string.length(), 0);
                break;
            case 'd':
                iss >> key;
                db->delete_key(key);
                break;
            case 'r':
                iss >> startKey >> endKey;
                return_string = db->range(startKey, endKey);
                send(client_socket_fd, return_string.c_str(), return_string.length(), 0);
                break;
            case 'l':
                iss >> fileName;
                load(fileName, db);
                break;
            case 's':
                return_string = db->printStats();
                send(client_socket_fd, return_string.c_str(), return_string.length(), 0);
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
                    processCommands(file, db, client_socket_fd);
                    file.close();
                } else {
                    send(client_socket_fd, "File not found!\n", 16, 0);
                }
                break;
            }
            case 'e':
                db->flush_buffer();
                db->cleanup();
                delete db;
                exit(0);
            case 'q':  // Client requests to close the connection
                send(client_socket_fd, "Goodbye!\n", 9, 0);
                return true;  // Return true to signal disconnect
            default:
                return_string = "Unknown command: ";
                return_string += command;
                return_string += '\n';
                send(client_socket_fd, return_string.c_str(), return_string.length(), 0);
        }

        ++lines_processed;
        if (lines_processed % 10000 == 0) {
            return_string = "Lines processed: " + to_string(lines_processed) + '\n';
            send(client_socket_fd, return_string.c_str(), return_string.length(), 0);
        }
    }

    return false; // don't return true since that will be interpreted by handle_client_connection() to close the client's connection
}

void handle_client_connection(int client_socket_fd, lsm_tree* db) {
    uint64_t lines_processed = 0;
    char buffer[256];
    bool shouldClose = false;
    while (!shouldClose) {
        memset(buffer, 0, 256);
        int n = read(client_socket_fd, buffer, 255);
        if (n <= 0) {
            cerr << "ERROR reading from socket or client disconnected" << endl;
            break;
        }

        string input(buffer);
        stringstream ss(input);
        shouldClose = processCommands(ss, db, client_socket_fd, lines_processed);
    }
    close(client_socket_fd);
}



int main() {
    cout << endl;
    cout << endl;
    cout << "=====NEW RUN=====" << endl;
    cout << endl;
    lsm_tree* db = new lsm_tree();

    // 1. socket()
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        cout << "Error in creating socket file descriptor" << endl;
        cout << "Error: " << errno << ": " << strerror(errno) << endl;
        exit(1);
    }


    // 2. setsockopt() (optional)
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        cout << "Error setting socket options" << endl;
        cout << "Error: " << errno << ": " << strerror(errno) << endl;
        close(sockfd);
        exit(1);
    }


    // 3. bind()

    struct sockaddr_in in_socket_struct;
    memset((char*) &in_socket_struct, 0, sizeof(in_socket_struct));

    in_socket_struct.sin_family = AF_INET;
    in_socket_struct.sin_port = htons(PORT);
    in_socket_struct.sin_addr.s_addr = htonl(INADDR_ANY); // defined to be address that accepts any incoming messages (recall this is on server -- on client, we need to specify a specific IP addr to connect to)

    // See https://stackoverflow.com/questions/10035294/compiling-code-that-uses-socket-function-bind-with-libcxx-fails for why ::bind and not just bind
    int r = ::bind(sockfd, (struct sockaddr *) &in_socket_struct, sizeof(in_socket_struct)); // Note that we need to cast the sockaddr_in struct to a pointer for the bind() call
    if (r == -1) {
        cout << "error in binding" << endl;
        cout << "Error: " << errno << ": " << strerror(errno) << endl;
        close(sockfd);
        exit(1);
    }


    // 4. listen()
    r = listen(sockfd, 5);
    if (r < 0) {
        cout << "Error in listening" << endl;
        cout << "Error: " << errno << ": " << strerror(errno) << endl;
        close(sockfd);
        exit(1);
    }

    cout << "Server is now listening on port: " << PORT << endl; 

    // int total_reqs_handled = 0;

    // server is now listening and will accept any client requests in sequential ordering, blocking on each
    while (true) {

        // 5. accept ()
        sockaddr_in client_info_to_fill;
        socklen_t cli_in_addr;
        int client_sock_fd = accept(sockfd, (sockaddr *) &client_info_to_fill, &cli_in_addr); // Note that we also need to pass in pointers here as well

        if (client_sock_fd == -1) {
            cout << "Error in accepting because client socket is " << client_sock_fd << endl;
            cout << "Error: " << errno << ": " << strerror(errno) << endl;
            continue;
        }


        // HAVE A MORE GRACEFUL WAY OF HANDLING THIS, PERHAPS HAVE A CLIENT CONNECTION CLOSE COMMAND SO THREADS CAN EXIT WITHOUT DISTURBING OTHERS
        // AND A SHUTDOWN COMMAND THAT SHUTS THE ENTIRE SERVER DOWN
        // if another thread has already closed/shutdown the database, close the server
        if (!db) {
            break;
        }

        // start a thread here
        thread(handle_client_connection, client_sock_fd, db).detach();
    }

    close(sockfd);
    // if db hasn't already been cleaned up yet
    if (db) {
        db->flush_buffer();
        db->cleanup();
        delete db;
    }
    return 0;
}
