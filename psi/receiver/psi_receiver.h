#pragma once

#include <iostream>
#include <vector>
#include "seal/seal.h"
using namespace std;

class PsiReceiver {
public:
    vector<uint64_t> exponents;

    map<uint64_t, seal::Ciphertext> PsiReceiver::generate_windowed_powers(
        const seal::Ciphertext& encrypted_table, 
        int l, 
        int max_degree, 
        seal::Evaluator& evaluator, 
        seal::RelinKeys& relin_keys)

    // 송신자의 결과 암호문을 복호화하여 교집합 인덱스 추출
    vector<int> identify_intersection(
        const seal::Ciphertext& response, 
        seal::Decryptor& decryptor, 
        seal::BatchEncoder& encoder);
};