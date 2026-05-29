#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <chrono>
#include <iomanip>
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

extern "C" {

#ifdef _WIN32
__declspec(dllexport) // 윈도우 환경 DLL 내보내기
#endif

int preprocess(const char* storage, const char* sender_csv) {
    try {
    omp_set_num_threads(NUM_THREADS);

    auto print_time = [](const string& label, Clock::time_point start, Clock::time_point end) {
        auto elapsed = chrono::duration_cast<Ms>(end - start).count();
        cout << "[time] " << label << ": "
             << elapsed << " ms"
             << " (" << elapsed / 1000.0 << " sec)"
             << endl;
    };

    cout << "[preprocess] sender preprocessing started" << endl;
    cout << "[preprocess] acceleration multi-threading enabled: " << NUM_THREADS << " threads" << endl;

    string storage_dir(storage);

    auto total_start = Clock::now();

    string parms_path = storage_dir + "/parms.bin";

    string table_bin_path = storage_dir + "/sender_table.bin";
    string coeffs_bin_path = storage_dir + "/sender_coeffs.bin";
    string meta_path = storage_dir + "/sender_meta.txt";

    // 1. sender.csv 로드
    auto t_load_start = Clock::now();

    cout << "[preprocess] loading sender csv: " << sender_csv << endl;

    auto sender_data = load_sender(sender_csv);

    cout << "[preprocess] sender data size = "
         << sender_data.size() << endl;

    auto t_load_end = Clock::now();
    print_time("Sender csv loading", t_load_start, t_load_end);

    // 2. Sender hashing
    auto t_hash_start = Clock::now();

    cout << "[preprocess] building sender hash table" << endl;

    SenderHashing sender_hashing;
    sender_hashing.locate(sender_data);

    auto t_hash_end = Clock::now();
    print_time("Sender hashing", t_hash_start, t_hash_end);

    // 3. sender_table.bin 저장
    auto t_save_table_start = Clock::now();

    save_hash_table_bin(sender_hashing.hash_table, table_bin_path);

    auto t_save_table_end = Clock::now();
    print_time("Save sender_table.bin", t_save_table_start, t_save_table_end);

    // 4. sender가 receiver와 같은 방식으로 SEAL parms를 직접 생성해서 plain_modulus 가져오기
    auto t_seal_start = Clock::now();

    cout << "[preprocess] creating local SEAL parms for plain_modulus" << endl;

    EncryptionParameters parms(scheme_type::bfv);
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, t));

    SEALContext context(parms);

    uint64_t plain_modulus =
        context.first_context_data()->parms().plain_modulus().value();

    cout << "[preprocess] plain_modulus = "
         << plain_modulus << endl;

    auto t_seal_end = Clock::now();
    print_time("Create local SEAL parms", t_seal_start, t_seal_end);

    // sender-main에서도 parms 객체 써야 하니까
    ofstream parms_out(storage_dir + "/parms.bin", ios::binary);
    parms.save(parms_out);
    parms_out.close();

    // 5. partitioning
    auto t_partitioning_start = Clock::now();

    cout << "[preprocess] partitioning sender hash table" << endl;

    SenderEvaluate sender_evaluator;

    auto partitioned =
        sender_evaluator.partitioning(sender_hashing.hash_table);

    cout << "[preprocess] partitioned size = "
         << partitioned.size() << endl;

    auto t_partitioning_end = Clock::now();
    print_time("Partitioning", t_partitioning_start, t_partitioning_end);

    // 6. coefficient extraction
    auto t_coefficients_start = Clock::now();

    cout << "[preprocess] extracting coefficients" << endl;

    auto coeffs =
        sender_evaluator.extract_all_coefficients(
            partitioned,
            plain_modulus
        );

    cout << "[preprocess] coeffs size = "
         << coeffs.size() << endl;

    auto t_coefficients_end = Clock::now();
    print_time("Coefficient extraction", t_coefficients_start, t_coefficients_end);

    // 7. sender_coeffs.bin 저장

    save_coeffs_bin(coeffs, coeffs_bin_path);

    // 8. metadata 저장

    SenderCacheMeta meta = make_current_sender_meta(
        "sender_table.bin",
        "sender_coeffs.bin",
        plain_modulus
    );

    save_sender_meta(meta, meta_path);

    auto total_end = Clock::now();
    print_time("TOTAL sender preprocessing runtime", total_start, total_end);

    double loadMs = chrono::duration<double, milli>(t_load_end - t_load_start).count();
    double hashMs = chrono::duration<double, milli>(t_hash_end - t_hash_start).count();
    double tableMs = chrono::duration<double, milli>(t_save_table_end - t_save_table_start).count();
    double sealMs = chrono::duration<double, milli>(t_seal_end - t_seal_start).count();
    double partitioningMs = chrono::duration<double, milli>(t_partitioning_end - t_partitioning_start).count();
    double coeffsMs = chrono::duration<double, milli>(t_coefficients_end - t_coefficients_start).count();
    double totalMs = chrono::duration<double, milli>(total_end - total_start).count();

    ofstream timing_out(storage_dir + "/sender_timing.json");
    timing_out << fixed << setprecision(3)
               << "{\"loadMs\":" << loadMs
               << ",\"hashMs\":" << hashMs
               << ",\"tableMs\":" << tableMs
               << ",\"sealMs\":" << sealMs
               << ",\"partitioningMs\":" << partitioningMs
               << ",\"coeffsMs\":" << coeffsMs
               << ",\"totalMs\":" << totalMs << "}\n";
    timing_out.close();

    cout << "[preprocess] sender preprocessing completed" << endl;

    return 0;

    } catch (const exception& e) {
        cerr << "[error] sender preprocessing failed: "
             << e.what() << endl;
        return 1;
    }
}
}
