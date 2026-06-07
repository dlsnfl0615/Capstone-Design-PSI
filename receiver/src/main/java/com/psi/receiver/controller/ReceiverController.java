package com.psi.receiver.controller;

import com.psi.receiver.service.ReceiverClient;
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
import java.util.UUID;

@RestController
@RequiredArgsConstructor
@CrossOrigin(origins = "*", allowedHeaders = "*")
public class ReceiverController {
    private final NativeService nativeService;
    private final ReceiverClient receiverClient;
    private final TimingEditor timingEditor;
    private JSONObject timing = new JSONObject();
    private int alpha = 0;
    private int windowing = 0;

    private static final Path PUBLIC_KEY = Paths.get("storage/public_key.bin").toAbsolutePath();
    private static final Path POWERS = Paths.get("storage/powers.bin").toAbsolutePath();
    private static final Path PARMS = Paths.get("storage/parms.bin").toAbsolutePath();
    private static final Path RELIN_KEY = Paths.get("storage/relin_key.bin").toAbsolutePath();
    private static final Path SECRET_KEY = Paths.get("storage/secret_key.bin").toAbsolutePath();
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
    private static final Path RECEIVER_CSV = Paths.get("storage/B_receiver_5300.csv").toAbsolutePath();
    private static final Path TIMING_JSON = Paths.get("storage/timing.json").toAbsolutePath();
    private static final Path SENDER_TIMING_JSON = Paths.get("storage/sender_timing.json").toAbsolutePath();

    /** 암호화 및 전송
     * 해싱 및 윈도잉 진행
     * 결과 전송
     */
    @PostMapping("/requests")
    public String request() throws JSONException {
        String sessionId = UUID.randomUUID().toString();

        // sender 서버의 작업 큐에 작업 몰렸는지 확인
        List<Integer> parameters = receiverClient.checkCongestion();
        alpha = parameters.get(0);
        windowing = parameters.get(1);

        // receiver 해싱 및 윈도잉
        Map<String, Double> t = nativeService.request(STORAGE_DIR.toString(), RECEIVER_CSV.toString(), alpha, windowing);

        // 시간 기록
        JSONObject receiverRequest = new JSONObject();
        receiverRequest.put("hashing", t.get("hashingMs"));
        receiverRequest.put("windowing", t.get("windowingMs"));
        timing.put("receiver", receiverRequest);

        // receiver 해싱 및 윈도잉 결과 sender로 전송
        String sendResult = sendResult(sessionId);

        return String.format("request completed (hashing=%.1fms, windowing=%.1fms)\n%s",
                receiverRequest.getDouble("hashing"), receiverRequest.getDouble("windowing"), sendResult);
    }

    /** Sender로 bin 파일 전송 */
    public String sendResult(String sessionId) throws JSONException {
        // 윈도잉 결과 파일의 통신 속도 측정을 위해서 따로 보냄
        long transferPowersNs = receiverClient.sendBinFiles(List.of(POWERS), sessionId);
        long transferKeysNs = receiverClient.sendBinFiles(
                List.of(PUBLIC_KEY, RELIN_KEY, SECRET_KEY, PARMS), sessionId);
        JSONObject transfer = timing.getJSONObject("receiver");
        transfer.put("transferPowersMs", Math.round(transferPowersNs / 1_000_000.0 * 1000.0) / 1000.0);
        transfer.put("transferKeysMs", Math.round(transferKeysNs / 1_000_000.0 * 1000.0) / 1000.0);

        timing.put("receiver", transfer);

        return String.format("Transfer completed (transferPowersMs=%.3fms, transferKeysMs=%.3f)",
                transfer.getDouble("transferPowersMs"), transfer.getDouble("transferKeysMs"));
    }

    /** Sender로부터 result.bin 수신 */
    @PostMapping("/files/result")
    public String loadResult(@RequestPart("binFile") MultipartFile binFile) throws IOException {
        Files.createDirectories(STORAGE_DIR);
        String filename = binFile.getOriginalFilename();
        if (filename == null || filename.isBlank()) {
            throw new IllegalArgumentException("No such file name.");
        }

        Path targetPath = STORAGE_DIR.resolve(filename);
        binFile.transferTo(targetPath.toFile());

        return "intersect result received.";
    }

    /** Phase 2-c: Sender로부터 sender 측 실행 시간 JSON 수신 */
    @PostMapping("/files/sender-timing")
    public String loadSenderTiming(@RequestBody String timingJson) throws IOException {
        Files.createDirectories(STORAGE_DIR);
        Files.writeString(SENDER_TIMING_JSON, timingJson);

        return "sender timing received.";
    }

    /** Phase 3: 복호화 및 교집합 검증, 최종 timing.json 저장 */
    @PostMapping("/intersections")
    public String check() throws IOException, JSONException {
        Map<String, Double> t = nativeService.result(STORAGE_DIR.toString(), RECEIVER_CSV.toString(), alpha, windowing);
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
