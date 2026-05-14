#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include <iomanip>
#include "parameters.h"
#include "data_loader.h"
#include "hashing.h"
#include "windowing.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

extern "C" {
#ifdef _WIN32
__declspec(dllexport) // 윈도우 환경 DLL 내보내기
#endif

int request(const char* storage_dir, const char* receiver_csv) {
try {
          string sd(storage_dir);
              cout << "[setup] Receiver SEAL setting..";
              EncryptionParameters parms(scheme_type::bfv);
              size_t poly_modulus_degree = n;
              parms.set_poly_modulus_degree(poly_modulus_degree);
              parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
              parms.set_plain_modulus(PlainModulus::Batching(n, t));

              SEALContext context(parms);
              uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();

              // 키 생성 시간 측정
              auto t_keygen_start = Clock::now();
              KeyGenerator keygen(context);
              SecretKey secret_key = keygen.secret_key();
              PublicKey public_key;
              keygen.create_public_key(public_key);
              RelinKeys relin_keys;
              keygen.create_relin_keys(relin_keys);
              auto t_keygen_end = Clock::now();

              Encryptor encryptor(context, public_key);
              Evaluator evaluator(context);
              Decryptor decryptor(context, secret_key);
              BatchEncoder batch_encoder(context);
              cout << "completed\n\n";

              // 데이터 로드
              auto receiver_data = load_receiver(receiver_csv);

              // 해싱 시간 측정
              auto t_hashing_start = Clock::now();
              ReceiverHashing receiver_hashing;
              receiver_hashing.locate(receiver_data);
              auto t_hashing_end = Clock::now();

              // 윈도잉 시간 측정
              auto t_windowing_start = Clock::now();
              Windowing windowing;
              vector<int> exponents = windowing.get_exponents();
              map<int, Ciphertext> encrypted_powers = windowing.receiver_windowing(
                  batch_encoder,
                  encryptor,
                  plain_modulus,
                  exponents,
                  receiver_hashing.hash_table);
              auto t_windowing_end = Clock::now();

              // parms 저장
              ofstream parms_out(sd + "/parms.bin", ios::binary);
              parms.save(parms_out);
              parms_out.close();

              // 공개키 저장
              ofstream pk_out(sd + "/public_key.bin", ios::binary);
              public_key.save(pk_out);
              pk_out.close();

              // 비밀키 저장
              ofstream sk_out(sd + "/secret_key.bin", ios::binary);
              secret_key.save(sk_out);
              sk_out.close();

              // relin 키 저장
              ofstream rk_out(sd + "/relin_key.bin", ios::binary);
              relin_keys.save(rk_out);
              rk_out.close();

              // 해시 테이블 저장
              ofstream hash_out(sd + "/receiver_hash.bin", ios::binary);
              size_t table_size = receiver_hashing.hash_table.size();
              hash_out.write(reinterpret_cast<const char*>(&table_size), sizeof(size_t));
              hash_out.write(reinterpret_cast<const char*>(receiver_hashing.hash_table.data()), table_size * sizeof(uint64_t));
              hash_out.close();

              // 윈도잉 결과 저장
              ofstream ofs(sd + "/powers.bin", ios::binary);
              size_t map_size = encrypted_powers.size();
              ofs.write(reinterpret_cast<const char*>(&map_size), sizeof(size_t));
              for (auto &kv : encrypted_powers) {
                  // key (int) 저장
                  int key = kv.first;
                  ofs.write(reinterpret_cast<const char*>(&key), sizeof(int));

                  // value (Ciphertext) 저장
                  kv.second.save(ofs);
              }
              ofs.close();

              cout << "Receiver Request completed." << endl;

              // C++ 단계별 소요 시간 저장
              double keygenMs     = chrono::duration<double, milli>(t_keygen_end   - t_keygen_start).count();
              double hashingMs    = chrono::duration<double, milli>(t_hashing_end  - t_hashing_start).count();
              double windowingMs  = chrono::duration<double, milli>(t_windowing_end - t_windowing_start).count();

              cout << "keygenMs: " << keygenMs << ", hashingMs: " << hashingMs << ", windowingMs: " << windowingMs << endl;

              ofstream timing_out(sd + "/cpp_timing.json");
              timing_out << fixed << setprecision(3)
                         << "{\"keygenMs\":"    << keygenMs
                         << ",\"hashingMs\":"   << hashingMs
                         << ",\"windowingMs\":" << windowingMs << "}";
              timing_out.close();

              return 0;
      } catch (const exception& e) {
          cerr << "[request ERROR] " << e.what() << endl;
          return -1;
      } catch (...) {
          cerr << "[request ERROR] unknown exception" << endl;
          return -2;
      }



}
}