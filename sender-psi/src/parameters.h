#pragma once

#include <stdint.h>
#include <iostream>
#include <cmath>

// extern int Nx; // 데이터 크기에 비례하지 않는다는 것
// extern int Ny;
// extern int sigma_max; // sigma_max = 2 log_2(Nx+Ny) + lambda - 1

/* plain modulus 비트 길이. 실제 값으로 사용하려면 2^t로 사용해야 함
   log2t > sigma_max - log2m + ceil(log2h) + 1 */ 
const int t = 45;
const int lambda = 40; // 보안 수준

const int m = 32768; // 해시 테이블의 전체 빈 (Bin) 개수. 바깥 벡터 길이
const int h = 3; // 해시 함수 개수
const int B = 9960; // sender 빈 당 할당되는 일차원 배열의 길이
const int n = 16384; // 다항식 차수
// m개의 bin을 n개 slot 단위로 몇 개 block으로 나눌지 계산
//
// 예:
// m = 32768, n = 16384 -> num_blocks = 2
// m = 65536, n = 16384 -> num_blocks = 4
//
// 기존에는 암호문 하나에 m개의 bin을 모두 넣었지만,
// 이제는 block 하나당 n개 bin만 넣고,
// 전체 m개 bin은 num_blocks개의 암호문으로 나누어 처리함.
const int num_blocks = (m + n - 1) / n; // m>n인 경우 블록으로 나눔

// dummy 값은 plain modulus 안에 들어가야 함. 실제 packed 값은 다항식 계산에서 mod plain_modulus로 비교됨.
const uint64_t sigma = 43;
const int CUCKOO_MAX_KICK = 500;
const uint64_t RECEIVER_DUMMY = (1ULL << sigma);
const uint64_t SENDER_DUMMY = ((1ULL << (sigma + 1)) - 1);
// const int q 자동으로 생성됨
const int l = 1; // 윈도잉 파라미터
const int alpha = 256; // 파티셔닝 파라미터 8이면 l을 3. 16이면 l을 2. 32면 1
const int seed = 4365234; // 일단 아무거나
const int B_prime = static_cast<int>(ceil(static_cast<double>(B) / alpha));
const int NUM_THREADS = 8; // 멀티스레드 개수

// 해시 함수용 log2m 계산
inline int get_log2_m() {
   int bits = 0;
   int temp = m;

   while (temp > 1) {
      temp >>= 1;
      bits++;
   }

   return bits;
}

inline uint64_t get_x_r_mask() {
   return (1ULL << get_log2_m()) - 1;
}
