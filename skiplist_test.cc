#include <iostream>
#include <string>

#include "skiplist.hh"

int main()
{
    skipList* sl = new skipList();

    for (int i = 0; i < 100; ++i) {
        sl->skipInsert(i, i);
    }
    sl->printSkipList();

    for (int i = 0; i < 100; ++i) {
        auto sl_node = sl->skipSearch(i);
        assert(sl_node->key == i);
    }

    delete sl;

    

    // sl->skipInsert(6, 6);
    // sl->skipInsert(15, 15);
    // sl->skipInsert(4, 4);

    // sl->printSkipList();



    // sl->remove(4);
    // sl->printSkipList();
}