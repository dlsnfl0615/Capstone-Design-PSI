#include <iostream>
#include <bitset>
#include <vector>
#include <string>
#include "../parameters.h"
#include "hashing.h"

void permutation_hashing(const vector<ReceiverRecord> combined) {
    cout << "permutation" << endl;
    for (auto data : combined) {
        auto numbers = data.combined;
        cout << numbers << endl;
    }
}