# FHE PSI 실행 흐름

이 저장소의 `fhe_psi` 폴더는 Microsoft SEAL의 BFV 스킴을 사용해
Private Set Intersection(PSI)을 실험하는 C++ 코드입니다. 현재 실행
엔트리포인트는 `fhe_psi/src/main.cpp`이며, receiver와 sender 데이터를
읽은 뒤 해싱, 윈도잉, 동형 다항식 평가, 복호화 검사를 순서대로 수행합니다.

## 전체 흐름 요약

```text
CSV 로딩
  -> Receiver cuckoo hashing
  -> Receiver windowing + encryption
  -> Sender simple hashing
  -> Sender partitioning + polynomial coefficients
  -> Sender homomorphic evaluation
  -> Receiver decrypt + intersection check
```

## 빌드 및 실행

`fhe_psi/CMakeLists.txt`는 실행 파일 이름을 `fhe_psi`로 설정합니다. CMake
설정 시 `fhe_psi/data` 폴더가 build 디렉터리 아래로 복사되므로, 현재 코드의
`data/receiver.csv`, `data/sender.csv` 상대 경로는 build 산출물 기준으로
동작합니다.

```bash
cd fhe_psi
cmake -S . -B build
cmake --build build
./build/fhe_psi
```

## 주요 파라미터

파라미터는 `fhe_psi/src/parameters.h`에 정의되어 있습니다.

| 이름 | 값 | 의미 |
| --- | ---: | --- |
| `m` | `8192` | 해시 테이블의 bin 개수 |
| `h` | `4` | 해시 함수 개수 |
| `B` | `74` | sender의 각 bin에 저장 가능한 최대 원소 수 |
| `n` | `32768` | BFV polynomial modulus degree |
| `t` | `44` | batching plain modulus 비트 길이 |
| `l` | `3` | windowing 파라미터 |
| `alpha` | `64` | sender hash table partition 개수 |
| `B_prime` | `ceil(B / alpha)` | partition 하나가 담당하는 sender row 수 |
| `RECEIVER_DUMMY` | `1 << sigma` | receiver hash table의 빈 슬롯 표시값 |
| `SENDER_DUMMY` | `(1 << (sigma + 1)) - 1` | sender hash table의 빈 슬롯 표시값 |

## `main.cpp` 단계별 실행 흐름

### 1. BFV 파라미터와 SEAL context 생성

`main()`은 먼저 BFV 스킴용 `EncryptionParameters`를 생성합니다.

- `poly_modulus_degree`는 `parameters.h`의 `n` 값을 사용합니다.
- coefficient modulus는 `CoeffModulus::BFVDefault(n)`로 설정합니다.
- batching을 위해 `PlainModulus::Batching(n, t)`를 plain modulus로 설정합니다.
- 설정된 파라미터로 `SEALContext`를 만들고 실제 plain modulus 값을 출력합니다.

이 단계는 이후 모든 암호화, 복호화, batch encoding, 동형 연산의 공통 환경을
준비합니다.

### 2. 키와 SEAL 연산 객체 초기화

다음으로 `KeyGenerator`를 사용해 receiver가 사용할 키를 생성합니다.

- `SecretKey`: 최종 결과 복호화에 사용합니다.
- `PublicKey`: receiver 데이터를 암호화하는 데 사용합니다.
- `RelinKeys`: sender가 암호문 곱셈 후 relinearization을 수행하는 데 사용합니다.

그 뒤 `Encryptor`, `Evaluator`, `Decryptor`, `BatchEncoder`를 생성합니다.

### 3. Receiver/Sender CSV 로딩

`data_loader.cpp`의 `load_receiver()`와 `load_sender()`가 CSV 파일을 읽습니다.

- receiver 입력: `data/receiver.csv`
- sender 입력: `data/sender.csv`
- 첫 줄은 header로 건너뜁니다.
- `personal_id`의 하이픈을 제거해 13자리 PID 문자열로 만듭니다.
- `Disease_1`부터 `Disease_10`까지 이어 붙여 10비트 질병 문자열을 만듭니다.
- 최종 record는 `PID 13자리 + 질병 10비트` 형태의 23자리 문자열입니다.

예를 들어 내부 계산에서는 이 값을 다음처럼 64비트 정수로 패킹합니다.

```text
full_item = (pid_decimal << 10) | disease_bits
```

### 4. Receiver cuckoo hashing

`ReceiverHashing::locate()`는 receiver record를 `m`개 bin으로 구성된 1차원
hash table에 배치합니다.

각 record는 다음 순서로 처리됩니다.

1. 23자리 문자열에서 PID와 질병 비트를 분리합니다.
2. `full_item = (pid << 10) | disease`로 패킹합니다.
3. `full_item`을 상위 비트 `x_L`과 하위 13비트 `x_R`로 나눕니다.
4. 초기 해시 함수 인덱스 `hash_idx = 0`을 사용합니다.
5. 저장 값은 `(x_L << 2) | hash_idx` 형태로 패킹합니다.
6. 위치는 `Loc = (H_i(x_L) % m) ^ x_R`로 계산합니다.
7. 충돌이 있으면 cuckoo hashing 방식으로 기존 값을 밀어내고 다음 해시 함수
   위치를 시도합니다.

빈 슬롯은 `RECEIVER_DUMMY`로 초기화되어 있습니다. 최대 `CUCKOO_MAX_KICK`
횟수 안에 빈 슬롯을 찾지 못하면 경고를 출력합니다.

### 5. Receiver windowing 및 암호화

`Windowing::get_exponents()`는 sender가 다항식 평가에 필요한 일부 거듭제곱
지수 목록을 만듭니다. `l`과 `B_prime`을 기준으로 windowing 기저가 되는
지수들을 선택합니다.

`Windowing::receiver_windowing()`은 각 지수 `e`에 대해 receiver hash table의
slot 값을 다음처럼 처리합니다.

- 빈 슬롯이면 `RECEIVER_DUMMY`를 그대로 넣습니다.
- 실제 값이면 `value^e mod plain_modulus`를 계산합니다.
- 길이 `n`의 vector에 `m`개 bin 값을 batch slot으로 배치합니다.
- `BatchEncoder`로 plaintext를 만들고 `Encryptor`로 암호화합니다.

결과는 `map<int, Ciphertext>` 형태로 저장되며, key는 지수 `e`, value는 해당
거듭제곱 값들이 packed 된 암호문입니다. 이 암호문들이 sender에게 전달되는
receiver 측 입력에 해당합니다.

### 6. Sender simple hashing

`SenderHashing::locate()`는 sender record를 `m x B` 형태의 2차원 hash table에
넣습니다.

Receiver와 같은 방식으로 `full_item`, `x_L`, `x_R`를 만든 뒤, sender는
cuckoo hashing을 하지 않고 `h`개의 모든 해시 위치에 값을 삽입합니다.

```text
Loc_i = (H_i(x_L) % m) ^ x_R
packed = (x_L << 2) | i
```

각 위치의 bin에는 최대 `B`개 slot이 있으며, 빈 slot은 `SENDER_DUMMY`로
초기화됩니다. 특정 bin이 가득 차면 해당 item은 삽입되지 않고 경고가 출력됩니다.

### 7. Sender partitioning 및 다항식 계수 생성

`SenderEvaluate::partitioning()`은 sender hash table의 `B`개 row를 `alpha`개
partition으로 나눕니다. 각 partition은 `B_prime`개 row와 `m`개 column을
가집니다.

그 다음 `extract_all_coefficients()`는 각 partition과 각 bin에 대해 sender
값들을 root로 하는 다항식 계수를 계산합니다.

```text
P(X) = r * product(X - sender_value)
```

여기서 `r`은 `random_seed` 기반 값으로, 계수를 난수화하기 위해 곱합니다.
계수 계산과 거듭제곱 계산에는 overflow를 피하기 위해 `safe_mul_mod()`와
`power_mod()`가 사용됩니다.

계수 테이블은 partition별, 차수별, bin별로 정리됩니다. 이후 sender는 이
계수들을 plaintext로 encoding하여 receiver의 암호문 powers와 곱합니다.

### 8. Sender의 암호문 power 확장

Receiver가 보낸 encrypted powers는 windowing 기저에 해당하는 일부 지수만
포함합니다. `SenderEvaluate::make_all_powers()`는 이 암호문들을 조합해
`0`부터 `B_prime` 차수까지 필요한 모든 power를 만듭니다.

- 이미 receiver가 보낸 power는 그대로 사용합니다.
- 없는 power는 지수를 windowing 기저의 합으로 분해한 뒤 암호문 곱셈으로 만듭니다.
- 암호문 곱셈 후에는 `relinearize_inplace()`를 호출합니다.
- `main.cpp`는 별도로 모든 slot이 `1`인 plaintext를 암호화해 `all_powers[0]`에
  넣습니다.

### 9. Sender의 동형 다항식 평가

`SenderEvaluate::product()`는 partition별로 다음 계산을 수행합니다.

```text
P(receiver_value) = c_0 + c_1 * y + c_2 * y^2 + ... + c_d * y^d
```

여기서 `y`는 receiver의 암호화된 hash table 값입니다. 각 차수 row의 계수는
bin별 plaintext vector로 batch encoding되고, 같은 차수의 encrypted power와
`multiply_plain()`으로 곱해집니다. 차수별 결과를 모두 더하면 partition 하나에
대한 암호문 결과가 만들어집니다.

최종적으로 sender는 partition 개수만큼의 `Ciphertext` vector를 반환합니다.
이 값들이 receiver에게 돌아가는 PSI 평가 결과입니다.

### 10. Receiver 복호화 및 교집합 후보 수집

Receiver는 sender가 계산한 각 partition 결과 암호문을 복호화하고 batch decode
합니다.

각 bin slot에 대해 decoded 값이 `0`이면 다음 의미를 갖습니다.

```text
P(y) == 0
```

즉 receiver의 해당 hash table 값 `y`가 sender bin의 root 중 하나와 같다는
뜻이므로 교집합 후보입니다. 이때 receiver hash table의 packed 값을
`intersection_packed_values` set에 저장해 중복을 제거합니다. `RECEIVER_DUMMY`
값은 실제 데이터가 아니므로 제외합니다.

### 11. 원본 receiver record와 대조해 최종 출력

마지막으로 `main.cpp`는 원본 receiver record를 다시 순회합니다.

각 record를 hashing 때와 동일하게 `full_item`, `x_L`로 변환하고, 가능한
`h`개의 packed 값 `(x_L << 2) | hash_idx` 중 하나가
`intersection_packed_values`에 있으면 최종 교집합으로 판단합니다.

교집합으로 판정된 record는 다음 형식으로 출력됩니다.

```text
[O] Found Intersection: PID(...) Disease(...)
```

마지막에는 전체 receiver 데이터 중 발견된 교집합 개수와 실행 시간이 출력됩니다.

## 파일별 역할

| 파일 | 역할 |
| --- | --- |
| `src/main.cpp` | 전체 PSI 실험 흐름을 연결하는 엔트리포인트 |
| `src/parameters.h` | 해시, partition, BFV 관련 상수 정의 |
| `src/data_loader.cpp` | receiver/sender CSV 로딩 및 record 패킹 보조 함수 |
| `src/receiver_hashing.cpp` | receiver cuckoo hashing 및 packed 값 복원 |
| `src/sender_hashing.cpp` | sender simple hashing |
| `src/windowing.cpp` | receiver power windowing 및 암호화 |
| `src/evaluate.cpp` | sender partitioning, 계수 생성, 동형 다항식 평가 |
| `src/mod.cpp` | overflow 방지 modular multiplication/power helper |
| `external/murmur3.cpp` | 해시 함수 구현 |

