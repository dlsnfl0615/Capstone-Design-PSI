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

map<int, Ciphertext> Windowing::receiver_windowing(
    BatchEncoder& batch_encoder,
    Encryptor& encryptor,
    const uint64_t plain_modulus,
    const vector<int> exponents,
    const vector<uint64_t> hash_table) {

    map<int, Ciphertext> result;

    for (int e : exponents) {
        vector<uint64_t> pod_powers(n, 0); 

        for (size_t k = 0; k < m; k++) {
            uint64_t val = hash_table[k]; 
            
            if (val == RECEIVER_DUMMY) {
                pod_powers[k] = RECEIVER_DUMMY; 
            } else {
                // [수정된 부분] 오버플로우 방지 거듭제곱 함수 적용함
                pod_powers[k] = power_mod(val, e, plain_modulus); 
            }
        }

        Plaintext plain_power;
        batch_encoder.encode(pod_powers, plain_power);

        Ciphertext encrypted_power;
        encryptor.encrypt(plain_power, encrypted_power);

        result.insert({e, encrypted_power});
        cout << "  - Generated encrypted ciphertext for exponent: " << e << endl;
    }

    return result;
}