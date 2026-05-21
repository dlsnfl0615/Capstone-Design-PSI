#include "cache_io.h"
#include "parameters.h"

#include <fstream>
#include <stdexcept>
#include <iostream>

using namespace std;

static const uint64_t HASH_TABLE_MAGIC = 0x53504854424C3031ULL; 
// SPHTBL01 비슷한 식별값
static const uint64_t HASH_TABLE_VERSION = 1;

void save_hash_table_bin(
    const vector<vector<uint64_t>>& table,
    const string& filepath
) {
    ofstream out(filepath, ios::binary);

    if (!out.is_open()) {
        throw runtime_error("Failed to open hash table cache for write: " + filepath);
    }

    uint64_t mm = static_cast<uint64_t>(m);
    uint64_t BB = static_cast<uint64_t>(B);

    out.write(reinterpret_cast<const char*>(&HASH_TABLE_MAGIC), sizeof(HASH_TABLE_MAGIC));
    out.write(reinterpret_cast<const char*>(&HASH_TABLE_VERSION), sizeof(HASH_TABLE_VERSION));
    out.write(reinterpret_cast<const char*>(&mm), sizeof(mm));
    out.write(reinterpret_cast<const char*>(&BB), sizeof(BB));

    if (table.size() != static_cast<size_t>(m)) {
        throw runtime_error("Hash table row size mismatch.");
    }

    for (int row = 0; row < m; row++) {
        if (table[row].size() != static_cast<size_t>(B)) {
            throw runtime_error("Hash table column size mismatch at row " + to_string(row));
        }

        out.write(
            reinterpret_cast<const char*>(table[row].data()),
            sizeof(uint64_t) * B
        );
    }

    out.close();

    cout << "[cache] sender hash table saved: " << filepath << endl;
}

vector<vector<uint64_t>> load_hash_table_bin(
    const string& filepath
) {
    ifstream in(filepath, ios::binary);

    if (!in.is_open()) {
        throw runtime_error("Failed to open hash table cache for read: " + filepath);
    }

    uint64_t magic = 0;
    uint64_t version = 0;
    uint64_t mm = 0;
    uint64_t BB = 0;

    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&mm), sizeof(mm));
    in.read(reinterpret_cast<char*>(&BB), sizeof(BB));

    if (magic != HASH_TABLE_MAGIC) {
        throw runtime_error("Invalid sender_table.bin magic value.");
    }

    if (version != HASH_TABLE_VERSION) {
        throw runtime_error("Unsupported sender_table.bin version.");
    }

    if (mm != static_cast<uint64_t>(m) || BB != static_cast<uint64_t>(B)) {
        throw runtime_error(
            "Hash table cache parameter mismatch. "
            "Cached m/B does not match current parameters.h"
        );
    }

    vector<vector<uint64_t>> table(
        m,
        vector<uint64_t>(B, SENDER_DUMMY)
    );

    for (int row = 0; row < m; row++) {
        in.read(
            reinterpret_cast<char*>(table[row].data()),
            sizeof(uint64_t) * B
        );

        if (!in) {
            throw runtime_error("Failed while reading hash table row " + to_string(row));
        }
    }

    in.close();

    cout << "[cache] sender hash table loaded: " << filepath << endl;

    return table;
}