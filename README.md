# Capstone-Design-PSI (Receiver)

PSI(Private Set Intersection) 프로토콜을 BFV 완전동형암호(FHE)와 뻐꾸기 해싱(Cuckoo Hashing) 기반으로 구현한 캡스톤 디자인 프로젝트의 **Receiver(수신자) 측** 구현체입니다.

Receiver는 자신의 데이터를 암호화하여 Sender에게 전송하고, Sender의 연산 결과를 복호화하여 두 집합의 **교집합(Intersection)** 을 프라이버시를 보호하면서 계산합니다.

---

## 프로젝트 개요

### PSI(Private Set Intersection)란?

두 당사자(Sender, Receiver)가 각자의 데이터를 상대방에게 공개하지 않으면서 두 집합의 교집합을 계산하는 암호학적 프로토콜입니다.

### 전체 동작 흐름

```
(io 사진으로 대체 예정)
```

---

## 디렉토리 구조

```
receiver/
├── receiver-request.cpp   # 1단계: 암호화 요청 생성 (Sender에 전송)
├── receiver-result.cpp    # 3단계: Sender 결과 수신 후 교집합 계산
├── hashing.cpp / .h       # 뻐꾸기 해싱 구현
├── windowing.cpp / .h     # 윈도잉 기반 거듭제곱 암호화
├── data_loader.cpp / .h   # CSV 데이터 로더 및 패킹 유틸
├── mod.cpp / .h           # 오버플로우 방지 모듈러 산술
├── parameters.h           # 전역 파라미터 정의
└── data/
    └── receiver.csv       # Receiver 입력 데이터
```

### 공유 데이터 디렉토리 (`../data/`)

Receiver와 Sender 간에 공유되는 파일들입니다.

| 파일명 | 방향 | 설명 |
|---|---|---|
| `parms.bin` | Receiver → Sender | BFV 암호화 파라미터 |
| `public_key.bin` | Receiver → Sender | BFV 공개키 |
| `relin_key.bin` | Receiver → Sender | Relinearization 키 |
| `powers.bin` | Receiver → Sender | 윈도잉 암호화 결과 |
| `receiver_hash.bin` | Receiver 내부 저장 | 뻐꾸기 해시 테이블 |
| `result.bin` | Sender → Receiver | Sender의 다항식 연산 결과 |

---

## 주요 모듈 설명

### `parameters.h` — 전역 파라미터

| 파라미터 | 값 | 설명 |
|---|---|---|
| `m` | 8192 | 뻐꾸기 해시 테이블 전체 빈(bin) 수 |
| `h` | 4 | 뻐꾸기 해싱에 사용하는 해시 함수 수 |
| `B` | 74 | Sender 빈당 배열 길이 |
| `sigma` | 43 | 아이템 비트 최대 길이 |
| `n` | 32768 | BFV 다항식 차수 |
| `t` | 44 | 평문 모듈러스(plain modulus) 비트 길이 |
| `l` | 3 | 윈도잉(Windowing) 파라미터 |
| `alpha` | 64 | 파티셔닝 파라미터 |
| `lambda` | 40 | 보안 수준 (비트) |
| `CUCKOO_MAX_KICK` | 500 | 뻐꾸기 해싱 최대 Kick 횟수 |

---

### `data_loader` — 데이터 로더

CSV 파일을 읽어 Receiver와 Sender의 데이터를 로드합니다.

**입력 CSV 형식 (Receiver):**

```
personal_id, Disease_1, Disease_2, ..., Disease_10, Status
920624-2447527, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1
```

**데이터 패킹 방식:**

주민등록번호(13자리) + 질병코드(10비트)를 하나의 `uint64_t`로 패킹합니다.

```
packed = (pid_decimal << 10) | disease_bits
```

---

### `hashing` — 뻐꾸기 해싱 (Cuckoo Hashing)

Receiver의 데이터를 해시 테이블에 배치합니다.  
MurmurHash3를 해시 함수로 사용하며, 순열 기반(Permutation-based) 뻐꾸기 해싱을 적용합니다.

**핵심 로직:**

1. 아이템을 상위 비트(`x_L`)와 하위 비트(`x_R`)로 분리
2. 위치 계산: `Loc = H(x_L) % m ^ x_R`
3. 충돌 발생 시 기존 값을 다음 해시 함수로 Kick-out
4. 최대 500회 Kick 이후에도 삽입 불가 시 경고 출력

**패킹 값 구조:**

```
packed = (x_L << 2) | hash_idx
```

해시 테이블의 빈 슬롯은 `RECEIVER_DUMMY`로 초기화됩니다.

---

### `windowing` — 윈도잉 기반 거듭제곱 암호화

지수(exponent)의 수를 최소화하면서 다항식 평가에 필요한 모든 거듭제곱을 암호화합니다.

**지수 집합 생성 (`get_exponents`):**

윈도우 크기 `l`과 파티셔닝 파라미터 `alpha`를 기반으로 아래 형태의 지수를 생성합니다.

```
e = i × 2^(l × j),   1 ≤ i ≤ 2^l - 1,   0 ≤ j ≤ j_max
```

**암호화 (`receiver_windowing`):**

각 지수 `e`에 대해 해시 테이블의 모든 슬롯 값의 `e`제곱을 계산하고 BFV 배치 인코딩으로 암호화합니다.

```
pod_powers[k] = hash_table[k]^e mod plain_modulus
```

---

### `mod` — 오버플로우 방지 모듈러 연산

Windows 환경에서 MSVC 컴파일러가 `unsigned __int128`을 지원하지 않으므로, 비트 분할 방식으로 모듈러 곱셈과 거듭제곱을 구현합니다.

| 함수 | 설명 |
|---|---|
| `safe_mul_mod(a, b, m)` | 오버플로우 없이 `(a × b) % m` 계산 |
| `power_mod(base, exp, mod)` | 오버플로우 없이 `base^exp % mod` 계산 |

---

## 실행 순서

### 1단계: `receiver-request` 실행

Sender에게 보낼 암호화된 요청을 생성합니다.

```bash
./receiver-request
```

**내부 동작:**
1. `data/receiver.csv` 로드
2. 뻐꾸기 해싱으로 해시 테이블 구성
3. 윈도잉으로 거듭제곱 암호화 (`powers.bin`)
4. BFV 파라미터, 공개키, 비밀키, Relinearization 키 저장

> 이 단계에서 생성된 `parms.bin`, `public_key.bin`, `relin_key.bin`, `powers.bin`을 **Sender에게 전달**합니다.

---

### 2단계: Sender 연산 (sender 브랜치)

Sender가 전달받은 파일로 다항식 연산을 수행하고 `result.bin`을 반환합니다.

---

### 3단계: `receiver-result` 실행

Sender로부터 `result.bin`을 받은 후 교집합을 계산합니다.

```bash
./receiver-result
```

**내부 동작:**
1. `result.bin` (Sender의 암호화된 연산 결과) 로드
2. 비밀키로 복호화 및 배치 디코딩
3. 디코딩된 슬롯 값이 `0`인 위치 → 교집합에 해당
4. 해시 테이블과 대조하여 원본 데이터와 매칭

**교집합 판별 원리:**

Sender는 자신의 집합 S를 근으로 하는 다항식 P(x)를 구성합니다.  
Receiver의 아이템 y에 대해 **P(y) = 0** 이면 y ∈ S, 즉 교집합 원소입니다.

---

## 의존성

| 라이브러리 | 용도 |
|---|---|
| [Microsoft SEAL](https://github.com/microsoft/SEAL) | BFV 완전동형암호 연산 |
| MurmurHash3 | 뻐꾸기 해싱용 해시 함수 |

### SEAL 설치

```bash
git clone https://github.com/microsoft/SEAL.git
cd SEAL
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

---

## 빌드

```bash
mkdir build && cd build
cmake ..
make
```

> SEAL 라이브러리 경로가 시스템에 설치되어 있어야 합니다.  
> CMakeLists.txt에서 `find_package(SEAL REQUIRED)` 설정을 확인하세요.

---

## 입력 데이터 형식

`data/receiver.csv` 파일의 형식은 다음과 같습니다.

```
personal_id,Disease_1,Disease_2,Disease_3,Disease_4,Disease_5,Disease_6,Disease_7,Disease_8,Disease_9,Disease_10,Status
920624-2447527,1,0,1,0,0,1,0,0,1,0,1
...
```

| 컬럼 | 형식 | 설명 |
|---|---|---|
| `personal_id` | `XXXXXX-XXXXXXX` | 주민등록번호 (하이픈 자동 제거) |
| `Disease_1` ~ `Disease_10` | `0` 또는 `1` | 질병 보유 여부 (이진값) |
| `Status` | 문자열 | 상태 정보 (현재 교집합 계산에 미사용) |

---

## 보안 파라미터 요약

| 항목 | 값 |
|---|---|
| 보안 수준 (λ) | 40 비트 |
| BFV 다항식 차수 (n) | 32,768 |
| 평문 모듈러스 비트 길이 (t) | 44 비트 |
| 아이템 최대 비트 길이 (σ) | 43 비트 |
