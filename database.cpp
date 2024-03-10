#include "lsm.hh"
#include <random>

int main() {
    // buffer* mybuff = new buffer();
    // mybuff->insert({1,10});
    // mybuff->insert({2,20});
    // mybuff->insert({3,30});
    // mybuff->insert({4,40});
    // mybuff->insert({5,50});
    // mybuff->insert({6,60});

    lsm_tree* db = new lsm_tree();

    // Random number generation setup
    std::random_device rd; // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_int_distribution<> distr(-500, 500); // Define the range

    for (int i = 0; i < 40; ++i) {
        // Generate random key and value
        int key = distr(gen);
        int value = distr(gen);

        cout << "Going to insert element " << i << ": (" << key << ", " << value << ")" << endl;

        // Insert the random key-value pair
        // mybuff->insert({key, value});
        db->insert({key, value});

        // print database contents every 5 insertions to check merging logic
        if (i % 5 == 0) {
            // print_database(&mybuff);
            print_database(&(db->buffer_ptr_));
        }
    }

    // Insert one more and print final database contents
    // mybuff->insert({distr(gen), distr(gen)});
    db->insert({distr(gen), distr(gen)});
    // print_database(&mybuff);
    print_database(&(db->buffer_ptr_));

    return 0;
}