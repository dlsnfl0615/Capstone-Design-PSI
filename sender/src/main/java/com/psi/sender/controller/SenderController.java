package com.psi.sender.controller;

import com.psi.sender.service.NativeAsync;
import com.psi.sender.service.Uploader;
import lombok.RequiredArgsConstructor;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.http.ResponseEntity;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

@RestController
@RequiredArgsConstructor
@CrossOrigin(origins = "*", allowedHeaders = "*")
public class SenderController {
    private final NativeAsync nativeAsync;
    private final Uploader uploader;
    @Autowired
    @Qualifier("psiExecutor")
    private ThreadPoolTaskExecutor psiExecutor;// thread queue 크기 확인 용도
//    private SseEmitter clientReceiver;
    private static final int MAX_QUEUE_SIZE = 3;
    private static final int DEFAULT_WINDOWING = 4;
    private static final int DEFAULT_ALPHA = 512;
    private static final int CONGESTION_WINDOWING = 7;
    private static final int CONGESTION_ALPHA = 128;
    private static final Path STORAGE_DIR = Paths.get("storage/sessions").toAbsolutePath();
    private static final String bucketName = "capstone-design-sender-bucker-684494100299-ap-northeast-2-an";
    private static final String CONGESTED_DIR = "congested-queue/";
    private static final String DEFAULT_DIR = "default-queue/";
    private static final List<String> REQUIRED_FILES = List.of("parms.bin", "public_key.bin", "relin_key.bin", "powers.bin");

    /** sender의 thread queue의 혼잡 상태 확인
     * 큐에 일정 개수 이상의 작업이 쌓이면 곱셈 깊이를 1로 줄여야 함
     * true: 혼잡 상태임. receiver에서 l을 7로
     * false: 혼잡 상태 아님. receiver에서 l을 4로 */
    @GetMapping("/parameters/congestion")
    public List<Integer> checkThreadQueue() {
        return getParms();
    }

    /** receiver에게서 파일 받아서 s3에 올리고 바로 다항식 연산 시작 */
    @PostMapping("/files/result-polynomial")
    public ResponseEntity<Map<String, String>> load(
            @RequestParam("sessionId") String sessionId,
            @RequestPart("binFiles") List<MultipartFile> binFiles
    ) {
        String s3Prefix = String.format("storage/sessions/%s/", sessionId);

        // receiver에게서 받은 파일 s3 버킷으로 업로드
        try {
            uploader.uploadFilesToS3(binFiles, sessionId, bucketName, s3Prefix);
        } catch (Exception e) {
            System.err.println("[Sender Error] uploadFilesToS3 error: " + e.getMessage());
            return ResponseEntity.internalServerError().build();
        }

        // 필수 파일이 모두 S3에 올라왔는지 확인 (receiver가 2번에 나눠 보내므로)
        if (!uploader.hasAllRequiredFiles(bucketName, s3Prefix, REQUIRED_FILES)) {
            Map<String, String> partial = new HashMap<>();
            partial.put("status", "UPLOADED");
            partial.put("sessionId", sessionId);
            partial.put("message", "files uploaded, waiting for remaining files.");
            return ResponseEntity.ok(partial);
        }

        try {
            String localBasePath = STORAGE_DIR + "/" + sessionId;

            // docker 로컬에 해당 세션 전용 디렉토리 생성
            Files.createDirectories(Paths.get(localBasePath));

            // s3의 sessions/{sessionId}/ 경로에 있는 파일들(receiver에게 받은 파일)을 로컬로 다운로드
            String s3InputPrefix = "storage/sessions/" + sessionId;
            uploader.downloadDirectoryFromS3(s3InputPrefix, localBasePath, bucketName);

            // s3에 있는 전처리 파일을 곱셈 깊이에 맞게 가져옴
            String s3PreprocessPrefix = isCongested() ? CONGESTED_DIR : DEFAULT_DIR;
            uploader.downloadDirectoryFromS3(s3PreprocessPrefix, localBasePath, bucketName);

        } catch (Exception e) {
            System.err.println("[Sender Error] S3 download error: " + e.getMessage());
            return ResponseEntity.internalServerError().build();
        }

        // 다항식 연산 시작
        try {
            int alpha = getParms().getFirst();
            int windowing = getParms().getLast();
            CompletableFuture<String> futureResult = nativeAsync.productPolynomial(STORAGE_DIR, sessionId, alpha, windowing);
            futureResult
                    .thenAccept(_ -> System.out.println("job completed."))
                    .exceptionally(e -> {
                        System.err.println("[Sender Error] Async task failed: " + e.getMessage());
                        e.printStackTrace();
                        return null;
                    });
        } catch (Exception e) {
            System.err.println("[Sender Error] Error occurred during background computation and transmission: " + e.getMessage());
            return ResponseEntity.internalServerError().build();
        }

        Map<String, String> response = new HashMap<>();
        response.put("status", "ACCEPTED");
        response.put("sessionId", sessionId);
        response.put("message", "request accepted.");

        return ResponseEntity.accepted().body(response);
    }

    private boolean isCongested() {
        int queueSize = psiExecutor.getQueueSize();

        if (queueSize > MAX_QUEUE_SIZE) {
            return true;
        }

        return false;
    }

    private List<Integer> getParms() {
        if (isCongested()) {
            return List.of(CONGESTION_ALPHA, CONGESTION_WINDOWING);
        }

        return List.of(DEFAULT_ALPHA, DEFAULT_WINDOWING);
    }

//    @GetMapping(value = "/connect", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
//    public SseEmitter connect() {
//        this.clientReceiver = new SseEmitter(60 * 1000L); // 1분간 연결 유지
//        return this.clientReceiver;
//    }

//    /** c++ 연산 시간 전송 */
//    @PostMapping("/timing")
//    public String sendTiming() {
//        fileTransfer.sendJsonFile(CPP_TIMING);
//
//        return "product and transfer time send completed";
//    }
}
