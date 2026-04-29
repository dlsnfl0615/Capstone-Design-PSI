#include "windowing.h"

vector<int> Windowing::get_exponents() {
    vector<int> result;

    int exponent_max = B / alpha;
    int j_max = static_cast<int>(floor(log2(ceil(B/alpha)) / l));
    int i_max = (1 << l) - 1;

    cout << "[windowing] j_max: " << j_max << ", i_max: " << i_max << endl;

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

vector<Ciphertext> Windowing::receiver_windowing(
    BatchEncoder& batch_encoder,
    Encryptor& encryptor,
    const uint64_t plain_modulus,
    const vector<int> exponents,
    const vector<uint64_t> hash_table) {
    vector<Ciphertext> result;

    for (int e : exponents) {
        // 평문 공간에 담을 벡터 준비 (차수 n=32768)
        vector<uint64_t> pod_powers(n, 0); 

        for (size_t k = 0; k < m; k++) {
            uint64_t val = hash_table[k]; // 해시 테이블에서 패킹된 값 추출

            if (val == RECEIVER_DUMMY) {
                // 더미 데이터일 경우 연산 생략하고 더미 값 유지
                pod_powers[k] = RECEIVER_DUMMY; 
            } else {
                // 평문 상태에서 거듭제곱 계산: (val^e) mod t
                // t=44이므로 평문 공간 내에서 연산이 이루어짐
                uint64_t res = 1;
                uint64_t base = val % plain_modulus;
                int temp_e = e;
                while (temp_e > 0) {
                    if (temp_e % 2 == 1) res = (res * base) % plain_modulus;
                    base = (base * base) % plain_modulus;
                    temp_e /= 2;
                }
                pod_powers[k] = res; // 계산된 슬롯 값 저장
            }
        }

        Plaintext plain_power;
        batch_encoder.encode(pod_powers, plain_power);

        Ciphertext encrypted_power;
        encryptor.encrypt(plain_power, encrypted_power);

        result.push_back(encrypted_power);
        cout << "  - Generated encrypted ciphertext for exponent: " << e << endl;
    }

    return result;
}