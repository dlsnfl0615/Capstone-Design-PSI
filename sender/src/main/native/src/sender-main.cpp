#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include <iomanip>
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

extern "C" {
#ifdef _WIN32
__declspec(dllexport) // 윈도우 환경 DLL 내보내기
#endif

int intersect(const char* storage, const char* sender_csv) {
    try {

    cout << "[debug] sender main started" << endl;

        string storage_dir(storage);

        // 단계별 시간 출력 함수
        auto print_time = [](const string& label, Clock::time_point start, Clock::time_point end) {
            auto elapsed = chrono::duration_cast<Ms>(end - start).count();
            cout << "[time] " << label << ": "
                 << elapsed << " ms"
                 << " (" << elapsed / 1000.0 << " sec)"
                 << endl;
        };

        // 전체 sender 실행 시간 측정
        auto total_start = Clock::now();

        auto t_load_start = Clock::now();

        cout << "[load] SEAL objects loading.." << endl;

        // preprocess가 만든 값 가져오기
        EncryptionParameters parms;
        cout << "[debug] opening parms.bin" << endl;

        ifstream parms_in(storage_dir + "/parms.bin", ios::binary);
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

        ifstream pk_in(storage_dir + "/public_key.bin", ios::binary);
        if (pk_in.is_open()) {
            public_key.load(context, pk_in);
            pk_in.close();
            cout << "[debug] public key loaded" << endl;
        } else {
            cerr << "[warning] public_key.bin 파일을 열 수 없습니다." << endl;
        }

        RelinKeys relin_keys;
        cout << "[debug] opening relin_key.bin" << endl;

        ifstream rk_in(storage_dir + "/relin_key.bin", ios::binary);
        if (rk_in.is_open()) {
            relin_keys.load(context, rk_in);
            rk_in.close();
            cout << "[debug] relin key loaded" << endl;
        } else {
            cerr << "[warning] relin_key.bin 파일을 열 수 없습니다." << endl;
        }

        map<int, vector<Ciphertext>> encrypted_powers;

        cout << "[debug] opening powers.bin" << endl;
        ifstream ofs_in(storage_dir + "/powers.bin", ios::binary);

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

        auto t_load_end = Clock::now();

        cout << "[debug] plain_modulus = " << plain_modulus << endl;
        cout << "[load] SEAL objects loading completed" << "\n\n";

        print_time("SEAL objects and powers loading", t_load_start, t_load_end);

        // 캐시 metadata 검증 + coeffs cache 로드

        auto t_cache_meta_start = Clock::now();

        cout << "[cache] loading sender cache metadata" << endl;

        SenderCacheMeta meta = load_sender_meta(storage_dir + "/sender_meta.txt");
        validate_sender_meta(meta, plain_modulus);

        auto t_cache_meta_end = Clock::now();
        print_time("Sender cache metadata validation", t_cache_meta_start, t_cache_meta_end);


        // sender_coeffs.bin 로드
        SenderEvaluate sender_evaluator;

        auto t_coeffs_load_start = Clock::now();

        cout << "[cache] loading sender coeffs cache" << endl;

        auto coeffs = load_coeffs_bin(storage_dir + "/sender_coeffs.bin");

        cout << "[cache] coeffs size = "
             << coeffs.size() << endl;
        auto t_coeffs_load_end = Clock::now();
        print_time("Sender coeffs loading", t_coeffs_load_start, t_coeffs_load_end);


        auto t_make_powers_start = Clock::now();

        cout << "[debug] before make_all_powers" << endl;
        map<int, vector<Ciphertext>> all_powers =
            sender_evaluator.make_all_powers(encrypted_powers, evaluator, relin_keys);
        cout << "[debug] after make_all_powers, all_powers size = "
             << all_powers.size() << endl;

        auto t_make_powers_end = Clock::now();
        print_time("Make all powers", t_make_powers_start, t_make_powers_end);

        // 0승 따로 계산
        auto t_zero_power_start = Clock::now();

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

        auto t_zero_power_end = Clock::now();
        print_time("Zero-th power encryption", t_zero_power_start, t_zero_power_end);

        // 다항식 연산
        auto t_product_start = Clock::now();

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

        auto t_product_end = Clock::now();
        print_time("Polynomial product", t_product_start, t_product_end);

        // context.last_parms_id()까지 mod switch X
        // noise budget이 0이 되어 receiver에서 최종 복호화가 제대로 되지 X

        // 다항식 연산 결과 저장

        cout << "[debug] before result.bin save" << endl;

        ofstream result_out(storage_dir + "/result.bin", ios::binary);

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
        cout << "Result generated." << endl;

        auto total_end = Clock::now();

        double loadMs = chrono::duration<double, milli>(t_load_end - t_load_start).count();
        double cacheMs = chrono::duration<double, milli>(t_cache_meta_end - t_cache_meta_start).count();
        double coeffsMs = chrono::duration<double, milli>(t_coeffs_load_end - t_coeffs_load_start).count();
        double powersMs = chrono::duration<double, milli>(t_make_powers_end - t_make_powers_start).count();
        double zeroPowersMs = chrono::duration<double, milli>(t_zero_power_end - t_zero_power_start).count();
        double productMs = chrono::duration<double, milli>(t_product_end - t_product_start).count();
        double totalMs = chrono::duration<double, milli>(total_end - total_start).count();

        ofstream timing_out(storage_dir + "/sender_timing.json");
        timing_out << fixed << setprecision(3)
                   << "{\"load\":" << loadMs
                   << ",\"cache\":" << cacheMs
                   << ",\"coeffs\":" << coeffsMs
                   << ",\"powers\":" << powersMs
                   << ",\"zeroPowers\":" << zeroPowersMs
                   << ",\"product\":" << productMs
                   << ",\"total\":" << totalMs << "}";
        timing_out.close();

        print_time("TOTAL sender runtime", total_start, total_end);

        return 0;

    }
    catch (const exception& e) {
        cerr << "[error] intersect failed: " << e.what() << endl;
        return 1;
    }
}
}
