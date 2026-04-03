#include "fhe_setup.h"
#include <iostream>
#include <stdexcept>

// ─────────────────────────────────────────
// context 생성
// ─────────────────────────────────────────
SEALContext create_context(const FHEParams& params) {
    EncryptionParameters parms(scheme_type::bfv); // BFV 방식으로 진행

    // n 설정: 클수록 보안/용량 올라가지만 느려짐 + main에서 값 받아서 사용
    // 논문 기준: 8192 또는 16384
    parms.set_poly_modulus_degree(params.poly_modulus_degree);

    // coeff_modulus: 보안 수준에 맞게 SEAL이 추천하는 값 사용
    parms.set_coeff_modulus(
        CoeffModulus::BFVDefault(params.poly_modulus_degree)
    );

    // t 설정: main에서 poly_modulus_degree(n)과 입력 비트수(plain_modulus_bit_size)을 batching이 가능한 t 추천
    parms.set_plain_modulus(
    PlainModulus::Batching(params.poly_modulus_degree,
                                 params.plain_modulus_bit_size)
    );

    SEALContext context(parms);

    // 파라미터 유효성 검사
    if (!context.parameters_set()) {
        throw runtime_error("[FHE] Invalid parameters");
    }

    return context;
}

// ─────────────────────────────────────────
// 키 생성
// ─────────────────────────────────────────
void generate_keys(FHEContext& fhe) {
    KeyGenerator keygen(*fhe.context);

    // secret key
    fhe.secret_key = keygen.secret_key();

    // public key
    keygen.create_public_key(fhe.public_key);

    // relin keys: 동형 곱셈 후 암호문 크기 줄이는 데 필요(논문의 evaluator에 해당)
    keygen.create_relin_keys(fhe.relin_keys);
}

// ─────────────────────────────────────────
// 전체 초기화 한번에
// ─────────────────────────────────────────
FHEContext setup_fhe(const FHEParams& params) {
    FHEContext fhe;

    // context 만들기(파라미터 생성)
    fhe.context = make_shared<SEALContext>(
        create_context(params)
    );

    // 키 (key들 생성)
    generate_keys(fhe);

    // encoder / encryptor / decryptor / evaluator
    fhe.encoder   = make_shared<BatchEncoder>(*fhe.context);
    fhe.encryptor  = make_shared<Encryptor>(*fhe.context, fhe.public_key); //공개키 암호화
    fhe.decryptor  = make_shared<Decryptor>(*fhe.context, fhe.secret_key); //비밀키 복호화
    fhe.evaluator  = make_shared<Evaluator>(*fhe.context);
    fhe.slot_count = fhe.encoder->slot_count(); // 한번에 몇개의 데이터를 batch에 담을 지 결정(seal이 정해줌)

    return fhe;
}

// ─────────────────────────────────────────
// encode : input(숫자벡터)를 seal의 plain_text로 바꿔줌
// ─────────────────────────────────────────
Plaintext encode_batch(const vector<uint64_t>& input,
                             const FHEContext& fhe) {
    // 슬롯 수보다 input이 작으면 나머지는 0으로 채움 (SEAL이 자동 처리)
    if (input.size() > fhe.slot_count) {
        throw runtime_error(
            "[FHE] Input size " + to_string(input.size()) +
            " exceeds slot count " + to_string(fhe.slot_count)
        );
    }
    Plaintext plain;
    fhe.encoder->encode(input, plain); 
    return plain;
}

// ─────────────────────────────────────────
// encrypt :plain text를 받아서 암호화
// ─────────────────────────────────────────
Ciphertext encrypt_batch(const Plaintext& plain,
                               const FHEContext& fhe) {
    Ciphertext cipher; //빈 cipher 만들기
    fhe.encryptor->encrypt(plain, cipher); //암호화
    return cipher;
}

// ─────────────────────────────────────────
// decrypt : cipher를 복호화해서 plain으로 복호화
// ─────────────────────────────────────────
Plaintext decrypt_batch(const Ciphertext& cipher,
                              const FHEContext& fhe) {
    Plaintext plain; // 빈 plain 생성
    fhe.decryptor->decrypt(cipher, plain); //복호화
    return plain;
}

// decrypt로 복호화 한 plain을 우리가 읽을 수 있는 숫자벡터로 변환
vector<uint64_t> decrypt_decode(const Ciphertext& cipher,
                                     const FHEContext& fhe) {
    Plaintext plain = decrypt_batch(cipher, fhe);
    vector<uint64_t> result;
    fhe.encoder->decode(plain, result);
    return result;
}

// ─────────────────────────────────────────
// 파라미터 출력
// ─────────────────────────────────────────
void print_parameters(const FHEContext& fhe) {
    auto& parms = fhe.context->first_context_data()->parms();
    cout << "[FHE Parameters]\n";
    cout << "  scheme     : BFV\n";
    cout << "  poly_modulus_degree (n) : " << parms.poly_modulus_degree() << "\n";
    cout << "  plain_modulus (t)       : " << parms.plain_modulus().value() << "\n";
    cout << "  slot_count              : " << fhe.slot_count << "\n";
}

// ─────────────────────────────────────────
// round-trip 테스트
// ─────────────────────────────────────────
bool test_roundtrip(const FHEContext& fhe) {
    // 슬롯을 1,2,3,...으로 채워서 테스트
    vector<uint64_t> input(fhe.slot_count, 0);
    for (size_t i = 0; i < min((size_t)10, fhe.slot_count); i++)
        input[i] = i + 1; // 1,2,3,...,10

    // encode → encrypt → decrypt → decode
    auto plain1  = encode_batch(input, fhe);
    auto cipher  = encrypt_batch(plain1, fhe);
    auto decoded = decrypt_decode(cipher, fhe);

    // 앞 10개만 비교
    bool ok = true;
    for (size_t i = 0; i < 10; i++) {
        if (decoded[i] != input[i]) {
            ok = false;
            cerr << "[roundtrip FAIL] slot " << i
                      << ": expected " << input[i]
                      << ", got " << decoded[i] << "\n";
        }
    }
    if (ok) cout << "[roundtrip OK] encode->encrypt->decrypt->decode passed\n";
    return ok;
}

// ─────────────────────────────────────────
// 입력 벡터 round-trip 테스트
// ─────────────────────────────────────────
bool test_roundtrip_with_input(const vector<uint64_t>& raw_input,
                               const FHEContext& fhe,
                               const string& label) {
    // slot_count보다 작으면 0 padding
    vector<uint64_t> input(fhe.slot_count, 0);

    size_t count = min(raw_input.size(), fhe.slot_count);
    for (size_t i = 0; i < count; i++) {
        input[i] = raw_input[i];
    }

    // encode → encrypt → decrypt → decode
    auto plain   = encode_batch(input, fhe);
    auto cipher  = encrypt_batch(plain, fhe);
    auto decoded = decrypt_decode(cipher, fhe);

    // 실제 넣은 count개만 비교
    bool ok = true;
    for (size_t i = 0; i < count; i++) {
        if (decoded[i] != input[i]) {
            ok = false;
            cerr << "[" << label << " FAIL] slot " << i
                      << ": expected " << input[i]
                      << ", got " << decoded[i] << "\n";
        }
    }

    if (ok) {
        cout << "[" << label << " OK] encode->encrypt->decrypt->decode passed ("
                  << count << " items checked)\n";
    }
    return ok;
}

//전체비교
bool test_roundtrip_all_inputs(const vector<uint64_t>& raw_input,
                               const FHEContext& fhe,
                               const string& label) {
    bool ok = true;
    size_t total_checked = 0;

    for (size_t offset = 0; offset < raw_input.size(); offset += fhe.slot_count) {
        size_t count = min(fhe.slot_count, raw_input.size() - offset);

        // 이번 batch만큼 복사, 나머지는 0 padding
        vector<uint64_t> batch_input(fhe.slot_count, 0);
        for (size_t i = 0; i < count; i++) {
            batch_input[i] = raw_input[offset + i];
        }

        // encode → encrypt → decrypt → decode
        auto plain   = encode_batch(batch_input, fhe);
        auto cipher  = encrypt_batch(plain, fhe);
        auto decoded = decrypt_decode(cipher, fhe);

        // 실제 데이터가 들어간 count개만 비교
        for (size_t i = 0; i < count; i++) {
            if (decoded[i] != batch_input[i]) {
                ok = false;
                cerr << "[" << label << " FAIL] global_idx=" << (offset + i)
                          << " : expected " << batch_input[i]
                          << ", got " << decoded[i] << "\n";
            }
        }

        total_checked += count;
    }

    if (ok) {
        cout << "[" << label << " OK] encode->encrypt->decrypt->decode passed ("
                  << total_checked << " items checked)\n";
    }

    return ok;
}


//03.31 : 중간과정을 보여주는 디버그 함수 추가 
// ─────────────────────────────────────────
// 디버그용: 벡터 앞부분 출력
// ─────────────────────────────────────────
void print_vector_preview(const vector<uint64_t>& data,
                          const string& label,
                          size_t max_items) {
    cout << "[" << label << "] size = " << data.size() << "\n";
    cout << "  preview: [";

    size_t count = min(max_items, data.size());
    for (size_t i = 0; i < count; i++) {
        cout << data[i];
        if (i + 1 != count) cout << ", ";
    }

    if (data.size() > count) cout << ", ...";
    cout << "]\n";
}


// ─────────────────────────────────────────
// 디버그용: plaintext를 다시 decode해서 앞부분 출력
//
// BFV + BatchEncoder를 쓰고 있으므로,
// plaintext 내부 다항식을 사람이 직접 읽기보다는
// decode해서 "슬롯 값"으로 보는 게 훨씬 직관적이다.
// ─────────────────────────────────────────
void print_plain_preview(const Plaintext& plain,
                         const FHEContext& fhe,
                         const string& label,
                         size_t max_items) {
    vector<uint64_t> decoded;
    fhe.encoder->decode(plain, decoded);

    cout << "[" << label << "]\n";
    cout << "  coeff_count = " << plain.coeff_count() << "\n";
    cout << "  significant_coeff_count = " << plain.significant_coeff_count() << "\n";

    size_t count = min(max_items, decoded.size());
    cout << "  decoded preview: [";
    for (size_t i = 0; i < count; i++) {
        cout << decoded[i];
        if (i + 1 != count) cout << ", ";
    }
    if (decoded.size() > count) cout << ", ...";
    cout << "]\n";
}


// ─────────────────────────────────────────
// 디버그용: ciphertext 기본 정보 출력
//
// 현재 보고 싶은 정보:
// - ciphertext size (직렬화 크기)
// - 현재 level(parms_id 기준)
// - coeff_modulus 개수 및 bits
// - noise budget (BFV에서 복호화 여유 정도)
//
// noise budget은 "지금 암호문이 얼마나 더 연산을 견딜 수 있는지"
// 감을 보는 데 유용하다.
// ─────────────────────────────────────────
void print_cipher_basic_info(const Ciphertext& cipher,
                             const FHEContext& fhe,
                             const string& label) {
    auto context_data = fhe.context->get_context_data(cipher.parms_id());

    cout << "[" << label << "]\n";
    cout << "  ciphertext size = "
              << cipher.save_size(compr_mode_type::none)
              << " bytes\n";

    if (!context_data) {
        cout << "  invalid parms_id\n";
        return;
    }

    const auto& coeffs = context_data->parms().coeff_modulus();

    cout << "  chain_index     = " << context_data->chain_index() << "\n";
    cout << "  coeff_mod count = " << coeffs.size() << "\n";

    cout << "  coeff_mod bits  = [";
    for (size_t i = 0; i < coeffs.size(); i++) {
        cout << coeffs[i].bit_count();
        if (i + 1 != coeffs.size()) cout << ", ";
    }
    cout << "]\n";

    // BFV에서 현재 noise budget 확인
    cout << "  noise budget    = "
              << fhe.decryptor->invariant_noise_budget(cipher)
              << " bits\n";
}


// ─────────────────────────────────────────
// 디버그용: FHE 파이프라인 전체 보기
//
// raw_input
//   -> encode_batch
//   -> encrypt_batch
//   -> decrypt_decode
//
// 각 단계에서 앞부분 슬롯과 ciphertext 정보를 출력한다.
// 지금 단계에서 "배칭 / 암호화 / 복호화"를 눈으로 확인하기 가장 좋다.
// ─────────────────────────────────────────
void debug_fhe_pipeline(const vector<uint64_t>& raw_input,
                        const FHEContext& fhe,
                        const string& label,
                        size_t preview_count) {
    cout << "\n=== " << label << " ===\n";

    // slot_count보다 작으면 0 padding
    vector<uint64_t> input(fhe.slot_count, 0);
    size_t count = min(raw_input.size(), fhe.slot_count);
    for (size_t i = 0; i < count; i++) {
        input[i] = raw_input[i];
    }

    // 1) 원본 입력 보기
    print_vector_preview(input, label + " / raw input", preview_count);

    // 2) encode 후 plain 보기
    auto plain = encode_batch(input, fhe);
    print_plain_preview(plain, fhe, label + " / encoded plain", preview_count);

    // 3) encrypt 후 ciphertext 정보 보기
    auto cipher = encrypt_batch(plain, fhe);
    print_cipher_basic_info(cipher, fhe, label + " / encrypted cipher");

    // 4) decrypt + decode 후 보기
    auto decoded = decrypt_decode(cipher, fhe);
    print_vector_preview(decoded, label + " / decrypted output", preview_count);

    // 5) 앞 preview_count개 비교
    bool ok = true;
    size_t check_count = min(preview_count, count);
    for (size_t i = 0; i < check_count; i++) {
        if (decoded[i] != input[i]) {
            ok = false;
            cerr << "[" << label << " FAIL] slot " << i
                      << ": expected " << input[i]
                      << ", got " << decoded[i] << "\n";
        }
    }

    if (ok) {
        cout << "[" << label << " OK] preview slots matched after full pipeline\n";
    }
}