#pragma once

#include <stdint.h>
#include <iostream>

// const int Nx = 65538; // 데이터 크기에 비례하지 않는다는 것
// const int Ny = 5535;
const int m = 16384; // 해시 테이블의 전체 빈 (Bin) 개수. 바깥 벡터 길이
const int h = 4; // 해시 함수 개수
const int B = 6798; // sender 빈 당 할당되는 일차원 배열의 길이
const uint64_t sigma = 43; // 비트화했을 때의 최대 길이. sigma_max = 2 log_2(Nx+Ny) + lambda - 1
const int CUCKOO_MAX_KICK = 500;
const uint64_t RECEIVER_DUMMY = (1ULL << sigma);
const uint64_t SENDER_DUMMY = ((1ULL << (sigma + 1)) - 1);

const int n = 32768; // 다항식 차수. *
const int t = 44; // plain modulus 비트 길이. 실제 값으로 사용하려면 2^t로 사용해야 함
// const int q 자동으로 생성됨. *

const int l = 1; // 윈도잉 파라미터
const int alpha = 256; // 파티셔닝 파라미터 8이면 l을 3. 16이면 l을 2. 32면 1
const int lambda = 40; // 보안 수준

const int random = 4365234; // 일단 아무거나
const int B_prime = static_cast<int>(ceil(static_cast<double>(B) / alpha));