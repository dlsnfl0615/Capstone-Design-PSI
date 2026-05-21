#include <iostream>
#include <chrono>
#include <exception>
#include <fstream>
#include <omp.h> // OpenMP 헤더 추

#include "seal/seal.h"

#include "data_loader.h"
#include "hashing.h"
#include "evaluate.h"
#include "cache_io.h"
#include "metadata.h"
#include "parameters.h"

using namespace std;
using namespace seal;

using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    omp_set_num_threads(NUM_THREADS);
    
    cout << "[preprocess] sender preprocessing started" << endl;
    cout << "[preprocess] acceleration multi-threading enabled: " << NUM_THREADS << " threads" << endl;

    auto total_start = Clock::now();

    auto print_time = [](const string& label, Clock::time_point start) {
        auto elapsed = chrono::duration_cast<Ms>(Clock::now() - start).count();
        cout << "[time] " << label << ": "
             << elapsed << " ms"
             << " (" << elapsed / 1000.0 << " sec)"
             << endl;
    };

    try {
        // 경로 설정
        string sender_csv_path = "../data/sender.csv";
        string parms_path = "../data/parms.bin";

        string table_bin_path = "../data/sender_table.bin";
        string coeffs_bin_path = "../data/sender_coeffs.bin";
        string meta_path = "../data/sender_meta.txt";

        // 1. sender.csv 로드
        auto t_load = Clock::now();

        cout << "[preprocess] loading sender csv: " << sender_csv_path << endl;

        auto sender_data = load_sender(sender_csv_path);

        cout << "[preprocess] sender data size = "
             << sender_data.size() << endl;

        print_time("Sender csv loading", t_load);

        // 2. Sender hashing
        auto t_hash = Clock::now();

        cout << "[preprocess] building sender hash table" << endl;

        SenderHashing sender_hashing;
        sender_hashing.locate(sender_data);

        print_time("Sender hashing", t_hash);

        // 3. sender_table.bin 저장
        auto t_save_table = Clock::now();

        save_hash_table_bin(sender_hashing.hash_table, table_bin_path);

        print_time("Save sender_table.bin", t_save_table);

        // 4. parms.bin 로드해서 plain_modulus 가져오기
        auto t_seal = Clock::now();

        cout << "[preprocess] loading SEAL parms: " << parms_path << endl;

        EncryptionParameters parms;
        ifstream parms_in(parms_path, ios::binary);

        if (!parms_in.is_open()) {
            throw runtime_error("Failed to open parms.bin: " + parms_path);
        }

        parms.load(parms_in);
        parms_in.close();

        SEALContext context(parms);

        uint64_t plain_modulus =
            context.first_context_data()->parms().plain_modulus().value();

        cout << "[preprocess] plain_modulus = "
             << plain_modulus << endl;

        print_time("Load SEAL parms", t_seal);

        // 5. partitioning
        auto t_partitioning = Clock::now();

        cout << "[preprocess] partitioning sender hash table" << endl;

        SenderEvaluate sender_evaluator;

        auto partitioned =
            sender_evaluator.partitioning(sender_hashing.hash_table);

        cout << "[preprocess] partitioned size = "
             << partitioned.size() << endl;

        print_time("Partitioning", t_partitioning);

        // 6. coefficient extraction
        auto t_coefficients = Clock::now();

        cout << "[preprocess] extracting coefficients" << endl;

        auto coeffs =
            sender_evaluator.extract_all_coefficients(
                partitioned,
                plain_modulus
            );

        cout << "[preprocess] coeffs size = "
             << coeffs.size() << endl;

        print_time("Coefficient extraction", t_coefficients);

        // 7. sender_coeffs.bin 저장
        auto t_save_coeffs = Clock::now();

        save_coeffs_bin(coeffs, coeffs_bin_path);

        print_time("Save sender_coeffs.bin", t_save_coeffs);

        // 8. metadata 저장
        auto t_save_meta = Clock::now();

        SenderCacheMeta meta = make_current_sender_meta(
            "sender_table.bin",
            "sender_coeffs.bin",
            plain_modulus
        );

        save_sender_meta(meta, meta_path);

        print_time("Save sender_meta.txt", t_save_meta);

    } catch (const exception& e) {
        cerr << "[error] sender preprocessing failed: "
             << e.what() << endl;
        return 1;
    }

    print_time("TOTAL sender preprocessing runtime", total_start);

    cout << "[preprocess] sender preprocessing completed" << endl;

    return 0;
}
