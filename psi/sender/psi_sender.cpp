#include "psi_sender.h"

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

void PsiSender::load_receiver_powers(const string& filename, seal::SEALContext& context) {
    ifstream in(filename, ios::binary);
    if (!in.is_open()) throw runtime_error("Cannot open receiver's power file: " + filename);

    uint64_t size;
    in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));

    for (uint64_t i = 0; i < size; i++) {
        uint64_t exponent;
        in.read(reinterpret_cast<char*>(&exponent), sizeof(uint64_t));
        
        seal::Ciphertext ct;
        ct.load(context, in);
        received_powers[exponent] = move(ct);
    }
    in.close();
    cout << "Sender: Loaded " << size << " ciphertexts from receiver." << endl;
}

void PsiSender::partition_bins(const vector<vector<uint64_t>>& hash_table) {
    // 1. 개별 파티션의 크기 d 계산 (B=74, alpha=32일 때 d=3)
    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    
    // 3차원 벡터 초기화: [alpha][m][d]
    partitioned_tables.assign(alpha, vector<vector<uint64_t>>(m, vector<uint64_t>(d, SENDER_DUMMY)));

    cout << "Sender: Partitioning bins with d = " << d << "..." << endl;

    for (int i = 0; i < m; i++) { // 각 빈을 순회
        for (int k = 0; k < alpha; k++) { // 각 파티션을 순회
            for (int j = 0; j < d; j++) { // 파티션 내의 슬롯을 순회
                int original_idx = k * d + j;
                
                // 원본 빈(B=74)의 범위를 벗어나지 않는 경우에만 복사
                if (original_idx < B) {
                    partitioned_tables[k][i][j] = hash_table[i][original_idx];
                } else {
                    // 범위를 벗어나면 자동으로 SENDER_DUMMY 유지
                    partitioned_tables[k][i][j] = SENDER_DUMMY;
                }
            }
        }
    }
    cout << "Sender: Partitioning complete. Created " << alpha << " sub-tables." << endl;
}

void PsiSender::compute_coefficients(seal::BatchEncoder& encoder, uint64_t plain_modulus) {
    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    size_t slot_count = encoder.slot_count();
    batched_coeffs.assign(alpha, vector<seal::Plaintext>(d + 1));

    for (int k = 0; k < alpha; k++) {
        // [수정] 0번 차수(상수항)를 1로 초기화하여 사용하지 않는 슬롯이 0이 되지 않게 함
        vector<vector<uint64_t>> all_coeffs_by_degree(d + 1, vector<uint64_t>(slot_count, 0));
        fill(all_coeffs_by_degree[0].begin(), all_coeffs_by_degree[0].end(), 1); 

        for (int i = 0; i < m; i++) {
            vector<uint64_t> current_poly = { 1 };
            for (int j = 0; j < d; j++) {
                uint64_t root = partitioned_tables[k][i][j];
                vector<uint64_t> next_poly(current_poly.size() + 1, 0);
                for (size_t p = 0; p < current_poly.size(); p++) {
                    next_poly[p + 1] = (next_poly[p + 1] + current_poly[p]) % plain_modulus;
                    uint64_t neg_root = (plain_modulus - (root % plain_modulus)) % plain_modulus;
                    
                    // [필독] 오버플로우 방지를 위해 safe_mul_mod를 반드시 사용해야 함
                    uint64_t term = safe_mul_mod(current_poly[p], neg_root, plain_modulus); 
                    next_poly[p] = (next_poly[p] + term) % plain_modulus;
                }
                current_poly = next_poly;
            }
            for (int deg = 0; deg <= d; deg++) {
                all_coeffs_by_degree[deg][i] = current_poly[deg];
            }
        }
        for (int deg = 0; deg <= d; deg++) {
            encoder.encode(all_coeffs_by_degree[deg], batched_coeffs[k][deg]);
        }
    }
}

void PsiSender::reconstruct_all_powers(seal::Evaluator& evaluator, seal::RelinKeys& relin_keys) {
    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    int base = (1 << l);

    all_powers = received_powers; // 수신자가 보낸 기저 복사 [cite: 24]

    for (int k = 1; k <= d; k++) {
        if (all_powers.find(k) != all_powers.end()) continue;

        seal::Ciphertext combined;
        bool first = true;
        int temp_k = k;
        int j = 0;

        // 지수 k를 윈도우 기저의 합으로 분해하여 동형 곱셈 수행 [cite: 24]
        while (temp_k > 0) {
            int i = temp_k % base;
            if (i > 0) {
                uint64_t part_exp = static_cast<uint64_t>(i) * (1ULL << (l * j));
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
        all_powers[k] = std::move(combined);
    }
}

void PsiSender::evaluate_polynomials(seal::Evaluator& evaluator, seal::RelinKeys& relin_keys) {
    reconstruct_all_powers(evaluator, relin_keys); // 연산 전 지수 재구성 [cite: 5]

    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    evaluation_results.assign(alpha, seal::Ciphertext());

    for (int k = 0; k < alpha; k++) {
        seal::Ciphertext current_res;
        evaluator.multiply_plain(all_powers[1], batched_coeffs[k][1], current_res);

        for (int j = 2; j <= d; j++) {
            seal::Ciphertext temp;
            evaluator.multiply_plain(all_powers[j], batched_coeffs[k][j], temp);
            evaluator.add_inplace(current_res, temp);
        }
        evaluator.add_plain_inplace(current_res, batched_coeffs[k][0]);
        evaluation_results[k] = std::move(current_res);
    }
}

void PsiSender::randomize_and_save_responses(
    seal::Evaluator& evaluator, 
    seal::BatchEncoder& encoder, 
    uint64_t plain_modulus, 
    const string& filename) {
    
    cout << "Sender: Randomizing and saving responses..." << endl;

    // 1. 무작위 마스크(r) 생성
    // 0이 아닌 값을 곱해야 교집합(0)이 유지되므로 1 ~ (t-1) 사이의 값을 선택함
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<uint64_t> dist(1, plain_modulus - 1);

    vector<uint64_t> random_vec(encoder.slot_count());
    for (size_t i = 0; i < encoder.slot_count(); i++) {
        random_vec[i] = dist(gen);
    }

    seal::Plaintext plain_mask;
    encoder.encode(random_vec, plain_mask);

    // 2. 각 파티션 결과에 마스크 곱하기: r * P(y)
    for (int k = 0; k < alpha; k++) {
        evaluator.multiply_plain_inplace(evaluation_results[k], plain_mask);
    }

    // 3. 파일로 직렬화하여 저장 (수신자의 load_responses와 대응)
    ofstream out(filename, ios::binary);
    if (!out.is_open()) throw runtime_error("fail to create response file");

    // 응답 개수(alpha) 기록
    uint64_t response_count = static_cast<uint64_t>(alpha);
    out.write(reinterpret_cast<const char*>(&response_count), sizeof(uint64_t));

    for (int k = 0; k < alpha; k++) {
        evaluation_results[k].save(out);
    }

    out.close();
    cout << "Sender: " << alpha << " randomized responses saved to " << filename << endl;
}
