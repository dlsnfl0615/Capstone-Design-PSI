#pragma once

#include <stdint.h>

const int Nx = 5535;
const int Ny = 10;
const int m = 8192; // 해시 테이블의 전체 빈 (Bin) 개수. 바깥 벡터 길이
const int h = 4; // 해시 함수 개수
const int B = 74; // sender 빈 당 할당되는 일차원 배열의 길이
const uint64_t sigma = 54; // 원본 데이터 길이. 비트화했을 때의 최대 길이
const int CUCKOO_MAX_KICK = 500;
const uint64_t RECEIVER_DUMMY = (1ULL << sigma);
const uint64_t SENDER_DUMMY = ((1ULL << (sigma + 1)) - 1);

const int n = 32768; // 다항식 차수
const int t = 45; // 평문 계수
// const int q 자동으로 생성됨

const int l = 3; // 윈도잉 파라미터
const int alpha = 32; // 파티셔닝 파라미터
const int lambda = 40; // 보안 수준