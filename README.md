# Capstone-Design-PSI (Private Set Intersection)

본 프로젝트는 BFV 완전동형암호(FHE, Fully Homomorphic Encryption)와 뻐꾸기 해싱(Cuckoo Hashing) 및 단순 해싱(Simple Hashing) 기법을 융합하여 구현한 프라이버시 보호 교집합 계산(PSI) 시스템입니다.

두 당사자(Receiver와 Sender)가 각자의 민감한 데이터를 데이터 원본이나 식별 가능한 정보 노출 없이, 오직 공통으로 보유한 교집합 데이터만을 안전하게 추출할 수 있도록 설계되었습니다. 연산 집약적인 동형암호 및 해싱 엔진은 **C++ Core Native 라이브러리**로 처리하고, 다중 사용자 세션 관리, 파일 입출력 및 네트워크 제어는 **Spring Boot 웹 서버** 인프라로 처리하는 효율적인 하이브리드 아키텍처를 가집니다.

---

## 🚀 전체 아키텍처 및 동작 흐름

전체 PSI 프로토콜은 Receiver(수신자) 웹 서버와 Sender(송신자) 웹 서버 간의 API 통신 및 데이터 스트리밍을 통해 3단계의 하이브리드 워크플로우로 진행됩니다.

### 1단계: Receiver - 데이터 로드, 해싱 및 윈도잉 암호화
- Receiver 사용자가 웹 대시보드를 통해 원본 CSV 파일을 업로드합니다.
- 서버는 주민등록번호(하이픈 자동 제거 후 13자리 숫자)와 10개의 질병 코드 이진 플래그를 결합하여 하나의 uint64_t 데이터로 패킹합니다.
- 순열 기반 뻐꾸기 해싱(Permutation-based Cuckoo Hashing)을 수행하여 테이블을 배치합니다. 충돌 발생 시 최대 500회 Kick-out을 수행합니다.
- Java FFM(Foreign Function 및 Memory) API를 활용해 내부 Native 라이브러리(`libreceiver-psi.so`)를 호출하고, 다항식 평가를 위한 윈도잉 거듭제곱 암호화 연산을 유발합니다.
- 생성된 암호화 컨텍스트 파일들(parms.bin, public_key.bin, relin_key.bin, powers.bin)을 WebClient를 통해 Sender 웹 서버로 자동 전송합니다.

### 2단계: Sender - 단순 해싱, 파티셔닝 및 암호화 다항식 평가
- Sender 웹 서버는 Receiver의 혼잡도 체크를 통과한 요청에 대해 세션별 격리 스토리지 공간을 구성하고 수신된 암호문 파일들을 로드합니다.
- Sender는 자신의 입력 CSV 데이터를 동일한 사양으로 패킹한 후, h(값: 4)개의 해시 함수를 사용하는 단순 해싱(Simple Hashing)을 통해 아이템을 매핑합니다.
- 연산 복호도를 비약적으로 낮추기 위해 각 버킷의 아이템들을 alpha(값: 64)개의 파티션으로 분할하는 파티셔닝(Partitioning) 기법을 적용합니다.
- 오버플로우 방지 모듈러 산술 연산을 기반으로 각 파티션의 다항식 평문 계수를 계산합니다.
- Receiver의 윈도잉 암호문과 결합하여, Sender의 원본 데이터를 노출하지 않은 채 암호화된 상태 그대로 다항식을 연산(FHE Evaluation)합니다.
- 최종 연산 결과 암호문인 `result.bin`과 Sender 측 세부 성능 측정 JSON을 Receiver 서버로 다시 전송합니다.

### 3단계: Receiver - 최종 복호화 및 교집합 판별
- Receiver 웹 서버는 수신한 `result.bin`을 로드하여 Native 라이브러리의 `result` 함수를 다운콜 호출합니다.
- 보유하고 있던 비밀키(Secret Key)로 암호문을 복호화하고 배치 디코딩을 수행합니다.
- **교집합 판별 원리**: Sender는 자신의 집합 S를 근으로 하는 다항식 P(x)를 구성했으므로, 복호화 결과 디코딩된 슬롯 값이 0인 위치가 y가 S의 원소(y ∈ S)인 교집합 위치에 해당합니다.
- 최종 식별된 인덱스를 뻐꾸기 해시 테이블과 대조하여 매칭된 원본 데이터를 추출하고, 사용자에게 `intersections.csv` 다운로드 명세를 제공합니다.

---

## 📂 프로젝트 레포지토리 및 브랜치 구조

본 시스템은 상호 협업하는 독립된 웹 소프트웨어 인프라로 구성되어 있으며, 각 컴포넌트의 상세 구현 및 설치 사양은 개별 브랜치의 README 문서를 참고하시기 바랍니다.

- **main 브랜치 (현재 위치)**
  - 전체 완전동형암호(FHE) PSI 하이브리드 프로토콜의 통합 동작 흐름 기술
  - 다중 세션 격리 기반 엔드 투 엔드 네트워크 워크플로우 아키텍처 명세

- **receiver 브랜치**
  - Spring Boot 기반 수신자 웹 인프라 및 전역 예외 처리(GlobalExceptionHandler) 레이어
  - Java 22+ Foreign Function 및 Memory API를 적용한 `libreceiver-psi.so` 연동 모듈
  - 주민등록번호 패킹 유틸, 뻐꾸기 해싱 알고리즘 엔진 및 윈도잉 기반 거듭제곱 암호화 모듈 내장

- **sender 브랜치**
  - Spring Boot 기반 송신자 웹 인프라 및 비동기 스레드 연산 제어(NativeAsync) 아키텍처
  - 단순 해싱(Simple Hashing), 계산 최적화용 파티셔닝(Partitioning) 알고리즘 모듈 내장
  - 암호화된 동형암호 상태에서의 대용량 다항식 계수 계산 및 평가(Evaluation) 엔진 내장

---

## 🔒 시스템 전역 보안 파라미터 요약

본 PSI 프로토콜 아키텍처는 동형암호 연산 성능 최적화와 학술적 안전성을 동시에 유지하기 위해 전용 parameters.h 명세에 따른 아래 전역 설정을 공통 준수합니다.

- BFV 다항식 차수 (n) : 32,768
- 평문 모듈러스 비트 길이 (t) : 44 bits
- 아이템 최대 비트 길이 (sigma) : 43 bits
- 보안 수준 (lambda) : 40 비트
- 뻐꾸기 해싱 해시 함수 수 (h) : 3
- Sender 빈당 배열 길이 (B) : 9960

---

## 🛠️ 클라우드 및 운영 서버 구동 환경 설정

### 1. 인프라 공통 제약 사양
- Java 실행 환경: 각 가상 머신(AWS EC2 등) 또는 도커 컨테이너는 **Java 22 이상**의 가동 환경을 필수로 요구합니다. (프로젝트 빌드는 Java 25 툴체인 기준)
- 핵심 라이브러리 연동: Microsoft SEAL 및 MurmurHash3 환경에서 대상 OS(Linux x64 환경 등)에 맞게 컴파일된 C++ 공유 라이브러리(`libreceiver-psi.so`, `libsender-psi.so`) 파일이 각각의 웹 서버 라이브러리 경로 내에 사전에 안전하게 배치되어야 합니다.

### 2. 구동 시 JVM 필수 매개변수 설정
본 시스템은 고성능 Foreign Function 및 Memory API를 기반으로 네이티브 다이렉트 메모리를 참조하므로, 서비스 구동 시 반드시 외부 접근 허용 옵션을 적용해야 정상 작동합니다.

(빌드 명령어)
./gradlew clean bootJar

(운영 가동 명령어)
java --enable-native-access=ALL-UNNAMED -jar build/libs/호스트명-0.0.1-SNAPSHOT.jar
EOF
