#include <iostream>
#include "data_loader.h"
#include "hashing.h"

using namespace std;

int main() {
    auto receiver_data = load_receiver("data/receiver.csv");
    Hashing hashing;
    hashing.locate(receiver_data);
    hashing.print_hash_table();
}