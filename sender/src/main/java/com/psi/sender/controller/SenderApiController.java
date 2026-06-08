package com.psi.sender.controller;

import com.psi.sender.service.NativeAsync;
import com.psi.sender.service.S3Service;
import com.psi.sender.service.SseService;
import lombok.RequiredArgsConstructor;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.http.ResponseEntity;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

@RestController
@RequiredArgsConstructor
@CrossOrigin(origins = "*", allowedHeaders = "*")
public class SenderApiController {
    private final NativeAsync nativeAsync;
    private final S3Service s3Service;
    private final SseService sseService;

    @Autowired
    @Qualifier("psiExecutor")
    private ThreadPoolTaskExecutor psiExecutor;// thread queue 크기 확인 용도

//    private SseEmitter clientReceiver;
    private static final int MAX_READY_QUEUE_SIZE = 1; // 첫 요청은 바로 처리되기 때문에 대기 큐에 쌓이지 않음. 따라서 0 -> 0 -> 1-> ...
    private static final int DEFAULT_WINDOWING = 4;
    private static final int DEFAULT_ALPHA = 512;
    private static final int CONGESTION_WINDOWING = 7;
    private static final int CONGESTION_ALPHA = 128;
    private static final Path STORAGE_DIR = Paths.get("storage/sessions").toAbsolutePath();
    private static final String bucketName = "capstone-design-sender-bucker-684494100299-ap-northeast-2-an";
    private static final String CONGESTED_DIR = "congested-queue/";
    private static final String DEFAULT_DIR = "default-queue/";

    /** sender의 thread queue의 혼잡 상태 확인
     * 큐에 일정 개수 이상의 작업이 쌓이면 곱셈 깊이를 1로 줄여야 함
     * true: 혼잡 상태임. receiver에서 l을 7로
     * false: 혼잡 상태 아님. receiver에서 l을 4로 */
    @GetMapping("/parameters/congestion")
    public List<Integer> checkThreadQueue() {
        List<Integer> parms = getParms();
        Integer alpha = parms.get(0);
        Integer windowing = parms.get(1);
        int queueSize = psiExecutor.getQueueSize();
        System.out.println("windowing = " + windowing + ", alpha = " + alpha + ", queueSize = " + queueSize);
        return parms;
    }

    /** receiver에게서 파일 받아서 s3에 올리고 바로 다항식 연산 시작 */
    @PostMapping("/files/result-polynomial")
    public ResponseEntity<Map<String, String>> load(
            @RequestParam("sessionId") String sessionId,
            @RequestPart("binFiles") List<MultipartFile> binFiles
    ) {
        // Current PSI Execution에서 첫번째
        sseService.send("request-received", 20);

        String s3Prefix = String.format("storage/sessions/%s/", sessionId);

        // receiver에게서 받은 파일 s3 버킷으로 업로드
        try {
            s3Service.uploadFilesToS3(binFiles, sessionId, bucketName, s3Prefix);
        } catch (Exception e) {
            System.err.println("[Sender Error] uploadFilesToS3 error: " + e.getMessage());
            return ResponseEntity.internalServerError().build();
        }

        // 다항식 연산 시작
        try {
            int alpha = getParms().getFirst();
            int windowing = getParms().getLast();
            String s3PreprocessPrefix = isCongested() ? CONGESTED_DIR : DEFAULT_DIR;
            CompletableFuture<String> futureResult = nativeAsync.productPolynomial(STORAGE_DIR, sessionId, alpha, windowing, s3PreprocessPrefix);
            futureResult
                    .thenAccept(_ -> {
                        System.out.println("job completed.");
                    })
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

        if (queueSize > MAX_READY_QUEUE_SIZE) {
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
