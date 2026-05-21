#include <iostream>
#include <chrono>
#include <exception>

#include "data_loader.h"
#include "hashing.h"
#include "cache_io.h"
#include "metadata.h"

using namespace std;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    cout << "[preprocess] sender preprocessing started" << endl;

    auto total_start = Clock::now();

    auto print_time = [](const string& label, Clock::time_point start) {
        auto elapsed = chrono::duration_cast<Ms>(Clock::now() - start).count();
        cout << "[time] " << label << ": "
             << elapsed << " ms"
             << " (" << elapsed / 1000.0 << " sec)"
             << endl;
    };

    try {
        string sender_csv_path = "../data/sender.csv";
        string table_bin_path = "../data/sender_table.bin";
        string meta_path = "../data/sender_meta.txt";

        auto t_load = Clock::now();

        cout << "[preprocess] loading sender csv: " << sender_csv_path << endl;
        auto sender_data = load_sender(sender_csv_path);

        cout << "[preprocess] sender data size = " << sender_data.size() << endl;

        print_time("Sender csv loading", t_load);

        auto t_hash = Clock::now();

        cout << "[preprocess] building sender hash table" << endl;

        SenderHashing sender_hashing;
        sender_hashing.locate(sender_data);

        print_time("Sender hashing", t_hash);

        auto t_save_table = Clock::now();

        save_hash_table_bin(sender_hashing.hash_table, table_bin_path);

        print_time("Save sender_table.bin", t_save_table);

        auto t_save_meta = Clock::now();

        SenderCacheMeta meta = make_current_sender_meta("sender_table.bin");
        save_sender_meta(meta, meta_path);

        print_time("Save sender_meta.txt", t_save_meta);

    } catch (const exception& e) {
        cerr << "[error] sender preprocessing failed: " << e.what() << endl;
        return 1;
    }

    print_time("TOTAL sender preprocessing runtime", total_start);

    cout << "[preprocess] sender preprocessing completed" << endl;

    return 0;
}