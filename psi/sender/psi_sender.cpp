#include "psi_sender.h"

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
    
    // 결과 저장 공간 초기화: [alpha 파티션][d + 1 차수]
    batched_coeffs.assign(alpha, vector<seal::Plaintext>(d + 1));

    cout << "Sender: Computing polynomial coefficients..." << endl;

    for (int k = 0; k < alpha; k++) {
        // 각 차수(0 ~ d)별로 m개의 슬롯 데이터를 임시 저장할 벡터들
        vector<vector<uint64_t>> all_coeffs_by_degree(d + 1, vector<uint64_t>(slot_count, 0));

        for (int i = 0; i < m; i++) {
            // 1. 현재 빈의 아이템들로부터 다항식 (y - x1)(y - x2)...(y - xd) 계산
            // 초기 상태: P(y) = 1 (0차항 계수가 1인 상태)
            vector<uint64_t> current_poly = { 1 };

            for (int j = 0; j < d; j++) {
                uint64_t root = partitioned_tables[k][i][j];
                
                // 더미 아이템인 경우 (y - 1) 등을 곱하는 대신 결과에 영향 없는 값 처리 가능
                // 여기서는 논문 방식대로 모든 루트를 사용하여 d차 다항식을 완성함
                
                vector<uint64_t> next_poly(current_poly.size() + 1, 0);
                for (size_t p = 0; p < current_poly.size(); p++) {
                    // y를 곱함: (y^p * y)
                    next_poly[p + 1] = (next_poly[p + 1] + current_poly[p]) % plain_modulus;
                    
                    // -root를 곱함: (y^p * -root)
                    uint64_t neg_root = (plain_modulus - (root % plain_modulus)) % plain_modulus;
                    uint64_t term = (current_poly[p] * neg_root) % plain_modulus;
                    next_poly[p] = (next_poly[p] + term) % plain_modulus;
                }
                current_poly = next_poly;
            }

            // 2. 계산된 계수들을 차수별 벡터의 해당 빈 위치(i)에 저장
            for (int deg = 0; deg <= d; deg++) {
                all_coeffs_by_degree[deg][i] = current_poly[deg];
            }
        }

        // 3. 각 차수별 벡터를 SEAL Plaintext로 인코딩 (SIMD 배칭)
        for (int deg = 0; deg <= d; deg++) {
            encoder.encode(all_coeffs_by_degree[deg], batched_coeffs[k][deg]);
        }
    }
    cout << "Sender: Coefficient calculation and batching complete." << endl;
}

void PsiSender::evaluate_polynomials(seal::Evaluator& evaluator, seal::RelinKeys& relin_keys) {
    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    evaluation_results.assign(alpha, seal::Ciphertext());

    cout << "Sender: Evaluating polynomials homomorphically..." << endl;

    for (int k = 0; k < alpha; k++) {
        // 1. 상수항 (a_0) 처리
        // a_0는 y^0이므로 암호문 곱셈 없이 평문 덧셈으로 시작함
        seal::Ciphertext current_res;
        evaluator.multiply_plain(received_powers[1], batched_coeffs[k][1], current_res); // a_1 * y^1

        // 2. 나머지 차수 (a_j * y^j) 처리
        for (int j = 2; j <= d; j++) {
            seal::Ciphertext temp;
            // a_j * y^j (Plain-Ciphertext Multiplication)
            evaluator.multiply_plain(received_powers[j], batched_coeffs[k][j], temp);
            // 누적 합
            evaluator.add_inplace(current_res, temp);
        }

        // 3. 마지막에 상수항 a_0 더하기
        evaluator.add_plain_inplace(current_res, batched_coeffs[k][0]);
        
        evaluation_results[k] = move(current_res);
        cout << "Sender: Partition " << k << " evaluation complete." << endl;
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
