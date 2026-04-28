#include "psi_receiver.h"
#include "../parameters.h"

// 64비트 범위 내에서 (a * b) % m 오버플로우 방지 곱셈 함수 
uint64_t safe_mul_mod(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t res = 0;
    a %= m;
    while (b > 0) {
        if (b % 2 == 1) res = (res + a) % m;
        a = (a + a) % m;
        b /= 2;
    }
    return res;
}

// 오버플로우 방지 거듭제곱 함수 
uint64_t power_mod(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t res = 1;
    base %= mod;
    while (exp > 0) {
        if (exp % 2 == 1) res = safe_mul_mod(res, base, mod);
        base = safe_mul_mod(base, base, mod);
        exp /= 2;
    }
    return res;
}

std::map<uint64_t, seal::Ciphertext> PsiReceiver::generate_windowed_powers(
    const std::vector<uint64_t>& original_batched_data, 
    int l, 
    int max_degree, 
    seal::BatchEncoder& encoder,
    seal::Encryptor& encryptor,
    uint64_t plain_modulus) {
    
    std::map<uint64_t, seal::Ciphertext> windowed_powers;
    int num_j = static_cast<int>(floor(log2(max_degree) / l)) + 1;
    int max_i = (1 << l) - 1;

    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            uint64_t exponent = static_cast<uint64_t>(i) * (1ULL << (l * j));
            if (exponent > static_cast<uint64_t>(max_degree)) break;

            // 평문 상태에서 거듭제곱 계산 후 개별 암호화하여 노이즈 절약 
            std::vector<uint64_t> power_vec(original_batched_data.size());
            for (size_t s = 0; s < original_batched_data.size(); s++) {
                power_vec[s] = power_mod(original_batched_data[s], exponent, plain_modulus);
            }

            seal::Plaintext plain_power;
            encoder.encode(power_vec, plain_power);
            
            seal::Ciphertext encrypted_power;
            encryptor.encrypt(plain_power, encrypted_power);

            // [최적화 제외] mod_switch_to_next_inplace(encrypted_power, evaluator); // 검증을 위해 주석 처리

            windowed_powers[exponent] = std::move(encrypted_power);
        }
    }
    return windowed_powers;
}

vector<int> PsiReceiver::identify_intersection(
    const vector<seal::Ciphertext>& responses, 
    seal::Decryptor& decryptor, 
    seal::BatchEncoder& encoder) {
        
    
    // 1. 모든 슬롯의 상태를 관리하기 위한 벡터 (기본값 true: 교집합 가능성 있음)
    // alpha개의 결과 중 하나라도 0이 아니면(non-zero) 교집합이 아님
    // 하지만 논문 구조상 각 파티션은 독립적인 subset이므로, 
    // 특정 슬롯 i에 대해 alpha개 중 단 하나라도 0이 나오면 교집합임
    vector<bool> is_intersection(m, false);

    for (const auto& response : responses) {
        seal::Plaintext plain;
        decryptor.decrypt(response, plain);

        vector<uint64_t> decoded;
        encoder.decode(plain, decoded);

        for (int i = 0; i < m; i++) {
            // 어느 한 파티션에서라도 결과가 0이면 교집합으로 확정함
            if (decoded[i] == 0) {
                is_intersection[i] = true;
            }
        }
    }

    // 2. 최종 교집합 인덱스 추출
    vector<int> intersection_indices;
    for (int i = 0; i < m; i++) {
        if (is_intersection[i]) {
            intersection_indices.push_back(i);
        }
    }
    
    return intersection_indices;
}

void PsiReceiver::save_powers(const map<uint64_t, seal::Ciphertext>& powers, const string& filename) {
    ofstream out(filename, ios::binary); // 바이너리 모드로 파일 열기
    if (!out.is_open()) throw runtime_error("fail to create power file");

    // 1. 저장할 암호문 개수 기록
    uint64_t size = static_cast<uint64_t>(powers.size());
    out.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));

    for (auto const& [exponent, ciphertext] : powers) {
        // 2. 지수(Key) 기록
        out.write(reinterpret_cast<const char*>(&exponent), sizeof(uint64_t));
        // 3. 암호문(Value) 직렬화
        ciphertext.save(out); 
    }
    out.close();
    cout << "Receiver: " << size << " powers saved to " << filename << endl;
}

vector<seal::Ciphertext> PsiReceiver::load_responses(const string& filename, seal::SEALContext& context) {
    ifstream in(filename, ios::binary);
    if (!in.is_open()) throw runtime_error("fail to open response file");

    // 1. 응답 개수(alpha) 읽기
    uint64_t size;
    in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));

    vector<seal::Ciphertext> responses(size);
    for (uint64_t i = 0; i < size; i++) {
        // 2. 암호문 역직렬화 (context 정보가 반드시 필요함)
        responses[i].load(context, in);
    }
    in.close();
    cout << "Receiver: Loaded " << size << " response ciphertexts." << endl;
    return responses;
}