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

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    try {
        // 전체 sender 실행 시간을 측정하기 위한 시작 시점
        auto total_start = Clock::now();

        // 단계별 실행 시간을 출력하는 함수
        // 사용 방식:
        //   auto t = Clock::now();
        //   실행할 코드
        //   print_time("단계 이름", t);
        //
        // 출력 예:
        //   [time] Sender hashing: 1234 ms (1.234 sec)
        auto print_time = [](const string& label, Clock::time_point start) {
            auto elapsed = chrono::duration_cast<Ms>(Clock::now() - start).count();
            cout << "[time] " << label << ": "
                 << elapsed << " ms"
                 << " (" << elapsed / 1000.0 << " sec)"
                 << endl;
        };

        // -----------------------------
        // 1. SEAL 객체 및 receiver 전달 파일 로드 시간 측정
        // -----------------------------
        auto t_load = Clock::now();

        cout << "[load]SEAL objects loading..";
        // receiver가 보내준 값 가져오기
        EncryptionParameters parms;
        ifstream parms_in("data/parms.bin", ios::binary);
        if (!parms_in.is_open()) {
            cerr << "Error: parms.bin 파일을 찾을 수 없습니다." << endl;
            return 1;
        }
        parms.load(parms_in);
        parms_in.close();

        SEALContext context(parms);

        PublicKey public_key;
        ifstream pk_in("data/public_key.bin", ios::binary);
        if (pk_in.is_open()) {
            public_key.load(context, pk_in);
            pk_in.close();
        }

        RelinKeys relin_keys;
        ifstream rk_in("data/relin_key.bin", ios::binary);
        if (rk_in.is_open()) {
            relin_keys.load(context, rk_in);
            rk_in.close();
        }

        // 기존:
        // map<int, Ciphertext> encrypted_powers;
        //
        // 변경:
        // exponent 하나당 block ciphertext가 여러 개 있으므로
        // map<int, vector<Ciphertext>> 사용
        map<int, vector<Ciphertext>> encrypted_powers;

        // sender의 실제 data 폴더가 sender-psi/data라면 data/...로 읽는 게 맞음.
        ifstream ofs_in("data/powers.bin", ios::binary);

        if (ofs_in.is_open()) {
            size_t map_size;
            size_t block_size;

            // receiver-request.cpp에서 저장한 순서와 동일하게 읽어야 함.
            //
            // 저장 구조:
            //   map_size
            //   block_size
            //   key, ct_block_0, ct_block_1, ...
            ofs_in.read(reinterpret_cast<char*>(&map_size), sizeof(size_t));
            ofs_in.read(reinterpret_cast<char*>(&block_size), sizeof(size_t));

            for (size_t i = 0; i < map_size; i++) {
                int key;
                ofs_in.read(reinterpret_cast<char*>(&key), sizeof(int));

                vector<Ciphertext> block_cts;

                for (size_t b = 0; b < block_size; b++) {
                    Ciphertext ct;
                    ct.load(context, ofs_in);
                    block_cts.push_back(move(ct));
                }

                encrypted_powers[key] = move(block_cts);
            }

            ofs_in.close();
        } else {
            cerr << "Error: data/powers.bin 파일을 찾을 수 없습니다." << endl;
            return 1;
        }

        // 디버깅 출력
        // m = 32768, n = 16384라면 각 exponent마다 2 blocks가 보여야 함.
        cout << "[loaded powers] ";
        for (const auto& kv : encrypted_powers) {
            cout << kv.first << "(" << kv.second.size() << " blocks) ";
        }
        cout << endl;

        Evaluator evaluator(context);
        BatchEncoder batch_encoder(context);
        Encryptor encryptor(context, public_key);
        uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();
        cout << "completed" << "\n\n";

        print_time("SEAL objects and powers loading", t_load);

        // -----------------------------
        // 2. sender 데이터 로드 시간 측정
        // -----------------------------
        auto t_data_load = Clock::now();

        cout << "[data load]\n\n";
        auto sender_data = load_sender("data/B_sender_50M.csv");

        print_time("Sender data loading", t_data_load);

        // -----------------------------
        // 3. 해싱 시간 측정
        // -----------------------------
        auto t_hashing = Clock::now();

        // 해싱
        cout << "[hashing]\n\n";
        SenderHashing sender_hashing;
        sender_hashing.locate(sender_hashing.compress(sender_data));

        print_time("Sender hashing", t_hashing);

        // -----------------------------
        // 4. 파티셔닝 시간 측정
        // -----------------------------
        auto t_partitioning = Clock::now();

        // 파티셔닝 및 계수 연산
        cout << "[partitioning]\n\n";
        SenderEvaluate sender_evaluator;
        auto partitioned = sender_evaluator.partitioning(sender_hashing.hash_table);

        print_time("Partitioning only", t_partitioning);

        // -----------------------------
        // 5. 계수 계산 시간 측정
        // -----------------------------
        auto t_coefficients = Clock::now();

        auto coeffs = sender_evaluator.extract_all_coefficients(partitioned, plain_modulus);

        print_time("Coefficient extraction", t_coefficients);

        // -----------------------------
        // 6. 필요한 모든 power 생성 시간 측정
        // -----------------------------
        auto t_make_powers = Clock::now();

        map<int, vector<Ciphertext>> all_powers = sender_evaluator.make_all_powers(encrypted_powers, evaluator, relin_keys);

        print_time("Make all powers", t_make_powers);

        // -----------------------------
        // 7. 0승 암호문 생성 시간 측정
        // -----------------------------
        auto t_zero_power = Clock::now();

        // 0승 따로 계산
        //
        // 0승은 모든 slot에서 1이어야 함.
        // 다항식의 상수항 계산에 사용됨.
        //
        // 기존 m <= n 구조에서는 all_powers[0]에 Ciphertext 하나만 넣으면 됐음.
        // 하지만 m > n 구조에서는 product()가
        //
        //   all_powers[row][block_idx]
        //
        // 형태로 접근함.
        // 따라서 all_powers[0]도 block 개수만큼 Ciphertext를 가져야 함.
        vector<uint64_t> constant_one(n, 1);

        Plaintext plain_one;
        batch_encoder.encode(constant_one, plain_one);

        vector<Ciphertext> encrypted_one_blocks;

        for (int b = 0; b < num_blocks; b++) {
            Ciphertext encrypted_one;
            encryptor.encrypt(plain_one, encrypted_one);
            encrypted_one_blocks.push_back(move(encrypted_one));
        }

        all_powers[0] = move(encrypted_one_blocks);

        print_time("Zero-th power encryption", t_zero_power);

        // -----------------------------
        // 8. 다항식 연산 시간 측정
        // -----------------------------
        auto t_product = Clock::now();
        
        // 다항식 연산
        cout << "[product]\n\n";
        vector<Ciphertext> producted = sender_evaluator.product(all_powers, coeffs, batch_encoder, evaluator, context);

        print_time("Polynomial product", t_product);

        // -----------------------------
        // 9. modulus switching 시간 측정
        // -----------------------------
        auto t_mod_switch = Clock::now();

        // 모듈러스 스위칭
        cout << "[modulus switching]\n\n";
        for (Ciphertext& polynomial : producted) {
            evaluator.mod_switch_to_inplace(polynomial, context.last_parms_id());
        }

        print_time("Modulus switching", t_mod_switch);
        
        // -----------------------------
        // 10. result.bin 저장 시간 측정
        // -----------------------------
        auto t_save = Clock::now();

        // 다항식 연산 결과 저장
        ofstream result_out("C:/psi_0515/Capstone-Design-PSI-receiver/receiver-psi/data/result.bin", ios::binary);
        size_t result_size = producted.size();
        result_out.write(reinterpret_cast<const char*>(&result_size), sizeof(size_t));
        for (const auto& ct : producted) {
            ct.save(result_out); //
        }
        result_out.close();

        print_time("Save result.bin", t_save);

        cout << "Result generated." << endl;

        // 전체 sender 실행 시간 출력
        print_time("TOTAL sender runtime", total_start);

        return 0;
    } catch (exception e) {
        cerr << e.what() << endl;
    }
}