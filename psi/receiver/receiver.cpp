#include <iostream>
#include "data_loader.h"
#include "hashing.h"

using namespace std;

int main() {
    auto receiver_data = load_receiver("data/receiver.csv");
    permutation_hashing(receiver_data);
}