package com.psi.sender.controller;

import com.psi.sender.service.FileTransfer;
import com.psi.sender.service.NativeAsync;
import com.psi.sender.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.json.JSONObject;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;
import java.util.concurrent.CompletableFuture;

@RestController
@RequiredArgsConstructor
@CrossOrigin(origins = "*", allowedHeaders = "*")
public class SenderController {
    private final NativeService nativeService;
    private final FileTransfer fileTransfer;
    private final NativeAsync nativeAsync;
    private SseEmitter clientReceiver;
    private JSONObject timing = new JSONObject();
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
    private static final Path RESULT = Paths.get("storage/result.bin").toAbsolutePath();
    private static final Path CPP_TIMING = Paths.get("storage/sender_timing.json").toAbsolutePath();
    private static final Path SENDER_CSV = Paths.get("storage/B_sender_50M.csv").toAbsolutePath();

    /** receiver에게서 윈도잉 결과와 객체 생성에 필요한 키 받아옴 */
    @PostMapping("/files/powers-keys")
    public String load(@RequestPart("binFiles") List<MultipartFile> binFiles) throws IOException {
        Files.createDirectories(STORAGE_DIR);

        for (MultipartFile file : binFiles) {
            String filename = file.getOriginalFilename();
            if (filename == null || filename.isBlank()) {
                throw new IllegalArgumentException("No such file: " + filename);
            }

            Path targetPath = STORAGE_DIR.resolve(filename);
            file.transferTo(targetPath.toFile());
        }

        return "powers and keys received.";
    }

    /** 다항식 연산 진행. 오래 걸리니까 비동기 처리로 */
    @PostMapping("/polynomial")
    public String product() {
        // c++ 코드 실행 비동기로 처리
        CompletableFuture<String> futureResult = nativeAsync.productPolynomial(STORAGE_DIR, SENDER_CSV, RESULT, CPP_TIMING);

        // 비동기 연산 끝나면 진행
        futureResult.thenAccept(result -> {
            if (clientReceiver != null) {
                try {
                    clientReceiver.send(SseEmitter.event().name("complete").data(result));
                } catch (Exception e) {
                    clientReceiver = null;
                }
            }
        });

        return "백그라운드에서 PSI 연산 시작";
    }

    @GetMapping(value = "/connect", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public SseEmitter connect() {
        this.clientReceiver = new SseEmitter(60 * 1000L); // 1분간 연결 유지
        return this.clientReceiver;
    }

    /** preprocess 과정은 로컬에서 진행하고 서버에 올리기 때문에 이 과정은 진행되지 않음 */
    @PostMapping("/preprocess")
    public String preprocess() throws IOException {
        int result = nativeService.hashing(STORAGE_DIR.toString(), SENDER_CSV.toString());

        return "교집합 연산 완료. 반환 코드: " + result;
    }

    /** c++ 연산 시간 전송 */
    @PostMapping("/timing")
    public String sendTiming() {
        fileTransfer.sendJsonFile(CPP_TIMING);

        return "product and transfer time send completed";
    }
}
