#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include "parameters.h"
#include "data_loader.h"
#include "hashing.h"
#include "evaluate.h"
#include "cache_io.h"
#include "metadata.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    cout << "[debug] sender main started" << endl;

    // 전체 sender 실행 시간 측정
    auto total_start = Clock::now();

    // 단계별 시간 출력 함수
    auto print_time = [](const string& label, Clock::time_point start) {
        auto elapsed = chrono::duration_cast<Ms>(Clock::now() - start).count();
        cout << "[time] " << label << ": "
             << elapsed << " ms"
             << " (" << elapsed / 1000.0 << " sec)"
             << endl;
    };

    // 저장용 경로 (이 부분만 본인에게 맞게 수정)
    string receiver_data_dir = "/home/2022111531/capstone_design/receiver/receiver-psi/data/";

    auto t_load = Clock::now();

    cout << "[load] SEAL objects loading.." << endl;

    // receiver가 보내준 값 가져오기
    EncryptionParameters parms;
    cout << "[debug] opening parms.bin" << endl;

    ifstream parms_in("../data/parms.bin", ios::binary);
    if (!parms_in.is_open()) {
        cerr << "Error: parms.bin 파일을 찾을 수 없습니다." << endl;
        return 1;
    }

    cout << "[debug] parms.bin opened" << endl;
    parms.load(parms_in);
    parms_in.close();
    cout << "[debug] parms loaded" << endl;

    SEALContext context(parms);
    cout << "[debug] SEALContext created" << endl;

    PublicKey public_key;
    cout << "[debug] opening public_key.bin" << endl;

    ifstream pk_in("../data/public_key.bin", ios::binary);
    if (pk_in.is_open()) {
        public_key.load(context, pk_in);
        pk_in.close();
        cout << "[debug] public key loaded" << endl;
    } else {
        cerr << "[warning] public_key.bin 파일을 열 수 없습니다." << endl;
    }

    RelinKeys relin_keys;
    cout << "[debug] opening relin_key.bin" << endl;

    ifstream rk_in("../data/relin_key.bin", ios::binary);
    if (rk_in.is_open()) {
        relin_keys.load(context, rk_in);
        rk_in.close();
        cout << "[debug] relin key loaded" << endl;
    } else {
        cerr << "[warning] relin_key.bin 파일을 열 수 없습니다." << endl;
    }

    map<int, vector<Ciphertext>> encrypted_powers;

    cout << "[debug] opening powers.bin" << endl;
    ifstream ofs_in("../data/powers.bin", ios::binary);

    if (ofs_in.is_open()) {
        cout << "[debug] powers.bin opened" << endl;

        size_t map_size;
        size_t block_size;

        ofs_in.read(reinterpret_cast<char*>(&map_size), sizeof(size_t));
        ofs_in.read(reinterpret_cast<char*>(&block_size), sizeof(size_t));

        cout << "[debug] powers map_size = " << map_size << endl;
        cout << "[debug] powers block_size = " << block_size << endl;

        for (size_t i = 0; i < map_size; i++) {
            int key;
            ofs_in.read(reinterpret_cast<char*>(&key), sizeof(int));

            cout << "[debug] loading power key = " << key << endl;

            vector<Ciphertext> block_cts;

            for (size_t b = 0; b < block_size; b++) {
                cout << "[debug] loading ciphertext block " << b
                     << " for key " << key << endl;

                Ciphertext ct;
                ct.load(context, ofs_in);
                block_cts.push_back(move(ct));
            }

            encrypted_powers[key] = move(block_cts);
        }

        ofs_in.close();
        cout << "[debug] powers.bin loaded" << endl;
    } else {
        cerr << "[warning] data/powers.bin 파일을 열 수 없습니다." << endl;
    }

    cout << "[debug] creating evaluator / batch_encoder / encryptor" << endl;

    Evaluator evaluator(context);
    BatchEncoder batch_encoder(context);
    Encryptor encryptor(context, public_key);

    uint64_t plain_modulus =
        context.first_context_data()->parms().plain_modulus().value();

    cout << "[debug] plain_modulus = " << plain_modulus << endl;
    cout << "[load] SEAL objects loading completed" << "\n\n";

    print_time("SEAL objects and powers loading", t_load);

    // 캐시 데이터 불러오기

    auto t_cache_load = Clock::now();

    cout << "[cache] loading sender cache metadata" << endl;

    SenderCacheMeta meta = load_sender_meta("../data/sender_meta.txt");
    validate_sender_meta(meta);

    cout << "[cache] loading cached sender hash table" << endl;

    auto cached_hash_table = load_hash_table_bin("../data/sender_table.bin");

    cout << "[cache] cached sender hash_table size = "
         << cached_hash_table.size() << endl;

    print_time("Sender cached hash table loading", t_cache_load);

    // 파티셔닝 및 계수 연산
    cout << "[partitioning]" << endl;
    cout << "[debug] before SenderEvaluate create" << endl;

    SenderEvaluate sender_evaluator;

    auto t_partitioning = Clock::now();

    cout << "[debug] before partitioning" << endl;
    auto partitioned = sender_evaluator.partitioning(cached_hash_table);
    cout << "[debug] after partitioning, partitioned size = "
         << partitioned.size() << endl;

    print_time("Partitioning", t_partitioning);

    auto t_coefficients = Clock::now();

    cout << "[debug] before extract_all_coefficients" << endl;
    auto coeffs =
        sender_evaluator.extract_all_coefficients(partitioned, plain_modulus);
    cout << "[debug] after extract_all_coefficients, coeffs size = "
         << coeffs.size() << endl;

    print_time("Coefficient extraction", t_coefficients);

    auto t_make_powers = Clock::now();

    cout << "[debug] before make_all_powers" << endl;
    map<int, vector<Ciphertext>> all_powers =
        sender_evaluator.make_all_powers(encrypted_powers, evaluator, relin_keys);
    cout << "[debug] after make_all_powers, all_powers size = "
         << all_powers.size() << endl;

    print_time("Make all powers", t_make_powers);

    // 0승 따로 계산
    auto t_zero_power = Clock::now();

    cout << "[debug] before zero-th power encryption" << endl;

    vector<uint64_t> constant_one(n, 1);

    Plaintext plain_one;
    batch_encoder.encode(constant_one, plain_one);

    vector<Ciphertext> encrypted_one_blocks;

    for (int b = 0; b < num_blocks; b++) {
        cout << "[debug] encrypting zero-th power block " << b << endl;

        Ciphertext encrypted_one;
        encryptor.encrypt(plain_one, encrypted_one);
        encrypted_one_blocks.push_back(move(encrypted_one));
    }

    all_powers[0] = move(encrypted_one_blocks);

    cout << "[debug] after zero-th power encryption" << endl;

    print_time("Zero-th power encryption", t_zero_power);

    // 다항식 연산
    auto t_product = Clock::now();

    cout << "[debug] before product" << endl;

    vector<Ciphertext> producted =
        sender_evaluator.product(
            all_powers,
            coeffs,
            batch_encoder,
            evaluator,
            context
        );

    cout << "[debug] after product, producted size = "
         << producted.size() << endl;

    print_time("Polynomial product", t_product);

    // context.last_parms_id()까지 mod switch X
    // noise budget이 0이 되어 receiver에서 최종 복호화가 제대로 되지 X

    // 다항식 연산 결과 저장
    auto t_save = Clock::now();

    cout << "[debug] before result.bin save" << endl;

    ofstream result_out("../data/result.bin", ios::binary);

    if (!result_out.is_open()) {
        cerr << "Error: result.bin 저장 파일을 열 수 없습니다." << endl;
        return 1;
    }

    size_t result_size = producted.size();
    result_out.write(reinterpret_cast<const char*>(&result_size), sizeof(size_t));

    for (const auto& ct : producted) {
        ct.save(result_out);
    }

    result_out.close();

    // 다항식 연산 결과 저장 - receiver 쪽 복사 저장
    ofstream result_out_receiver(receiver_data_dir + "result.bin", ios::binary);

    if (!result_out_receiver.is_open()) {
        cerr << "Error: receiver 쪽 result.bin 저장 파일을 열 수 없습니다." << endl;
        return 1;
    }

    result_out_receiver.write(reinterpret_cast<const char*>(&result_size), sizeof(size_t));

    for (const auto& ct : producted) {
        ct.save(result_out_receiver);
    }

    result_out_receiver.close();

    cout << "[debug] after result.bin save" << endl;

    print_time("Save result.bin", t_save);

    cout << "Result generated." << endl;

    print_time("TOTAL sender runtime", total_start);

    return 0;
}
