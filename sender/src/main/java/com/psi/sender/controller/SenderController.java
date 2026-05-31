package com.psi.sender.controller;

import com.psi.sender.service.FileTransfer;
import com.psi.sender.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.json.JSONObject;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;
import java.util.concurrent.CompletableFuture;

@RestController
@RequestMapping("/api")
@RequiredArgsConstructor
@CrossOrigin(origins = "*", allowedHeaders = "*")
public class SenderController {
    private JSONObject timing = new JSONObject();
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
    private static final Path RESULT = Paths.get("storage/result.bin").toAbsolutePath();
    private static final Path CPP_TIMING = Paths.get("storage/sender_timing.json").toAbsolutePath();
    private static final Path SENDER_CSV = Paths.get("storage/B_sender_50M.csv").toAbsolutePath();
    private final NativeService nativeService;
    private final FileTransfer fileTransfer;

    @PostMapping("/files/upload")
    public String load(@RequestPart("binFiles") List<MultipartFile> binFiles) throws IOException {
        Files.createDirectories(STORAGE_DIR);

        for (MultipartFile file : binFiles) {
            String filename = file.getOriginalFilename();
            if (filename == null || filename.isBlank()) {
                throw new IllegalArgumentException("No such file: " + filename);
            }
            Path dest = STORAGE_DIR.resolve(Paths.get(filename).getFileName());
            file.transferTo(dest);
        }

        return "수신 완료";
    }

    @PostMapping("/product")
    public String product() {
        System.out.println("[Sender] Background operation started...");

        // 비동기 스레드를 생성하여 367초 동안 걸리는 C++ 연산과 전송을 백그라운드에서 실행
        CompletableFuture.runAsync(() -> {
            try {
                // 1. C++ 무거운 다항식 교집합 연산 수행 (여기서 오래 걸려도 브라우저는 영향을 받지 않음)
                int result = nativeService.intersect(STORAGE_DIR.toString(), SENDER_CSV.toString());
                System.out.println("[Sender C++] Operation completed. Return code: " + result);

                // 2. 가공된 result.bin 파일을 receiver로 전송
                long transferNs = fileTransfer.sendBinFile(STORAGE_DIR.resolve(RESULT));
                double transferMs = transferNs / 1000000.0;

                // 3. cpp_timing.json 읽기 및 transferMs 추가 저장
                String content = new String(Files.readAllBytes(CPP_TIMING));
                JSONObject senderTiming = new JSONObject(content);
                senderTiming.put("transfer", String.format("%.3f", transferMs));
                System.out.println("[Sender] Operation timing data file update complete");

//                if (Files.exists(CPP_TIMING)) {
//                    String json = Files.readString(CPP_TIMING).trim();
//                    String updated = json.substring(0, json.lastIndexOf('}'))
//                            + String.format(",\"transferMs\":%.3f}", transferMs);
//                    Files.writeString(CPP_TIMING, updated);
//
//                }

                // 4. 연산 타이밍이 저장된 JSON 파일을 receiver 측으로 자동 최종 전송
                fileTransfer.sendJsonFile(CPP_TIMING);
                System.out.println("[Sender] Final execution time, file transfer completed");

            } catch (Exception e) {
                System.err.println("[Sender Error] Error occurred during background computation and transmission: " + e.getMessage());
                e.printStackTrace();
            }
        });

        // 백그라운드 스레드가 돌아가는 것과 관계없이, 요청을 보낸 브라우저에는 즉시 접수 메시지를 반환함
        return "백그라운드에서 PSI 연산 시작";
    }

    @PostMapping("/preprocess")
    public String preprocess() throws IOException {
        int result = nativeService.hashing(STORAGE_DIR.toString(), SENDER_CSV.toString());

        return "교집합 연산 완료. 반환 코드: " + result;
    }

    @PostMapping("/timing")
    public String sendTiming() {
        fileTransfer.sendJsonFile(CPP_TIMING);

//        return "연산 및 통신 시간 전송 완료.";
        return "product and transfer time send completed";
    }
}
