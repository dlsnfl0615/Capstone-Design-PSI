package com.psi.sender.service;

import lombok.RequiredArgsConstructor;
import org.json.JSONObject;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;
import org.springframework.util.FileSystemUtils;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.CompletableFuture;

@Service
@RequiredArgsConstructor
public class NativeAsync {
    private final NativeService nativeService;
    private final SenderClient senderClient;
    private final Uploader uploader;

    private static final String BUCKET_NAME = "capstone-design-sender-bucker-684494100299-ap-northeast-2-an";

    /**
     * 다항식 연산 후 결과 반환하는 함수
     * */
    @Async("psiExecutor")
    public CompletableFuture<String> productPolynomial(Path storageDir, String sessionId, int alpha, int windowing, String s3PreprocessPrefix) throws Exception {
        System.out.println("[Sender] Background operation started...");

        Path sessionPath = Paths.get(storageDir.toString(), sessionId);
        Path csv = Paths.get("storage/B_sender_50M.csv");
        Path resultDir = Paths.get(storageDir.toString(), sessionId, "result.bin");
        Path cppTiming = Paths.get(storageDir.toString(), sessionId, "sender_timing.json");

        // 로컬 세션 디렉토리 생성 및 S3 파일 다운로드
        Files.createDirectories(sessionPath);
        uploader.downloadDirectoryFromS3("storage/sessions/" + sessionId, sessionPath.toString(), BUCKET_NAME);
        uploader.downloadDirectoryFromS3(s3PreprocessPrefix, sessionPath.toString(), BUCKET_NAME);

        // 다항식 연산
        int result = nativeService.intersect(storageDir.toString() + "/" + sessionId, csv.toString(), alpha, windowing);
        System.out.println("[Sender C++] Operation completed. Return code: " + result);

        if (result != 0) {
            throw new RuntimeException("[Sender C++] intersect failed with code: " + result);
        }

        // 다항식 결과 전송 걸리는 시간
        long transferNs = senderClient.sendBinFile(resultDir);

        // 시간 저장
        String content = new String(Files.readAllBytes(cppTiming));
        JSONObject senderTiming = new JSONObject(content);
        senderTiming.put("transferResult", Math.round(transferNs / 1000000.0 * 1000.0) / 1000.0);
        Files.writeString(cppTiming, senderTiming.toString());

        System.out.println("[Sender] Operation timing data file update complete");

        // 연산 시간 전송
        senderClient.sendJsonFile(cppTiming);

        System.out.println("[Sender] Final execution time, file transfer completed");

        FileSystemUtils.deleteRecursively(Paths.get(storageDir + "/" + sessionId).toFile()); // 도커에 있는 sessions 폴더 통째로 삭제해서 다음 작업 진행

        return CompletableFuture.completedFuture("c++ execution completed");
    }
}
