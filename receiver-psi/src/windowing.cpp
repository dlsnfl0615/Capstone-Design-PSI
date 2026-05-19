#include "windowing.h"

vector<int> Windowing::get_exponents() {
    vector<int> result;

    int exponent_max = B_prime;
    int j_max = static_cast<int>(floor(log2(ceil(B_prime)) / l));
    int i_max = (1 << l) - 1;

    cout << "[windowing]" << endl;

    bool exit_flag = false;
    for (int j = 0; j <= j_max; j++) {
        for (int i = 1; i <= i_max; i++) {
            int e = i * (1 << (l * j));

            if (e > exponent_max) {
                exit_flag = true;
                break;
            }

            result.push_back(e);
        }

        if (exit_flag) {
            break;
        }
    }

    return result;
}

map<int, vector<Ciphertext>> Windowing::receiver_windowing(
    BatchEncoder& batch_encoder,
    Encryptor& encryptor,
    const uint64_t plain_modulus,
    const vector<int>& exponents,
    const vector<uint64_t>& hash_table) {

    // result[e]는 exponent e에 대한 block별 암호문 벡터
    //
    // 예:
    // result[1][0] = block 0의 1승 암호문
    // result[1][1] = block 1의 1승 암호문
    // result[2][0] = block 0의 2승 암호문
    // ...
    map<int, vector<Ciphertext>> result;

    for (int e : exponents) {
        vector<Ciphertext> block_ciphertexts;

        // m개의 bin을 n개씩 나눠서 암호화함
        //
        // 기존 구조:
        //   ciphertext 1개 안에 bin 0 ~ m-1을 모두 넣음
        //
        // 변경 구조:
        //   block 0 ciphertext에는 bin 0 ~ n-1
        //   block 1 ciphertext에는 bin n ~ 2n-1
        //   ...
        for (int block_idx = 0; block_idx < num_blocks; block_idx++) {

            // BatchEncoder에 넣을 plaintext slot 벡터
            // 크기는 반드시 n이어야 함.
            //
            // 이유:
            // BFV batching에서 한 번에 encode하는 slot 수는 n 기준이므로,
            // m > n이라고 해서 vector 크기를 m으로 만들면 안 됨.
            vector<uint64_t> pod_powers(n, RECEIVER_DUMMY);

            for (int slot = 0; slot < n; slot++) {
                // block 내부 slot 번호를 전체 hash_table의 bin 번호로 변환
                //
                // 예:
                // block_idx = 0, slot = 10 -> global_bin = 10
                // block_idx = 1, slot = 10 -> global_bin = 16384 + 10
                int global_bin = block_idx * n + slot;

                // 마지막 block에서 m이 딱 나누어떨어지지 않을 수 있음.
                // 그 경우 실제 bin이 없는 slot은 dummy로 채움.
                if (global_bin >= m) {
                    pod_powers[slot] = RECEIVER_DUMMY;
                    continue;
                }

                uint64_t val = hash_table[global_bin];

                if (val == RECEIVER_DUMMY) {
                    // receiver cuckoo hash table에서 비어 있는 bin은 dummy 유지
                    pod_powers[slot] = RECEIVER_DUMMY;
                } else {
                    // 실제 receiver 값은 e승을 계산해서 넣음.
                    // sender가 나중에 P(y)를 계산할 때 필요한 y^e 값임.
                    pod_powers[slot] = power_mod(val, e, plain_modulus);
                }
            }

            Plaintext plain_power;
            batch_encoder.encode(pod_powers, plain_power);

            Ciphertext encrypted_power;
            encryptor.encrypt(plain_power, encrypted_power);

            block_ciphertexts.push_back(move(encrypted_power));
        }

        result.insert({e, move(block_ciphertexts)});

        cout << "  - Generated encrypted ciphertexts for exponent: "
             << e << " / blocks: " << num_blocks << endl;
    }

    return result;
}