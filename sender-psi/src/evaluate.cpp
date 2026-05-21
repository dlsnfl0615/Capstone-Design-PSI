#include "evaluate.h"
#include <stdexcept>
#include <algorithm>
#include <omp.h> // OpenMP 헤더 추가

vector<vector<vector<uint64_t>>> SenderEvaluate::partitioning(
    const vector<vector<uint64_t>>& hash_table
) {
    vector<vector<vector<uint64_t>>> partitions(alpha, vector<vector<uint64_t>>(B_prime, vector<uint64_t>(m, SENDER_DUMMY))); 

    for (int p = 0; p < alpha; p++) {
        for (int row = 0; row < B_prime; row++) {
            int original_row = p * B_prime + row;
            if (original_row < B) {
                for (int col = 0; col < m; col++) {
                    // 원본의 c번째 빈, original_row번째 슬롯의 값을 복사함
                    partitions[p][row][col] = hash_table[col][original_row]; 
                }
            }
        }
    }
    return partitions;
}

// 단일 열(Bin)의 근들을 사용하여 다항식 계수를 계산함
vector<uint64_t> SenderEvaluate::compute_bin_coeffs(const vector<uint64_t>& roots, uint64_t plain_modulus) {
    int deg = roots.size(); 
    vector<uint64_t> coeffs(deg + 1, 0);
    coeffs[0] = 1; // 최고차항 계수는 1로 시작함

    for (int i = 0; i < deg; i++) {
        uint64_t root = roots[i] % plain_modulus;
        for (int j = i + 1; j > 0; j--) {
            uint64_t neg_root = plain_modulus - root;
            
            // [수정된 부분] safe_mul_mod를 사용하여 계수 전개 시 오버플로우를 방지함
            uint64_t term = safe_mul_mod(coeffs[j - 1], neg_root, plain_modulus);
            coeffs[j] = (coeffs[j] + term) % plain_modulus;
        }
    }
    
    // 난수 r을 곱하여 최종 계수를 난수화함
    uint64_t r = seed % plain_modulus;
    if (r == 0) r = 1; // 난수가 0이 되는 것을 방지함

    for (uint64_t& c : coeffs) {
        // [수정된 부분] 난수 곱셈 시에도 safe_mul_mod를 적용하여 오버플로우를 차단함
        c = safe_mul_mod(c, r, plain_modulus);
    }
    
    return coeffs;
}

vector<vector<vector<uint64_t>>> SenderEvaluate::extract_all_coefficients(
    const vector<vector<vector<uint64_t>>>& partitions,
    uint64_t plain_modulus) {

    cout << "[debug][coeff] function entered" << endl;
    cout << "[debug][coeff] alpha = " << alpha
         << ", B_prime = " << B_prime
         << ", m = " << m << endl;

    vector<vector<vector<uint64_t>>> coeff_tables(alpha);
    coeff_tables.reserve(alpha);

    // 모든 row가 SENDER_DUMMY인 bin의 계수는 항상 같으므로 한 번만 계산
    vector<uint64_t> dummy_roots(B_prime, SENDER_DUMMY);
    vector<uint64_t> dummy_coeffs = compute_bin_coeffs(dummy_roots, plain_modulus);

    // 파티션 루프 멀티스레드로 분할 처리
    #pragma omp parallel for schedule(dynamic)

    for (int p = 0; p < alpha; p++) {
        cout << "[debug][coeff] partition " << p << " start" << endl;

        vector<vector<uint64_t>> partition_coeffs(
            B_prime + 1,
            vector<uint64_t>(m, 0)
        );

        // 일단 모든 col을 dummy 계수로 채움
        for (int d = 0; d <= B_prime; d++) {
            uint64_t default_coeff = dummy_coeffs[B_prime - d];
            fill(partition_coeffs[d].begin(), partition_coeffs[d].end(), default_coeff);
        }

        size_t real_col_count = 0;

        for (int col = 0; col < m; col++) {
            bool has_real_value = false;

            for (int row = 0; row < B_prime; row++) {
                if (partitions[p][row][col] != SENDER_DUMMY) {
                    has_real_value = true;
                    break;
                }
            }

            // 전부 dummy면 이미 기본 계수로 채웠으므로 계산 생략
            if (!has_real_value) {
                continue;
            }

            real_col_count++;

            vector<uint64_t> roots;
            roots.reserve(B_prime);

            for (int row = 0; row < B_prime; row++) {
                roots.push_back(partitions[p][row][col]);
            }

            vector<uint64_t> bin_coeffs = compute_bin_coeffs(roots, plain_modulus);

            for (int d = 0; d <= B_prime; d++) {
                partition_coeffs[d][col] = bin_coeffs[B_prime - d];
            }
        }

        cout << "[debug][coeff] partition " << p
             << " end, real cols = " << real_col_count << endl;

        #기존 push_back 대신 인덱스 p에 다이렉트로 대입 -> 스레드 안전, 순서 섞이지 x
        coeff_tables[p] = move(partition_coeffs);
    }

    cout << "[debug][coeff] function end" << endl;

    return coeff_tables;
}

map<int, vector<Ciphertext>> SenderEvaluate::make_all_powers(
    const map<int, vector<Ciphertext>> received_powers,
    Evaluator& evaluator,
    RelinKeys& relin_keys) {

    // all_powers[k][b]
    //   k: 지수
    //   b: block 번호
    //
    // 예:
    // all_powers[1][0] = block 0의 y^1 암호문
    // all_powers[1][1] = block 1의 y^1 암호문
    // all_powers[17][0] = block 0의 y^17 암호문
    // all_powers[17][1] = block 1의 y^17 암호문
    map<int, vector<Ciphertext>> all_powers;

    int d = static_cast<int>(ceil(static_cast<double>(B) / alpha));
    int base = (1 << l);

    // receiver가 보내준 windowing 기본 powers 복사
    // 예: 1, 2, 4, 8, 16, 32 같은 기저 powers
    all_powers = received_powers;

    for (int k = 1; k <= d; k++) {

        // 이미 receiver가 보내준 power라면 새로 조합할 필요 없음.
        // 예: receiver가 16승을 보내줬다면 16은 skip.
        if (all_powers.find(k) != all_powers.end()) {
            continue;
        }

        cout << "degree to combine: " << k << endl;

        vector<Ciphertext> combined_blocks;

        // 기존에는 k승 암호문 하나만 만들면 됐음.
        // 이제는 block별로 k승 암호문을 만들어야 함.
        //
        // 이유:
        // block 0의 y값들과 block 1의 y값들은 서로 다른 ciphertext에 들어 있음.
        // 따라서 y^k도 block별로 따로 계산해야 함.
        for (int block_idx = 0; block_idx < num_blocks; block_idx++) {
            Ciphertext combined;
            bool first = true;

            int temp_k = k;
            int j = 0;
            int count = 0;

            // k를 windowing 기저들의 합으로 분해함.
            //
            // 예:
            // l = 1이면 base = 2
            // k = 17 = 16 + 1
            //
            // 그러면 all_powers[16][block_idx]와
            // all_powers[1][block_idx]를 곱해서
            // all_powers[17][block_idx]를 만듦.
            while (temp_k > 0) {
                int i = temp_k % base;

                if (i > 0) {
                    int part_exp = static_cast<int>(i) * (1ULL << (l * j));

                    // 안전장치:
                    // map의 [] 연산자는 key가 없으면 빈 Ciphertext를 만들어버림.
                    // 그러면 나중에 "encrypted2 is not valid" 같은 애매한 오류가 남.
                    // 따라서 반드시 find/at으로 존재 여부를 확인함.
                    if (all_powers.find(part_exp) == all_powers.end()) {
                        throw runtime_error(
                            "Missing encrypted power for exponent: " +
                            to_string(part_exp) +
                            " while computing degree: " +
                            to_string(k)
                        );
                    }

                    // 해당 exponent에 block ciphertext가 충분히 있는지도 확인함.
                    if (block_idx >= static_cast<int>(all_powers.at(part_exp).size())) {
                        throw runtime_error(
                            "Missing block " + to_string(block_idx) +
                            " for exponent: " + to_string(part_exp)
                        );
                    }

                    if (first) {
                        combined = all_powers.at(part_exp)[block_idx];
                        first = false;
                    } else {
                        evaluator.multiply_inplace(
                            combined,
                            all_powers.at(part_exp)[block_idx]
                        );
                        evaluator.relinearize_inplace(combined, relin_keys);
                        count++;
                    }
                }

                temp_k /= base;
                j++;
            }

            combined_blocks.push_back(move(combined));

            cout << "  block " << block_idx
                 << " multiplication times: " << count << endl;
        }

        all_powers[k] = move(combined_blocks);
    }

    cout << endl;
    return all_powers;
}

vector<Ciphertext> SenderEvaluate::product(
    const map<int, vector<Ciphertext>>& all_powers,
    const vector<vector<vector<uint64_t>>>& coeffs,
    BatchEncoder& batch_encoder,
    Evaluator& evaluator,
    SEALContext& context) {

    vector<Ciphertext> result;

    // coeffs[partition_idx][row][global_bin]
    //
    // partition_idx: partition 번호
    // row: 다항식 차수에 해당하는 row
    // global_bin: 전체 hash table bin 번호, 0 ~ m-1
    //
    // 기존에는 global_bin 전체를 한 plaintext에 넣었음.
    // 이제는 global_bin을 block 단위로 쪼개서
    // block 하나당 n개 slot만 encode함.
    for (int partition_idx = 0; partition_idx < static_cast<int>(coeffs.size()); partition_idx++) {

        for (int block_idx = 0; block_idx < num_blocks; block_idx++) {

            Ciphertext partition_block_sum;
            bool is_first_term = true;

            for (int row = 0; row < static_cast<int>(coeffs[partition_idx].size()); row++) {

                // 한 ciphertext에 들어갈 계수 벡터
                // 크기는 반드시 n.
                //
                // 이유:
                // BatchEncoder는 한 번에 n개 slot만 담는 구조이므로,
                // m > n일 때도 row_vector(m)을 만들면 안 됨.
                vector<uint64_t> row_vector(n, 0);

                for (int slot = 0; slot < n; slot++) {
                    // block 내부 slot 번호를 전체 bin 번호로 변환
                    int global_bin = block_idx * n + slot;

                    if (global_bin < m) {
                        row_vector[slot] = coeffs[partition_idx][row][global_bin];
                    } else {
                        // 마지막 block에서 남는 slot은 0으로 채움.
                        // 없는 bin에 대한 계수이므로 다항식 평가에 영향이 없어야 함.
                        row_vector[slot] = 0;
                    }
                }

                Plaintext plain_coeff;
                batch_encoder.encode(row_vector, plain_coeff);

                Ciphertext temp_product;

                // all_powers[row][block_idx]는
                // 현재 block에 대한 y^row 암호문.
                //
                // plain_coeff는
                // 현재 block에 대한 row차 계수 벡터.
                //
                // 따라서 두 개를 곱하면:
                //   coeff_row * y^row
                // 를 block 단위로 계산한 결과가 됨.
                evaluator.multiply_plain(
                    all_powers.at(row)[block_idx],
                    plain_coeff,
                    temp_product
                );

                if (is_first_term) {
                    partition_block_sum = temp_product;
                    is_first_term = false;
                } else {
                    evaluator.add_inplace(partition_block_sum, temp_product);
                }
            }

            // 결과 저장 순서:
            //   partition 0, block 0
            //   partition 0, block 1
            //   partition 1, block 0
            //   partition 1, block 1
            //
            // receiver-result.cpp에서 idx % num_blocks로 block_idx를 복원함.
            result.push_back(move(partition_block_sum));

            cout << "[product] partition " << partition_idx
                 << ", block " << block_idx << " done" << endl;
        }
    }

    return result;
}
