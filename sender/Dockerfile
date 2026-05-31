# 1단계: C++ 프로젝트 빌드
FROM ubuntu:24.04 AS cpp-builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git \
    && ./vcpkg/bootstrap-vcpkg.sh \
    && ./vcpkg/vcpkg install seal

WORKDIR /build-cpp
COPY src/main/native/CMakeLists.txt ./
COPY src/main/native/src/ ./src/
COPY src/main/native/external/ ./external/

RUN cmake -B build -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    && cmake --build build --config Release


# 2단계: 자바 환경을 빌려오기 위한 임시 스테이지 생성
FROM amazoncorretto:25 AS java-source


# 3단계: 최종 실행 환경 (Ubuntu 24.04 베이스)
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# OpenMP 런타임 및 AWS CLI 설치를 위한 curl/unzip 패키지 설치
RUN apt-get update && apt-get install -y \
    libgomp1 \
    curl \
    unzip \
    && rm -rf /var/lib/apt/lists/*

RUN curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" \
    && unzip awscliv2.zip \
    && ./aws/install \
    && rm -rf awscliv2.zip ./aws

# java-source 스테이지에서 자바 설치 디렉토리를 그대로 복사해옴
COPY --from=java-source /usr/lib/jvm/java-25-amazon-corretto /opt/corretto

# 복사해온 자바 경로를 환경변수로 등록함
ENV JAVA_HOME=/opt/corretto
ENV PATH="${JAVA_HOME}/bin:${PATH}"

WORKDIR /app

# C++ 라이브러리 및 종속성 복사
COPY --from=cpp-builder /build-cpp/build/libsender-psi.so /app/libs/
COPY --from=cpp-builder /opt/vcpkg/installed/x64-linux/lib/*.so* /usr/local/lib/
RUN ldconfig

COPY storage /app/storage

# 스프링 부트 JAR 파일 복사 (이름을 application.jar로 고정하여 복사)
ARG JAR_FILE=build/libs/*-SNAPSHOT.jar
COPY ${JAR_FILE} application.jar

# 💡 진입점 스크립트 복사 및 실행 권한 추가
COPY entrypoint.sh entrypoint.sh
RUN chmod +x entrypoint.sh

EXPOSE 8081

# 💡 ENTRYPOINT를 entrypoint.sh로 변경하여 스크립트가 먼저 실행되도록 제어함
ENTRYPOINT ["./entrypoint.sh"]