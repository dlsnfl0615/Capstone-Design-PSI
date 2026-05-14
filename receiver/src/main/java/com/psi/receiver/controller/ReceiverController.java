package com.psi.receiver.controller;

import com.psi.receiver.domain.Time;
import com.psi.receiver.service.BinFileTransfer;
import com.psi.receiver.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api")
@RequiredArgsConstructor
public class ReceiverController {
    private final NativeService nativeService;
    private final BinFileTransfer binFileTransfer;
    private final Time time = new Time();

    private static final Path PUBLIC_KEY  = Paths.get("storage/public_key.bin").toAbsolutePath();
    private static final Path POWERS      = Paths.get("storage/powers.bin").toAbsolutePath();
    private static final Path PARMS       = Paths.get("storage/parms.bin").toAbsolutePath();
    private static final Path HASH_TABLE  = Paths.get("storage/receiver_hash.bin").toAbsolutePath();
    private static final Path RELIN_KEY   = Paths.get("storage/relin_key.bin").toAbsolutePath();
    private static final Path SECRET_KEY  = Paths.get("storage/secret_key.bin").toAbsolutePath();
    private static final Path STORAGE_DIR       = Paths.get("storage").toAbsolutePath();
    private static final Path TIMING_JSON       = Paths.get("storage/timing.json").toAbsolutePath();
    private static final Path SENDER_TIMING_JSON = Paths.get("storage/sender_timing.json").toAbsolutePath();

    /** Phase 1: 암호화 요청 생성 (keygen + hashing + windowing) */
    @PostMapping("/process")
    public String request() {
        Map<String, Double> t = nativeService.request();
        time.setKeygenMs(t.get("keygenMs"));
        time.setHashingMs(t.get("hashingMs"));
        time.setWindowingMs(t.get("windowingMs"));
        System.out.println("keygen: " + time.getKeygenMs() + ", hashing: " + time.getHashingMs() + ", windowing: " + time.getWindowingMs());
        return String.format("request 완료 (keygen=%.1fms, hashing=%.1fms, windowing=%.1fms)",
                time.getKeygenMs(), time.getHashingMs(), time.getWindowingMs());
    }

    /** Phase 2: Sender로 bin 파일 전송 */
    @PostMapping("/send")
    public String send() {
        long transferNs = binFileTransfer.sendBinFiles(
                List.of(PUBLIC_KEY, POWERS, PARMS, HASH_TABLE, RELIN_KEY, SECRET_KEY));
        time.setTransferMs(transferNs / 1_000_000.0);
        return String.format("전송 완료 (transfer=%.1fms)", time.getTransferMs());
    }

    /** Phase 2-b: Sender로부터 result.bin 수신 */
    @PostMapping("/files/result")
    public String loadResult(@RequestPart("binFile") MultipartFile binFile) throws IOException {
        Files.createDirectories(STORAGE_DIR);
        String filename = binFile.getOriginalFilename();
        if (filename == null || filename.isBlank()) {
            throw new IllegalArgumentException("파일 이름이 없습니다.");
        }
        Path dest = STORAGE_DIR.resolve(Paths.get(filename).getFileName());
        Files.write(dest, binFile.getBytes());
        return "교집합 결과 로드 완료.";
    }

    /** Phase 2-c: Sender로부터 sender 측 실행 시간 JSON 수신 */
    @PostMapping("/files/sender-timing")
    public String loadSenderTiming(@RequestBody String timingJson) throws IOException {
        Files.createDirectories(STORAGE_DIR);
        Files.writeString(SENDER_TIMING_JSON, timingJson);
        return "sender timing 수신 완료.";
    }

    /** Phase 3: 복호화 및 교집합 검증, 최종 timing.json 저장 */
    @PostMapping("/check")
    public String check() throws IOException {
        Map<String, Double> t = nativeService.result();
        time.setLoadMs(t.get("loadMs"));
        time.setDecryptMs(t.get("decryptMs"));
        time.setIntersectMs(t.get("intersectMs"));

        saveTimingJson();

        return String.format(
                "교집합 찾기 완료 (load=%.1fms, decrypt=%.1fms, intersect=%.1fms)",
                time.getLoadMs(), time.getDecryptMs(), time.getIntersectMs());
    }

    /** 누적된 receiver 타이밍 + sender_timing.json을 병합하여 storage/timing.json 저장 */
    private void saveTimingJson() throws IOException {
        String senderJson = Files.exists(SENDER_TIMING_JSON)
                ? Files.readString(SENDER_TIMING_JSON).trim()
                : "{}";

        String json = String.format(
                "{%n" +
                "  \"receiver\": {%n" +
                "    \"phase1_request\": {%n" +
                "      \"keygenMs\": %.3f,%n" +
                "      \"hashingMs\": %.3f,%n" +
                "      \"windowingMs\": %.3f%n" +
                "    },%n" +
                "    \"phase2_transfer\": {%n" +
                "      \"transferMs\": %.3f%n" +
                "    },%n" +
                "    \"phase3_result\": {%n" +
                "      \"loadMs\": %.3f,%n" +
                "      \"decryptMs\": %.3f,%n" +
                "      \"intersectMs\": %.3f%n" +
                "    }%n" +
                "  },%n" +
                "  \"sender\": %s%n" +
                "}",
                time.getKeygenMs(), time.getHashingMs(), time.getWindowingMs(),
                time.getTransferMs(),
                time.getLoadMs(), time.getDecryptMs(), time.getIntersectMs(),
                senderJson
        );
        Files.createDirectories(STORAGE_DIR);
        Files.writeString(TIMING_JSON, json);
        System.out.println("[timing] storage/timing.json 저장 완료");
    }
}