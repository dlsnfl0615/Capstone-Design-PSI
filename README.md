# Capstone-Design-PSI (Receiver Web Server)

PSI(Private Set Intersection) 프로토콜을 BFV 완전동형암호(FHE)와 뻐꾸기 해싱(Cuckoo Hashing) 기반으로 구현한 캡스톤 디자인 프로젝트의 **Receiver(수신자) 웹 서버** 구현체입니다.

Receiver는 자신의 데이터를 암호화하여 Sender에게 전송하고, Sender의 연산 결과를 복호화하여 두 집합의 **교집합(Intersection)** 을 프라이버시를 보호하면서 계산합니다.

---

## 프로젝트 개요

### PSI(Private Set Intersection)란?
두 당사자(Sender, Receiver)가 각자의 데이터를 상대방에게 공개하지 않으면서 두 집합의 교집합을 계산하는 암호학적 프로토콜입니다.

### 전체 동작 흐름
(io 사진으로 대체 예정)

---

## 디렉토리 구조

- receiver/
  - src/
    - main/
      - java/com/psi/receiver/
        - advice/ : 전역 예외 처리 (GlobalExceptionHandler)
        - component/ : 세션별 파라미터, SSE 파일 상태 관리 및 중계
        - config/ : WebClient 설정 (네트워크 통신 인프라)
        - controller/ : REST API 및 파일 다운로드 컨트롤러 (ReceiverApiController)
        - domain/ : 내부 데이터 모델 (Parameters)
        - service/ : 핵심 비즈니스 로직
          - NativeService.java : FFM API를 활용한 C++ Native 라이브러리 연동
          - ReceiverClient.java : Sender 서버와의 파일/API 통신 담당
          - TimingEditor.java : 암호화/복호화/통신 성능 지표 취합 및 JSON 편집
      - native/ : C++ Core 소스 코드 및 빌드 환경 (CMake)
      - resources/ : 웹 프론트엔드 템플릿 (Thymeleaf) 및 설정 파일
  - storage/ : 세션별 데이터 및 파일 임시 저장소 (Docker 볼륨 연동)
  - build.gradle : Gradle 빌드 스크립트 (Java 25 환경 설정)

### 세션별 저장 데이터 관리 (storage/sessions/{sessionId}/)
사용자 요청 시점의 세션별로 아래 파일들이 독립적으로 생성 및 관리됩니다.

- uploaded_name.csv (방향: 사용자 -> Receiver) : 원본 Receiver 입력 데이터
- parms.bin (방향: Receiver Native -> Sender) : BFV 암호화 파라미터
- public_key.bin (방향: Receiver Native -> Sender) : BFV 공개키
- relin_key.bin (방향: Receiver Native -> Sender) : Relinearization 키
- powers.bin (방향: Receiver Native -> Sender) : 윈도잉 암호화 결과
- receiver_hash.bin (방향: Receiver 내부 저장) : 뻐꾸기 해시 테이블
- result.bin (방향: Sender -> Receiver Native) : Sender의 다항식 연산 결과
- intersections.csv (방향: Receiver Native -> 사용자) : 최종 매칭 및 복호화된 교집합 결과 리포트
- timing.json (방향: Receiver Server 통합 저장) : 모든 단계의 수행 시간 기록 (성능 지표 수집용)

---

## 주요 모듈 설명

### parameters.h - 전역 파라미터
- m : 뻐꾸기 해시 테이블 전체 빈 수
- h : 뻐꾸기 해싱에 사용하는 해시 함수 수
- B : Sender 빈당 배열 길이
- sigma : 아이템 비트 최대 길이
- n : BFV 다항식 차수
- t : 평문 모듈러스 비트 길이
- l : 윈도잉 파라미터
- alpha : 파티셔닝 파라미터
- lambda : 보안 수준 (비트)
- CUCKOO_MAX_KICK : 뻐꾸기 해싱 최대 Kick 횟수

---

### data_loader - 데이터 로더
CSV 파일을 읽어 Receiver와 Sender의 데이터를 로드합니다.

**입력 CSV 형식 (Receiver):**
personal_id, Disease_1, Disease_2, ..., Disease_10, Status
920624-2447527, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1

**데이터 패킹 방식:**
주민등록번호(13자리) + 질병코드(10비트)를 하나의 uint64_t로 패킹합니다.
packed = (pid_decimal << 10) | disease_bits

---

### hashing - 뻐꾸기 해싱 (Cuckoo Hashing)
Receiver의 데이터를 해시 테이블에 배치합니다. MurmurHash3를 해시 함수로 사용하며, 순열 기반 뻐꾸기 해싱을 적용합니다.

**핵심 로직:**
1. 아이템을 상위 비트(x_L)와 하위 비트(x_R)로 분리
2. 위치 계산: Loc = H(x_L) % m ^ x_R
3. 충돌 발생 시 기존 값을 다음 해시 함수로 Kick-out
4. 최대 500회 Kick 이후에도 삽입 불가 시 경고 출력

**패킹 값 구조:**
packed = (x_L << 2) | hash_idx
해시 테이블의 빈 슬롯은 RECEIVER_DUMMY로 초기화됩니다.

---

### windowing - 윈도잉 기반 거듭제곱 암호화
지수의 수를 최소화하면서 다항식 평가에 필요한 모든 거듭제곱을 암호화합니다.

**지수 집합 생성 (get_exponents):**
윈도우 크기 l과 파티셔닝 파라미터 alpha를 기반으로 아래 형태의 지수를 생성합니다.
e = i * 2^(l * j), (1 <= i <= 2^l - 1, 0 <= j <= j_max)

**암호화 (receiver_windowing):**
각 지수 e에 대해 해시 테이블의 모든 슬롯 값의 e제곱을 계산하고 BFV 배치 인코딩으로 암호화합니다.
pod_powers[k] = hash_table[k]^e mod plain_modulus

---

### mod - 오버플로우 방지 모듈러 연산
Windows 환경에서 MSVC 컴파일러가 unsigned __int128을 지원하지 않으므로, 비트 분할 방식으로 모듈러 곱셈과 거듭제곱을 구현합니다.

- safe_mul_mod(a, b, m) : 오버플로우 없이 (a * b) % m 계산
- power_mod(base, exp, mod) : 오버플로우 없이 base^exp % mod 계산

---

## 실행 순서 (Web API 워크플로우)

### 1단계: CSV 데이터 업로드 및 암호화 요청 생성
사용자가 웹 클라이언트를 통해 CSV 파일을 업로드하면, Receiver 서버는 내부 세션 디렉토리에 파일을 격리 저장합니다. 이후 암호화 요청을 생성하기 위해 Native API를 호출합니다.

**내부 동작:**
1. storage/sessions/{sessionId}/에 업로드된 CSV 로드
2. Java FFM API를 통해 libreceiver-psi.so의 request 함수 호출
3. 뻐꾸기 해싱으로 해시 테이블 구성 및 윈도잉 거듭제곱 암호화 (powers.bin 등 생성)
4. 생성된 파일들을 ReceiverClient를 통해 Sender 서버로 자동 전송

### 2단계: Sender 연산 수행
Sender 서버가 전달받은 파일로 다항식 연산을 원격 수행하고, 연산 결과 암호문인 result.bin을 Receiver의 결과 수신 API로 반환합니다.

### 3단계: 복호화 및 최종 교집합 연산
Sender로부터 연산 결과가 도착하면 최종 복호화 및 매칭 연산을 수행합니다.

**내부 동작:**
1. 수신된 result.bin 로드 및 Native 라이브러리의 result 함수 호출
2. 비밀키로 복호화 및 배치 디코딩 수행
3. 디코딩된 슬롯 값이 0인 위치를 판별하여 교집합 식별
4. 해시 테이블과 대조하여 원본 데이터와 매칭 후 intersections.csv 결과 파일 생성

**교집합 판별 원리:**
Sender는 자신의 집합 S를 근으로 하는 다항식 P(x)를 구성합니다. Receiver의 아이템 y에 대해 P(y) = 0 이면 y가 S의 원소, 즉 교집합 원소입니다.

---

## 의존성
- Microsoft SEAL : BFV 완전동형암호 연산
- MurmurHash3 : 뻐꾸기 해싱용 해시 함수
- Java 22+ (FFM API) : 고성능 C++ 공유 라이브러리 연동 및 메모리 제어

---

## 구동 환경 및 서버 실행 방법 (AWS / 운영 환경)

### JVM 옵션 지정 및 Native 접근 허용
Java FFM API를 활용한 원활한 공유 라이브러리 다운콜을 위해, 실행 시 반드시 외부 네이티브 메모리 접근 권한을 명시해야 합니다. build.gradle에 해당 설정이 반영되어 있습니다.

**애플리케이션 빌드 명령어:**
./gradlew clean bootJar

**JVM 필수 옵션을 포함한 서비스 가동 명령어:**
java --enable-native-access=ALL-UNNAMED -jar build/libs/receiver-0.0.1-SNAPSHOT.jar

---

## 입력 데이터 형식
data/receiver.csv 파일의 형식은 다음과 같습니다.

personal_id,Disease_1,Disease_2,Disease_3,Disease_4,Disease_5,Disease_6,Disease_7,Disease_8,Disease_9,Disease_10,Status
920624-2447527,1,0,1,0,0,1,0,0,1,0,1

- personal_id : 주민등록번호 (하이픈 자동 제거)
- Disease_1 ~ Disease_10 : 0 또는 1 (질병 보유 여부 이진값)
- Status : 문자열 상태 정보 (현재 교집합 계산에 미사용)

---

## 보안 파라미터 요약
- 보안 수준 (lambda) : 40 비트
- BFV 다항식 차수 (n) : 32,768
- 평문 모듈러스 비트 길이 (t) : 44 비트
- 아이템 최대 비트 길이 (sigma) : 43 비트
EOF
