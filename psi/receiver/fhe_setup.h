#pragma once

#include "seal/seal.h"
#include <memory>
#include <stdexcept> // std::runtime_error 사용을 위해 필요
#include <string>   // std::string 사용을 위해 필요
#include <fstream>

class FHESetup {
public:
    // SEAL 핵심 객체들을 저장할 멤버 변수
    std::shared_ptr<seal::SEALContext> context;
    seal::PublicKey public_key;
    seal::SecretKey secret_key;
    seal::RelinKeys relin_keys;
    std::unique_ptr<seal::Decryptor> decryptor;

    // unique_ptr을 사용하여 setup 호출 시 객체 생성
    std::unique_ptr<seal::Encryptor> encryptor;
    std::unique_ptr<seal::Evaluator> evaluator;
    std::unique_ptr<seal::BatchEncoder> batch_encoder;

    /**
     * @brief Microsoft SEAL 환경 설정을 수행함
     * @param n poly_modulus_degree
     * @param t_bits plain_modulus의 비트 크기
     */
    void setup(size_t n, int t_bits);
    void setup_for_response(size_t n, int t_bits, const std::string& sk_path);
};