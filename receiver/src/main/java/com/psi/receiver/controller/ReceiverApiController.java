package com.psi.receiver.controller;

import com.psi.receiver.component.SessionEmitter;
import com.psi.receiver.component.SessionParameters;
import com.psi.receiver.service.ReceiverClient;
import com.psi.receiver.service.NativeService;
import com.psi.receiver.service.TimingEditor;
import jakarta.servlet.http.HttpSession;
import lombok.RequiredArgsConstructor;
import org.json.JSONException;
import org.json.JSONObject;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;
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
public class ReceiverApiController {
    private final NativeService nativeService;
    private final ReceiverClient receiverClient;
    private final TimingEditor timingEditor;
    private final JSONObject timing = new JSONObject();
    private final SessionParameters sessionParameters;
    private final SessionEmitter sessionEmitter;
    private Path receiverCsv;
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
//    private static final Path TIMING_JSON = Paths.get("storage/timing.json").toAbsolutePath();
//    private static final Path SENDER_TIMING_JSON = Paths.get("storage/sender_timing.json").toAbsolutePath();

    /** receiver client에서 csv 파일 업로드 */
    @PostMapping("/csv")
    public ResponseEntity<?> uploadReceiverFile(@RequestParam("file") MultipartFile file,
                                                HttpSession sessionId) throws IOException {
        Path sessionBase = Paths.get(STORAGE_DIR.toString(), "sessions", sessionId.getId());
        Files.createDirectories(sessionBase);

        // 파일 유효성 검증
        if (file.isEmpty()) {
            return ResponseEntity.badRequest().body(Map.of("status", "fail", "message", "파일이 비어있습니다."));
        }

        try {
            // 파일명 추출 및 디렉터리와 결합하여 절대 경로 생성
            String originalFileName = file.getOriginalFilename();
            Path targetPath = Paths.get(sessionBase.toString(), originalFileName); // /app/storage/sesseions/{sessionId}/파일명.csv 형태로 결합
            receiverCsv = targetPath.toAbsolutePath();

            // 파일 쓰기 (기존 동일 파일명 존재 시 덮어쓰기)
            file.transferTo(targetPath.toFile());

            System.out.println("file saved in docker local: " + targetPath.toAbsolutePath()); // 로그 출력용

            return ResponseEntity.ok().body(Map.of(
                    "status", "success",
                    "message", "파일이 도커 내부 /app/storage에 안전하게 저장되었습니다."
            ));

        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.internalServerError().body(Map.of("status", "error", "message", "파일 스트림 읽기 실패"));
        }
    }

    /** 암호화 및 전송
     * 해싱 및 윈도잉 진행
     * 결과 전송
     */
    @PostMapping("/requests")
    public ResponseEntity<String> request(HttpSession session) throws JSONException, IOException {
        String sessionId = session.getId();

        Path sessionDir = Paths.get(STORAGE_DIR.toString(), "sessions", sessionId);
        System.out.println("sessionDir = " + sessionDir);

        // sender 서버의 작업 큐에 작업 몰렸는지 확인
        List<Integer> parameters = receiverClient.checkCongestion();
        sessionParameters.put(sessionId, parameters.getFirst(), parameters.getLast());
        int alpha = sessionParameters.get(sessionId).getAlpha();
        int windowing = sessionParameters.get(sessionId).getWindowing();

        // receiver 해싱 및 윈도잉
        Map<String, Double> t = nativeService.request(sessionDir.toString(), receiverCsv.toString(), alpha, windowing);

        // 시간 기록
        JSONObject receiverRequest = new JSONObject();
        receiverRequest.put("hashing", t.get("hashingMs"));
        receiverRequest.put("windowing", t.get("windowingMs"));
        timing.put("receiver", receiverRequest);

        // receiver 해싱 및 윈도잉 결과 sender로 전송
        String sendResult = sendResult(sessionId);

        return ResponseEntity.ok(String.format("request completed (hashing=%.1fms, windowing=%.1fms)\n%s",
                receiverRequest.getDouble("hashing"), receiverRequest.getDouble("windowing"), sendResult));
    }

    /** Sender로 bin 파일 전송 */
    public String sendResult(String sessionId) throws JSONException {
        // 윈도잉 결과 파일의 통신 속도 측정을 위해서 따로 보냄
        Path powers = Paths.get(STORAGE_DIR.toString(), "sessions/" + sessionId + "/powers.bin");
        Path publicKey = Paths.get(STORAGE_DIR.toString(), "sessions/" + sessionId + "/public_key.bin");
        Path relinKey = Paths.get(STORAGE_DIR.toString(), "sessions/" + sessionId + "/relin_key.bin");
        Path secretKey = Paths.get(STORAGE_DIR.toString(), "sessions/" + sessionId + "/secret_key.bin");
        Path parms = Paths.get(STORAGE_DIR.toString(), "sessions/" + sessionId + "/parms.bin");

        long transferFilesNs = receiverClient.sendBinFiles(List.of(powers, publicKey, relinKey, secretKey, parms), sessionId);
        JSONObject transfer = timing.getJSONObject("receiver");
        transfer.put("transferFilesMs", Math.round(transferFilesNs / 1_000_000.0 * 1000.0) / 1000.0);

        timing.put("receiver", transfer);

        return String.format("Transfer completed (transferFilesMs=%.3fms)", transfer.getDouble("transferFilesMs"));
    }

    @GetMapping("/status/stream/{sessionId}")
    public SseEmitter subscribe(@PathVariable String sessionId) {
        System.out.println("sessionId = " + sessionId);
        return sessionEmitter.subscribe(sessionId);
    }

    /** Sender로부터 sender 측 실행 시간 JSON과 result.bin 수신 */
    @PostMapping("/files/result")
    public String loadSenderTiming(
            @RequestParam("sessionId") String sessionId,
            @RequestPart("binFiles") List<MultipartFile> binFiles) throws IOException {
        Path sessionDir = STORAGE_DIR.resolve("sessions/" + sessionId);

        for (MultipartFile multipartFile : binFiles) {
            if (multipartFile.isEmpty()) {
                continue; // 비어있는 파일은 건너뜀
            }

            // 원본 파일명 추출
            String originalFileName = multipartFile.getOriginalFilename();

            Path targetPath = sessionDir.resolve(originalFileName);

            multipartFile.transferTo(targetPath.toFile());
        }
        sessionEmitter.sendCompletionMessage(sessionId);

        return "sender timing received.";
    }

    /** 교집합 검증 결과 csv 파일 다운로드 하기 */
    @GetMapping("/files/intersections")
    public ResponseEntity<Resource> downloadIntersections(HttpSession session) {
        String sessionId = session.getId();
        Path sessionDir = Paths.get(STORAGE_DIR.toString(), "sessions", sessionId, "intersections.csv");
        Resource resource = new FileSystemResource(sessionDir);
        return ResponseEntity.ok()
                .header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=\"intersections.csv\"")
                .contentType(MediaType.parseMediaType("text/csv"))
                .body(resource);
    }

    /** 복호화 및 교집합 검증, 최종 timing.json 저장 */
    @PostMapping("/intersections")
    public String check(HttpSession sessionId) throws IOException, JSONException {
        Path sessionDir = STORAGE_DIR.resolve("sessions/" + sessionId.getId());

        int alpha = sessionParameters.get(sessionId.getId()).getAlpha();
        int windowing = sessionParameters.get(sessionId.getId()).getWindowing();

        Map<String, Double> t = nativeService.result(sessionDir.toString(), receiverCsv.toString(), alpha, windowing);

        JSONObject result = timing.getJSONObject("receiver");
        result.put("load", t.get("loadMs"));
        result.put("decrypt", t.get("decryptMs"));
        result.put("intersect", t.get("intersectMs"));
        timing.put("receiver", result);

        Path timingJson = Paths.get(sessionDir.toString(), "timing.json").toAbsolutePath();
        Path senderTimingJson = Paths.get(sessionDir.toString(), "sender_timing.json").toAbsolutePath();

        timingEditor.combineJson(timingJson, timing, senderTimingJson);

        return String.format(
                "intersect completed (load=%.1fms, decrypt=%.1fms, intersect=%.1fms)",
                result.getDouble("load"), result.getDouble("decrypt"), result.getDouble("intersect"));
    }
}
