#include "psi_receiver.h"
#include "../parameters.h"

map<uint64_t, seal::Ciphertext> PsiReceiver::generate_windowed_powers(
    const seal::Ciphertext& encrypted_table, 
    int l, 
    int max_degree, 
    seal::Evaluator& evaluator, 
    seal::RelinKeys& relin_keys) {
    
    map<uint64_t, seal::Ciphertext> windowed_powers;

    // j의 범위: 0 ~ floor(log2(max_degree)/l)
    int num_j = static_cast<int>(floor(log2(max_degree) / l)) + 1;
    // i의 범위: 1 ~ 2^l - 1
    int max_i = (1 << l) - 1;

    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            // 지수 계산: i * 2^(l*j)
            uint64_t exponent = static_cast<uint64_t>(i) * (1ULL << (l * j));
            
            if (exponent > static_cast<uint64_t>(max_degree)) break;
            exponents.push_back(exponent);

            seal::Ciphertext power;
            if (exponent == 1) {
                power = encrypted_table;
            } else {
                // y^1을 기반으로 exponent만큼 거듭제곱 계산
                // 노이즈 관리를 위해 relin_keys 사용
                evaluator.exponentiate(encrypted_table, exponent, relin_keys, power);
            }
            
            // 송신자가 찾기 쉽도록 맵에 저장 (지수 -> 암호문)
            windowed_powers[exponent] = move(power);
            
            cout << "Receiver: y^" << exponent << " 생성 및 저장 완료" << endl;
        }
    }

    return windowed_powers;
}

vector<int> PsiReceiver::identify_intersection(
    const seal::Ciphertext& response, 
    seal::Decryptor& decryptor, 
    seal::BatchEncoder& encoder) {
    
    seal::Plaintext plain;
    decryptor.decrypt(response, plain);

    vector<uint64_t> decoded;
    encoder.decode(plain, decoded);

    vector<int> intersection_indices;
    for (int i = 0; i < m; i++) {
        // 결과값이 0이면 교집합으로 판단
        if (decoded[i] == 0) {
            intersection_indices.push_back(i);
        }
    }
    return intersection_indices;
}