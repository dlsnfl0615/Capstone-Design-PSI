#include "evaluate.h"

vector<vector<vector<uint64_t>>> SenderEvaluate::partitioning(const vector<vector<uint64_t>> hash_table) {
    
    int B_prime = static_cast<int>(ceil(static_cast<double>(B) / alpha)); 
    vector<vector<vector<uint64_t>>> partitions(alpha, vector<vector<uint64_t>>(B_prime, vector<uint64_t>(m, SENDER_DUMMY))); 

    for (int p = 0; p < alpha; p++) {
        for (int row = 0; row < B_prime; row++) {
            int original_row = p * B_prime + row;
            if (original_row < B) {
                for (int col = 0; col < m; col++) {
                    // 원본의 c번째 빈, original_row번째 슬롯의 값을 복사함[cite: 1]
                    partitions[p][row][col] = hash_table[col][original_row]; 
                }
            }
        }
    }

    cout << "partition num: " << partitions.size() << ", row: " << partitions[0].size() << ", col: " << partitions[0][0].size() << endl;

    return partitions;
}

// 1. 단일 열(Bin)의 근들을 사용하여 다항식 계수를 계산함
vector<uint64_t> SenderEvaluate::compute_bin_coefficients(const vector<uint64_t>& roots, uint64_t plain_modulus) {
    int deg = roots.size(); // B' 차수[cite: 1, 11]
    vector<uint64_t> coeffs(deg + 1, 0);
    coeffs[0] = 1; // 최고차항 계수는 1로 시작함

    for (int i = 0; i < deg; i++) {
        uint64_t root = roots[i] % plain_modulus;
        for (int j = i + 1; j > 0; j--) {
            // coeffs[j] = coeffs[j] * (-root) + coeffs[j-1] 전개 수행
            uint64_t neg_root = plain_modulus - root;
            uint64_t term = (coeffs[j - 1] * neg_root) % plain_modulus;
            coeffs[j] = (coeffs[j] + term) % plain_modulus;
        }
    }
    
    // 난수 r을 곱하여 최종 계수를 난수화함 (보안성 확보)
    uint64_t r = random; 
    for (uint64_t& c : coeffs) {
        c = (c * r) % plain_modulus;
    }
    return coeffs;
}

vector<vector<vector<uint64_t>>> SenderEvaluate::extract_all_coefficients(
    const vector<vector<vector<uint64_t>>>& partitions, 
    uint64_t plain_modulus) {
        
    int B_prime = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    vector<vector<vector<uint64_t>>> coeff_tables;

    for (int p = 0; p < alpha; p++) {
        // partition_coeffs[차수][빈] 구조
        vector<vector<uint64_t>> partition_coeffs(B_prime + 1, vector<uint64_t>(m, 0));

        for (int col = 0; col < m; col++) {
            vector<uint64_t> roots;
            for (int row = 0; row < B_prime; row++) {
                // 수정된 인덱스: [p][bin][row]
                roots.push_back(partitions[p][row][col]); 
            }

            vector<uint64_t> bin_coeffs = compute_bin_coefficients(roots, plain_modulus);

            for (int d = 0; d <= B_prime; d++) {
                // d차항 계수를 해당 빈(col) 위치에 저장
                partition_coeffs[d][col] = bin_coeffs[B_prime - d];
            }
        }
        coeff_tables.push_back(partition_coeffs);
    }
    return coeff_tables;
}

map<int, Ciphertext> SenderEvaluate::make_all_powers(
    const map<int, Ciphertext> received_powers,
    Evaluator& evaluator,
    RelinKeys& relin_keys) {
    map<int, Ciphertext> all_powers;
    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    int base = (1 << l);

    all_powers = received_powers; // 수신자가 보낸 기저 복사 [cite: 24]

    for (int k = 1; k <= d; k++) {
        if (all_powers.find(k) != all_powers.end()) continue;

        Ciphertext combined;
        bool first = true;
        int temp_k = k;
        int j = 0;

        // 지수 k를 윈도우 기저의 합으로 분해하여 동형 곱셈 수행 [cite: 24]
        while (temp_k > 0) {
            int i = temp_k % base;
            if (i > 0) {
                int part_exp = static_cast<int>(i) * (1ULL << (l * j));
                if (first) {
                    combined = all_powers[part_exp];
                    first = false;
                } else {
                    evaluator.multiply_inplace(combined, all_powers[part_exp]);
                    evaluator.relinearize_inplace(combined, relin_keys);
                }
            }
            temp_k /= base;
            j++;
        }
        all_powers[k] = move(combined);
    }

    return all_powers;
}

vector<Ciphertext> SenderEvaluate::intersect(
    const map<int, Ciphertext> all_powers,
    const vector<vector<vector<uint64_t>>> coeffs,
    BatchEncoder& batch_encoder,
    Evaluator& evaluator,
    SEALContext& context) {

    for (auto iter : all_powers) {
        cout << iter.first << " ";
    }
    cout << endl;
    
    vector<Ciphertext> result;
    for (int partition_idx = 0; partition_idx < coeffs.size(); partition_idx++) {
        Ciphertext partition_sum;
        bool is_first_term = true;

        for (int row = 0; row < coeffs[partition_idx].size(); row++) {
            vector<uint64_t> row_vector(m, 1); // coeffs[partition_idx][row].size() == m. 계수 벡터 열 길이는 m임

            for (int col = 0; col < coeffs[partition_idx][row].size(); col++) {
                row_vector[col] = coeffs[partition_idx][row][col];
            }

            Plaintext plain_coeff;
            batch_encoder.encode(row_vector, plain_coeff);

            Ciphertext temp_product;
            if (all_powers.find(row) == all_powers.end()) {
                cerr << "[Error] Degree " << row << " is missing in all_powers map!" << endl;
                exit(1); // 원인을 알 수 있게 에러 메시지 출력 후 종료함
            }
            evaluator.multiply_plain(all_powers.at(row), plain_coeff, temp_product);

            if (is_first_term) {
                partition_sum = temp_product;
                is_first_term = false;
            } else {
                evaluator.add_inplace(partition_sum, temp_product); //[cite: 1]
            }
        }

        while (context.get_context_data(partition_sum.parms_id())->chain_index() > 0) {
            evaluator.mod_switch_to_next_inplace(partition_sum);
        }

        result.push_back(partition_sum);
    }

    return result;
}
