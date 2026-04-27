#include "psi_receiver.h"
#include "../parameters.h"

map<uint64_t, seal::Ciphertext> PsiReceiver::generate_windowed_powers(
    const seal::SEALContext& context, // 인자 추가
    const seal::Ciphertext& encrypted_table, 
    int l, 
    int max_degree, 
    seal::Evaluator& evaluator, 
    seal::RelinKeys& relin_keys) {
    
    map<uint64_t, seal::Ciphertext> windowed_powers;
    int num_j = static_cast<int>(floor(log2(max_degree) / l)) + 1;
    int max_i = (1 << l) - 1;

    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            uint64_t exponent = static_cast<uint64_t>(i) * (1ULL << (l * j));
            if (exponent > static_cast<uint64_t>(max_degree)) break;

            seal::Ciphertext power;
            if (exponent == 1) {
                power = encrypted_table;
                // y^1도 다른 지수들과 레벨을 맞추기 위해 스위칭 수행
                evaluator.mod_switch_to_next_inplace(power); 
            } else {
                evaluator.exponentiate(encrypted_table, exponent, relin_keys, power);
                evaluator.mod_switch_to_next_inplace(power);
            }
            windowed_powers[exponent] = move(power);
            cout << "Receiver: y^" << exponent << " generated and mod-switched." << endl;
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