#pragma once

#include <stdint.h>
#include <iostream>
#include <cmath>

// extern int Nx; // 데이터 크기에 비례하지 않는다는 것
// extern int Ny;
// extern int sigma_max; // sigma_max = 2 log_2(Nx+Ny) + lambda - 1

/* plain modulus 비트 길이. 실제 값으로 사용하려면 2^t로 사용해야 함
   log2t > sigma_max - log2m + ceil(log2h) + 1 */ 
const int t = 23;
const int lambda = 40; // 보안 수준

const int m = 8192; // 해시 테이블의 전체 빈 (Bin) 개수. 바깥 벡터 길이
const int h = 3; // 해시 함수 개수
const int B = 74; // sender 빈 당 할당되는 일차원 배열의 길이
const int n = 8192; // 다항식 차수
// 현재는 순열기반해싱 결과로 들어가는 값의 비트길이로 설정함. sigma max = 71임
const uint64_t sigma = 21; // 비트화했을 때의 최대 길이. 순열기반해싱으로 저장되는 데이터 값(잘린 값 + 해시 함수 번호)
const int CUCKOO_MAX_KICK = 500;
const uint64_t RECEIVER_DUMMY = (1ULL << sigma);
const uint64_t SENDER_DUMMY = ((1ULL << (sigma + 1)) - 1);
// const int q 자동으로 생성됨
const int l = 1; // 윈도잉 파라미터
const int alpha = 32; // 파티셔닝 파라미터 8이면 l을 3. 16이면 l을 2. 32면 1
const int random = 4365234; // 일단 아무거나
const int B_prime = static_cast<int>(ceil(static_cast<double>(B) / alpha));
