#include "lsm.hh"

int main() {
    buffer* mybuff = new buffer();
    mybuff->insert({1,10});
    mybuff->insert({2,20});
    mybuff->insert({3,30});
    mybuff->insert({4,40});
    mybuff->insert({5,50});
    mybuff->insert({6,60});

    print_database(&mybuff);

    return 0;
}