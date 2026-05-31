#!/bin/sh

# 1. 컨테이너 내부에 storage 폴더가 없으면 생성
mkdir -p /app/storage

echo "진행: S3에서 동기화 시작..."

aws s3 sync s3://capstone-design-sender-bucker-684494100299-ap-northeast-2-an/alpha128-l6/ /app/storage/

echo "💡 [디버깅] /app/storage/ 내부 실제 파일 목록:"
ls -al /app/storage/

echo "완료: S3 동기화 성공. 스프링 부트를 시작함."

# 3. 스프링 부트 애플리케이션 실행
exec java --enable-native-access=ALL-UNNAMED -Djava.library.path=/app/libs -jar /app/application.jar