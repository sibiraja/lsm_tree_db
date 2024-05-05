// This code was heavily inspired by Tyler Reed's YouTube tutorial on skiplists: https://www.youtube.com/watch?v=Fsw6J8I6X7o, following his Java Design but tweaking it for C++

#ifndef SKIPLIST_HH
#define SKIPLIST_HH

#include <climits>
#include <iostream>
#include <random>
#include <ctime>
#include <cmath>
#include <cassert>
 

class Node {
public:
    Node* above;
    Node* below;
    Node* prev;
    Node* next;

    int key;
    int value;

    Node(int key, int value) {
        this->key = key;
        this->value = value;
        this->above = nullptr;
        this->below = nullptr;
        this->next = nullptr;
        this->prev = nullptr;
    }
};

class skipList {
public:
    Node* head;
    Node* tail;

    int curr_height = 0;
    int capacity = 10240;
    int max_height = 0;

    skipList(int capacity=10240) {
        // not setting value to be INT_MIN to avoid any future bugs with confusing head and tail as entries to be deleted
        head = new Node(INT_MIN, INT_MAX);
        tail = new Node(INT_MAX, INT_MAX);
        head->next = tail;
        tail->prev = head;
        
        this->capacity = capacity;

        auto result = std::log10(this->capacity);
        max_height = static_cast<int>(std::ceil(result));

        // std::cout << "max_height: " << max_height << std::endl;
    }

    
    ~skipList() {
        // Start from the highest level head node
        Node* levelHead = head;

        // Move down each level
        while (levelHead != nullptr) {
            Node* current = levelHead;
            Node* nextLevelHead = levelHead->below;

            // Traverse and delete nodes horizontally across the current level
            while (current != nullptr) {
                Node* nextNode = current->next;
                delete current;
                current = nextNode;
            }

            // Move down to the next level
            levelHead = nextLevelHead;
        }

        head = nullptr;
        tail = nullptr;
    }


    Node* skipSearch(int key) {
        Node* n = head;

        while (n->below != nullptr) {
            n = n->below;

            while (n->next != nullptr && key >= n->next->key) {
                n = n->next;
            }
        }

        assert(n != NULL);
        return n;
    }

    Node* skipInsert(int key, int value) {
        Node* position = skipSearch(key);
        assert(position);
        Node* q;

        int level = -1;
        // int num_heads = -1;

        if (position->key == key) {
            return position;
        }

        do {
            // ++num_heads;
            ++level;

            canIncreaseLevel(level);

            q = position;

            while (position->above == nullptr) {
                position = position->prev;
            }

            position = position->above;

            q = insertAfterAbove(position, q, key, value);

        } while (rand()%2 && level < max_height);

        return q;
    }

    Node* remove(int key) {
        Node* nodeToBeRemoved = skipSearch(key);

        if (nodeToBeRemoved->key != key) {
            return nullptr;
        }

        removeReferencesToNode(nodeToBeRemoved);

        while (nodeToBeRemoved != NULL) {
            removeReferencesToNode(nodeToBeRemoved);

            if (nodeToBeRemoved->above != nullptr) {
                nodeToBeRemoved = nodeToBeRemoved->above;
            } else {
                break;
            }
        }

        return nodeToBeRemoved;
    }

    void removeReferencesToNode(Node* nodeToBeRemoved) {
        Node* afterNodeToBeRemoved = nodeToBeRemoved->next;
        Node* beforeNodeToBeRemoved = nodeToBeRemoved->prev;

        beforeNodeToBeRemoved->next = afterNodeToBeRemoved;
        afterNodeToBeRemoved->prev = beforeNodeToBeRemoved;
    }

    void canIncreaseLevel(int level) {
        if (level >= curr_height) {
            ++curr_height;
            addEmptyLevel();
        }
    }

    void addEmptyLevel() {
        Node* newHead = new Node(INT_MIN, INT_MAX);
        Node* newTail = new Node(INT_MAX, INT_MAX);

        newHead->next = newTail;
        newHead->below = head;
        newTail->prev = newHead;
        newTail->below = tail;

        head->above = newHead;
        tail->above = newTail;

        head = newHead;
        tail = newTail;
    }

    Node* insertAfterAbove(Node* position, Node* q, int key, int value) {
        Node* newNode = new Node(key, value);
        Node* nodeBeforeNewNode = position->below->below;

        setBeforeAndAfterReferences(q, newNode);
        setAboveAndBelowReferences(position, key, newNode, nodeBeforeNewNode);

        return newNode;
    }

    void setBeforeAndAfterReferences(Node* q, Node* newNode) {
        newNode->next = q->next;
        newNode->prev = q;
        q->next->prev = newNode;
        q->next = newNode;
    }

    void setAboveAndBelowReferences(Node* position, int key, Node* newNode, Node* nodeBeforeNewNode) {
        if (nodeBeforeNewNode != NULL) {
            while (true) {
                if (nodeBeforeNewNode->next->key != key) {
                    nodeBeforeNewNode = nodeBeforeNewNode->next;
                } else {
                    break;
                }
            }

            newNode->below = nodeBeforeNewNode->next;
            nodeBeforeNewNode->next->above = newNode;
        }

        if (position != NULL) {
            if (position->next->key == key) {
                newNode->above = position->next;
            }
        }
    }

    void printSkipList() {
        std::string output = "";
        output += "\nSkipList starting with the top-left most node.\n";

        Node* starting = head;

        Node* highestLevel = starting;
        int level = curr_height;

        while (highestLevel != NULL) {
            output += "\nLevel: " + std::to_string(level) + "\n";

            while (starting != NULL) {
                output += "(" + std::to_string(starting->key) + ", " + std::to_string(starting->value) + ")";

                if (starting->next != NULL) {
                    output += " , ";
                }

                starting = starting->next;
            }

            highestLevel = highestLevel->below;
            starting = highestLevel;
            --level;
        }

        std::cout << output << std::endl;
    }
};

#endif // LSM_HH