#include "evaluate.h"

vector<vector<vector<uint64_t>>> SenderEvaluate::partitioning(const vector<vector<uint64_t>> hash_table) {
    vector<vector<vector<uint64_t>>> partitions;
    vector<vector<uint64_t>> temp = hash_table;
    int partition_len = ceil(B / alpha);
    
    if (temp.size() < partition_len * alpha) {
        temp.resize(partition_len * alpha, vector<uint64_t>(m, SENDER_DUMMY));
    }

    for (int i = 0; i < alpha; ++i) {
        auto start = temp.begin() + (i * partition_len);
        auto end = start + partition_len;
        
        // 각 블록은 10x8192 크기를 가짐
        partitions.emplace_back(start, end);
    }

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

// 2. 3차원 파티션 배열을 순회하며 계수만 추출함
// 입력: partitioned[alpha][B_prime][m][cite: 21]
// 출력: coeff_tables[alpha][B_prime + 1][m] (배칭에 최적화된 구조)[cite: 1]
vector<vector<vector<uint64_t>>> SenderEvaluate::extract_all_coefficients(
    const vector<vector<vector<uint64_t>>>& partitions, 
    uint64_t plain_modulus
) {
    int B_prime = B / alpha; // 각 파티션의 행(아이템) 개수[cite: 11, 24]
    vector<vector<vector<uint64_t>>> coeff_tables;

    for (int p = 0; p < alpha; p++) {
        // [차수][열] 구조의 테이블 생성 (배칭 시 행 단위로 읽기 위함)[cite: 1]
        vector<vector<uint64_t>> partition_coeffs(B_prime + 1, vector<uint64_t>(m, 0));

        for (int col = 0; col < m; col++) {
            // 해당 열(Bin)의 모든 행 데이터를 근(Roots)으로 수집함[cite: 1]
            vector<uint64_t> roots;
            for (int row = 0; row < B_prime; row++) {
                roots.push_back(partitions[p][row][col]);
            }

            // 다항식 전개 수행[cite: 1]
            vector<uint64_t> bin_coeffs = compute_bin_coefficients(roots, plain_modulus);

            // 계산된 계수들을 차수별로 테이블에 채움[cite: 1]
            for (int d = 0; d <= B_prime; d++) {
                // bin_coeffs[0]은 B_prime차항, bin_coeffs[B_prime]은 0차항임
                partition_coeffs[d][col] = bin_coeffs[B_prime - d];
            }
        }
        coeff_tables.push_back(partition_coeffs);
        cout << "[Sender] Partition " << p << " coefficients extracted." << endl;
    }
    return coeff_tables;
}