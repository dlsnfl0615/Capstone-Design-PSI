#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <fstream> // 파일 스트림 처리를 위해 필요
#include "seal/seal.h"
using namespace std;

class PsiReceiver {
public:
    vector<uint64_t> exponents;

    std::map<uint64_t, seal::Ciphertext> generate_windowed_powers(
        const std::vector<uint64_t>& original_batched_data, 
        int l, 
        int max_degree, 
        seal::BatchEncoder& encoder,
        seal::Encryptor& encryptor,
        uint64_t plain_modulus);

    // 수정: 송신자로부터 받은 alpha개의 응답 암호문을 모두 처리함
    vector<int> identify_intersection(
        const vector<seal::Ciphertext>& responses, 
        seal::Decryptor& decryptor, 
        seal::BatchEncoder& encoder);

    // 생성된 거듭제곱 조각들을 파일로 저장 (직렬화)
    void save_powers(const map<uint64_t, seal::Ciphertext>& powers, const string& filename);

    // 송신자로부터 받은 응답 암호문들을 파일에서 로드 (역직렬화)
    vector<seal::Ciphertext> load_responses(const string& filename, seal::SEALContext& context);
};