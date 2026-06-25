# Capstone-Design-PSI (Sender Web Server)

PSI(Private Set Intersection) 프로토콜을 BFV 완전동형암호(FHE)와 단순 해싱(Simple Hashing) 기반으로 구현한 캡스톤 디자인 프로젝트의 **Sender(송신자) 웹 서버** 구현체입니다.

Sender는 Receiver로부터 암호화된 거듭제곱 값을 수신하고, 자신의 데이터로 구성한 다항식을 **암호화된 상태로 평가(Evaluate)**하여 결과를 반환합니다. Receiver는 이 결과를 복호화하여 교집합을 계산합니다.

---

## 디렉토리 구조

- sender/
  - src/
    - main/
      - java/com/psi/sender/
        - advice/ : 전역 예외 처리 (GlobalExceptionHandler)
        - component/ : 요청 세션 ID 및 BFV 파라미터 상태 관리
        - config/ : 비동기 처리(Async) 및 WebClient 인프라 설정
        - controller/ : REST API 및 SSE 통신 컨트롤러 (SenderApiController, SseController)
        - domain/ : 내부 데이터 모델 (Parameters)
        - service/ : 핵심 비즈니스 로직
          - NativeService.java : FFM API를 활용한 C++ Native 라이브러리 연동
          - NativeAsync.java : 대용량 다항식 연산의 비동기 실행 제어
          - SenderClient.java : Receiver 서버로의 연산 결과 파일 전송 담당
          - S3Service.java : 대용량 연산 파일 백업 및 스토리지 관리
      - native/ : C++ Core 소스 코드 및 빌드 환경 (CMake)
      - resources/ : 웹 프론트엔드 대시보드 템플릿 및 설정 파일 (application.yaml)
  - settings.gradle : Gradle 프로젝트 설정 파일

### 세션 및 요청별 데이터 관리 (인프라 스토리지)
서버는 각 Receiver의 요청 세션별로 독립된 공간을 확보하여 아래 파일들을 격리 관리합니다.

- parms.bin (방향: Receiver -> Sender) : BFV 암호화 파라미터
- public_key.bin (방향: Receiver -> Sender) : BFV 공개키
- relin_key.bin (방향: Receiver -> Sender) : Relinearization 키
- powers.bin (방향: Receiver -> Sender) : 윈도잉 암호화 결과 거듭제곱 데이터
- result.bin (방향: Sender Native -> Receiver) : Sender의 다항식 연산 결과 암호문
- sender_timing.json (방향: Sender -> Receiver) : Sender 측 세부 연산 성능 지표 측정 파일

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
CSV 파일을 읽어 데이터를 로드하고 패킹을 수행합니다.

**입력 CSV 형식 (Sender):**
personal_id, Disease_1, Disease_2, ..., Disease_10, Status
920624-2447527, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1

**데이터 패킹 방식:**
주민등록번호(13자리) + 질병코드(10비트)를 하나의 uint64_t로 패킹합니다.
packed = (pid_decimal << 10) | disease_bits

---

### hashing - 단순 해싱 (Simple Hashing)
Sender의 데이터를 해시 테이블에 배치합니다. MurmurHash3를 해시 함수로 사용하며, h개의 해시 함수를 이용해 각 아이템을 h개의 모든 버킷에 중복하여 매핑하는 단순 해싱 방식을 사용합니다.

---

### evaluate - 파티셔닝 및 다항식 평가 (FHE Evaluation)
Receiver가 전송한 암호화된 거듭제곱 값을 사용하여, 암호화된 상태 그대로 다항식을 연산하는 핵심 엔진입니다.

**동적 파티셔닝 (Dynamic Partitioning):**
계산 복잡도를 낮추기 위해 각 버킷의 아이템들을 alpha개의 파티션으로 분할하여 관리합니다. 각 파티션은 독립적인 다항식을 구성하게 됩니다. 이때 alpha 값은 고정되지 않고, 서버의 작업 큐 혼잡도에 따라 연산 유연성을 확보할 수 있도록 가변적으로 적용됩니다.

**다항식 계수 계산:**
각 파티션에 속한 명제들을 근으로 갖는 다항식의 평문 계수(Coefficient)들을 오버플로우 없이 모듈러 산술을 통해 도출합니다.

**거듭제곱 암호문 생성 및 평가:**
Receiver의 가변 윈도잉 암호문(powers.bin) 사양과 매칭하여 0승부터 최대 차수까지의 모든 암호문을 구성한 뒤, 계수와 곱하여 암호화된 다항식 평가 결과(result.bin)를 최종 합성합니다.

---

### mod - 오버플로우 방지 모듈러 연산
Windows 환경에서 MSVC 컴파일러가 unsigned __int128을 지원하지 않으므로, 비트 분할 방식으로 모듈러 곱셈과 거듭제곱을 구현하여 연산 오버플로우를 원천 방지합니다.

- safe_mul_mod(a, b, m) : 오버플로우 없이 (a * b) % m 계산
- power_mod(base, exp, mod) : 오버플로우 없이 base^exp % mod 계산

---

## 실행 순서 (Web API 워크플로우)

### 1단계: Receiver 요청 수신 및 자원 체크
Receiver 웹 서버가 통신 세션을 시작하기 전, Sender 서버의 작업 큐 혼잡도 상태를 체크하고 현재 연산에 바인딩할 파라미터(alpha, windowing 등) 정보를 조율합니다.

### 2단계: 암호문 컨텍스트 로드 및 비동기 연산 가동
Receiver로부터 암호화 파라미터 및 윈도잉 거듭제곱 파일들이 수신되면 API 컨트롤러가 동작합니다.

**내부 동작:**
1. 전달받은 parms.bin, public_key.bin, relin_key.bin, powers.bin을 전용 스토리지에 세션별로 저장합니다.
2. Java FFM API를 통해 libsender-psi.so의 핵심 연산 엔진을 다운콜(Downcall) 형태로 호출합니다.
3. 대용량 동형암호 다항식 연산의 특성을 고려하여 `NativeAsync` 모듈이 이를 비동기 쓰레드로 안전하게 격리하여 가동합니다.
4. 단순 해싱 배치, 파티셔닝 처리, 암호화 상태에서의 다항식 평가 연산이 차례로 수행됩니다.

### 3단계: 연산 결과 반환 및 통계 수집
연산이 정상적으로 완료되면 생성된 결과 파일(`result.bin`)과 Sender단 측정 통계 지표(`sender_timing.json`)를 취합하여 Receiver 서버의 결과 수신 엔드포인트로 안전하게 전송합니다.

---

## 의존성
- Microsoft SEAL : BFV 완전동형암호 연산
- MurmurHash3 : 단순 해싱용 해시 함수
- Java 22+ (FFM API) : 고성능 C++ 공유 라이브러리 연동 및 메모리 제어

---

## 구동 환경 및 서버 실행 방법 (AWS / 운영 환경)

### JVM 옵션 지정 및 Native 접근 허용
Java FFM API를 활용한 원활한 공유 라이브러리 다운콜을 위해, 실행 시 반드시 외부 네이티브 메모리 접근 권한을 명시해야 합니다. build.gradle에 해당 설정이 반영되어 있습니다.

**애플리케이션 빌드 명령어:**
./gradlew clean bootJar

**JVM 필수 옵션을 포함한 서비스 가동 명령어:**
java --enable-native-access=ALL-UNNAMED -jar build/libs/sender-0.0.1-SNAPSHOT.jar

---

## 입력 데이터 형식
data/sender.csv 파일의 형식은 다음과 같습니다.

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
