# Capstone-Design-PSI (Sender)

PSI(Private Set Intersection) 프로토콜을 BFV 완전동형암호(FHE)와 단순 해싱(Simple Hashing) 기반으로 구현한 캡스톤 디자인 프로젝트의 **Sender(송신자) 측** 구현체입니다.

Sender는 Receiver로부터 암호화된 거듭제곱 값을 수신하고, 자신의 데이터로 구성한 다항식을 **암호화된 상태로 평가**하여 결과를 반환합니다. Receiver는 이 결과를 복호화하여 교집합을 계산합니다.

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
sender/
├── sender-main.cpp        # 메인 진입점: Sender 전체 흐름 제어
├── evaluate.cpp / .h      # 파티셔닝, 다항식 계수 계산, FHE 평가
├── hashing.cpp / .h       # 단순 해싱(Simple Hashing) 구현
├── data_loader.cpp / .h   # CSV 데이터 로더 및 패킹 유틸
├── mod.cpp / .h           # 오버플로우 방지 모듈러 산술
├── parameters.h           # 전역 파라미터 정의
└── data/
    └── sender.csv         # Sender 입력 데이터
```

### 공유 데이터 디렉토리 (`../data/`)

Receiver와 Sender 간에 공유되는 파일들입니다.

| 파일명 | 방향 | 설명 |
|---|---|---|
| `parms.bin` | Receiver → Sender | BFV 암호화 파라미터 |
| `public_key.bin` | Receiver → Sender | BFV 공개키 |
| `relin_key.bin` | Receiver → Sender | Relinearization 키 |
| `powers.bin` | Receiver → Sender | 윈도잉 암호화 거듭제곱 결과 |
| `result.bin` | Sender → Receiver | 다항식 평가 결과 (암호문) |

---

## 주요 모듈 설명

### `parameters.h` — 전역 파라미터

| 파라미터 | 값 | 설명 |
|---|---|---|
| `m` | 8192 | 해시 테이블 전체 빈(bin) 수 |
| `h` | 4 | 해싱에 사용하는 해시 함수 수 |
| `B` | 74 | Sender 빈당 슬롯 수 (배열 길이) |
| `sigma` | 43 | 아이템 비트 최대 길이 |
| `n` | 32768 | BFV 다항식 차수 |
| `t` | 44 | 평문 모듈러스(plain modulus) 비트 길이 |
| `l` | 3 | 윈도잉(Windowing) 파라미터 |
| `alpha` | 64 | 파티셔닝 파라미터 |
| `lambda` | 40 | 보안 수준 (비트) |
| `B_prime` | `ceil(B / alpha)` | 파티션당 행 수 |

---

### `data_loader` — 데이터 로더

CSV 파일을 읽어 Sender의 데이터를 로드합니다.

**입력 CSV 형식 (Sender):**

```
personal_id, Disease_1, Disease_2, ..., Disease_10
920624-2447527, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0
```

**데이터 패킹 방식:**

주민등록번호(13자리) + 질병코드(10비트)를 하나의 `uint64_t`로 패킹합니다.

```
packed = (pid_decimal << 10) | disease_bits
```

---

### `hashing` — 단순 해싱 (Simple Hashing)

Sender는 Receiver의 쿠쿠 해싱과 달리 **단순 해싱(Simple Hashing)** 을 사용합니다.  
각 아이템을 `h`개의 해시 함수 모두에 대해 계산하고, 해당하는 모든 빈(Bin)에 삽입합니다.

**핵심 로직:**

1. 아이템을 상위 비트(`x_L`)와 하위 비트(`x_R`)로 분리
2. `h`개의 해시 함수 각각에 대해 위치 계산: `Loc = H_i(x_L) % m ^ x_R`
3. 해당 빈의 슬롯에 순서대로 삽입 (최대 `B`개)
4. 빈이 가득 찰 경우 경고 출력 후 아이템 드롭

**해시 테이블 구조:**

```
hash_table[m][B]  →  m개의 빈, 각 빈에 최대 B개의 슬롯
```

빈 슬롯은 `SENDER_DUMMY`로 초기화됩니다.

---

### `evaluate` — 파티셔닝 및 FHE 다항식 평가

Sender의 핵심 연산 모듈로, 다음 세 단계로 구성됩니다.

#### 1. 파티셔닝 (`partitioning`)

`B`개의 슬롯으로 구성된 해시 테이블을 `alpha`개의 파티션으로 분할합니다.  
각 파티션은 `B_prime`개의 행을 가집니다.

```
partitions[alpha][B_prime][m]
↑ alpha=64개 파티션, 각 파티션은 B_prime행 × m열
```

#### 2. 다항식 계수 계산 (`extract_all_coefficients`)

각 파티션의 각 빈(열)에 대해, 해당 빈의 원소들을 **근(root)** 으로 하는 다항식의 계수를 계산합니다.

```
P(x) = r × ∏(x - s_i)   (s_i: Sender 집합 원소, r: 난수)
```

- 난수 `r`을 곱하여 계수를 난수화함으로써 보안성을 확보합니다.
- 오버플로우 방지를 위해 모든 곱셈에 `safe_mul_mod`를 적용합니다.

#### 3. FHE 다항식 평가 (`make_all_powers` + `product`)

**`make_all_powers`:** Receiver가 보낸 윈도잉 암호문을 기반으로 모든 필요한 지수의 암호문을 동형 곱셈으로 생성합니다.

```
암호문 조합 방식: e = i × 2^(l×j) 형태로 분해 → 동형 곱셈
```

**`product`:** 각 파티션에 대해 계수와 암호화된 거듭제곱의 내적을 계산합니다.

```
result[p] = Σ(coeff[p][d] × Enc(y^d))   for d = 0, 1, ..., B_prime
```

수신자의 아이템 y가 Sender 집합에 존재하면 해당 슬롯의 결과값이 0이 됩니다.

---

### `mod` — 오버플로우 방지 모듈러 산술

Windows 환경에서 MSVC 컴파일러가 `unsigned __int128`을 지원하지 않으므로, 비트 분할 방식으로 모듈러 곱셈과 거듭제곱을 구현합니다.

| 함수 | 설명 |
|---|---|
| `safe_mul_mod(a, b, m)` | 오버플로우 없이 `(a × b) % m` 계산 |
| `power_mod(base, exp, mod)` | 오버플로우 없이 `base^exp % mod` 계산 |

---

## 실행 순서

### 전제 조건

`sender-main`을 실행하기 전에 Receiver 측에서 생성한 아래 파일들이 `../data/` 경로에 존재해야 합니다.

| 파일 | 생성 주체 |
|---|---|
| `parms.bin` | Receiver (`receiver-request`) |
| `public_key.bin` | Receiver (`receiver-request`) |
| `relin_key.bin` | Receiver (`receiver-request`) |
| `powers.bin` | Receiver (`receiver-request`) |

### `sender-main` 실행

```bash
./sender-main
```

**내부 동작 순서:**

1. `../data/`에서 파라미터, 공개키, Relinearization 키, 암호화된 거듭제곱 로드
2. `data/sender.csv` 로드
3. 단순 해싱으로 해시 테이블 구성
4. 파티셔닝 수행 (`alpha`개 파티션으로 분할)
5. 각 파티션의 다항식 계수 계산
6. 윈도잉 암호문으로 모든 지수의 암호문 생성 (0승 포함)
7. 각 파티션에 대해 암호화된 다항식 평가
8. 결과를 `../data/result.bin`으로 저장

> 생성된 `result.bin`을 **Receiver에게 전달**합니다.

---

## 의존성

| 라이브러리 | 용도 |
|---|---|
| [Microsoft SEAL](https://github.com/microsoft/SEAL) | BFV 완전동형암호 연산 |
| MurmurHash3 | 단순 해싱용 해시 함수 |

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

`data/sender.csv` 파일의 형식은 다음과 같습니다.

```
personal_id,Disease_1,Disease_2,Disease_3,Disease_4,Disease_5,Disease_6,Disease_7,Disease_8,Disease_9,Disease_10
920624-2447527,1,0,1,0,0,1,0,0,1,0
...
```

| 컬럼 | 형식 | 설명 |
|---|---|---|
| `personal_id` | `XXXXXX-XXXXXXX` | 주민등록번호 (하이픈 자동 제거) |
| `Disease_1` ~ `Disease_10` | `0` 또는 `1` | 질병 보유 여부 (이진값) |

> Receiver CSV와 달리 `Status` 컬럼이 없습니다. (11개 컬럼)

---

## 보안 파라미터 요약

| 항목 | 값 |
|---|---|
| 보안 수준 (λ) | 40 비트 |
| BFV 다항식 차수 (n) | 32,768 |
| 평문 모듈러스 비트 길이 (t) | 44 비트 |
| 아이템 최대 비트 길이 (σ) | 43 비트 |
| 파티션 수 (α) | 64 |
| 빈당 슬롯 수 (B) | 74 |
| 파티션당 행 수 (B') | `ceil(74 / 64)` = 2 |
