package com.psi.receiver.controller;

import com.psi.receiver.service.BinFileTransfer;
import com.psi.receiver.service.NativeService;
import com.psi.receiver.service.TimingEditor;
import lombok.RequiredArgsConstructor;
import org.json.JSONException;
import org.json.JSONObject;
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
@CrossOrigin(origins = "*", allowedHeaders = "*")
public class ReceiverController {
    private final NativeService nativeService;
    private final BinFileTransfer binFileTransfer;
    private final TimingEditor timingEditor;
    private JSONObject timing = new JSONObject();

    private static final Path PUBLIC_KEY = Paths.get("storage/public_key.bin").toAbsolutePath();
    private static final Path POWERS = Paths.get("storage/powers.bin").toAbsolutePath();
    private static final Path PARMS = Paths.get("storage/parms.bin").toAbsolutePath();
    private static final Path HASH_TABLE = Paths.get("storage/receiver_hash.bin").toAbsolutePath();
    private static final Path RELIN_KEY = Paths.get("storage/relin_key.bin").toAbsolutePath();
    private static final Path SECRET_KEY = Paths.get("storage/secret_key.bin").toAbsolutePath();
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
    private static final Path RECEIVER_CSV = Paths.get("storage/B_receiver_5300.csv").toAbsolutePath();
    private static final Path TIMING_JSON = Paths.get("storage/timing.json").toAbsolutePath();
    private static final Path SENDER_TIMING_JSON = Paths.get("storage/sender_timing.json").toAbsolutePath();

    /** Phase 1: 암호화 요청 생성 (keygen + hashing + windowing) */
    @PostMapping("/process")
    public String request() throws JSONException {
        Map<String, Double> t = nativeService.request(STORAGE_DIR.toString(), RECEIVER_CSV.toString());
        JSONObject receiverRequest = new JSONObject();
        receiverRequest.put("hashing", t.get("hashingMs"));
        receiverRequest.put("windowing", t.get("windowingMs"));
        timing.put("receiver", receiverRequest);

        return String.format("request completed (hashing=%.1fms, windowing=%.1fms)",
                receiverRequest.getDouble("hashing"), receiverRequest.getDouble("windowing"));
    }

    /** Phase 2: Sender로 bin 파일 전송 */
    @PostMapping("/send")
    public String send() throws JSONException {
        long transferNs = binFileTransfer.sendBinFiles(
                List.of(PUBLIC_KEY, POWERS, PARMS, HASH_TABLE, RELIN_KEY, SECRET_KEY));
        JSONObject transfer = timing.getJSONObject("receiver");
        transfer.put("receiverToSender", Math.round(transferNs / 1_000_000.0 * 1000.0) / 1000.0);
        timing.put("receiver", transfer);

        return String.format("Transfer completed (transfer=%.1fms)", transfer.getDouble("receiverToSender"));
    }

    /** Phase 2-b: Sender로부터 result.bin 수신 */
    @PostMapping("/files/result")
    public String loadResult(@RequestPart("binFile") MultipartFile binFile) throws IOException {
        Files.createDirectories(STORAGE_DIR);
        String filename = binFile.getOriginalFilename();
        if (filename == null || filename.isBlank()) {
            throw new IllegalArgumentException("No such file name.");
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
    public String check() throws IOException, JSONException {
        Map<String, Double> t = nativeService.result(STORAGE_DIR.toString(), RECEIVER_CSV.toString());
        JSONObject result = timing.getJSONObject("receiver");
        result.put("load", t.get("loadMs"));
        result.put("decrypt", t.get("decryptMs"));
        result.put("intersect", t.get("intersectMs"));
        timing.put("receiver", result);

        timingEditor.combineJson(TIMING_JSON, timing, SENDER_TIMING_JSON);

        return String.format(
                "intersect completed (load=%.1fms, decrypt=%.1fms, intersect=%.1fms)",
                result.getDouble("load"), result.getDouble("decrypt"), result.getDouble("intersect"));
    }
}
