package com.psi.sender.service;

import io.awspring.cloud.s3.S3Resource;
import io.awspring.cloud.s3.S3Template;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.*;

import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.List;

@Service
@RequiredArgsConstructor
public class S3Service {
    private final S3Template s3Template;
    private final S3Client s3Client;

    public void uploadFilesToS3(List<MultipartFile> binFiles, String sessionId, String bucketName, String s3Prefix) throws Exception {
        for (MultipartFile file : binFiles) {
            if (file.isEmpty()) continue;

            // MultipartFile에서 원래 파일명 추출
            String fileName = file.getOriginalFilename();
            // S3 최종 객체 키(경로) 생성
            String s3Key = s3Prefix + fileName;

            // 파일의 내부 입력 스트림을 열어 s3Template으로 전달
            try (InputStream inputStream = file.getInputStream()) {
                s3Template.upload(bucketName, s3Key, inputStream);
            }
            System.out.println("upload to s3 completed: " + s3Key);
        }
    }

//    public boolean hasAllRequiredFiles(String bucketName, String s3Prefix, List<String> requiredFiles) {
//        List<S3Resource> resources = s3Template.listObjects(bucketName, s3Prefix);
//        Set<String> uploadedFileNames = resources.stream()
//                .map(r -> Paths.get(r.getFilename()).getFileName().toString())
//                .collect(Collectors.toSet());
//        return uploadedFileNames.containsAll(requiredFiles);
//    }

    public void downloadDirectoryFromS3(String s3Prefix, String dockerPath, String bucketName) throws Exception {
        List<S3Resource> resources = s3Template.listObjects(bucketName, s3Prefix);
        Path targetDir = Paths.get(dockerPath);

        for (S3Resource resource : resources) {
            // 전체 경로에서 맨 마지막 파일명만 추출함
            String pureFilename = Paths.get(resource.getFilename()).getFileName().toString();
            System.out.println("pureFilename = " + pureFilename);

            if (pureFilename.isEmpty()) continue;

            Path localFilePath = targetDir.resolve(pureFilename);

            try (InputStream inputStream = resource.getInputStream()) {
                Files.copy(inputStream, localFilePath, StandardCopyOption.REPLACE_EXISTING);
            }
            System.out.println("S3 files download completed: " + resource.getLocation());
        }
    }
}
