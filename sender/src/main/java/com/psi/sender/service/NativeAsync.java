package com.psi.sender.service;

import lombok.RequiredArgsConstructor;
import org.json.JSONObject;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.CompletableFuture;

@Service
@RequiredArgsConstructor
public class NativeAsync {
    private final NativeService nativeService;
    private final FileTransfer fileTransfer;

    @Async
    public CompletableFuture<String> productPolynomial(Path storageDir, Path csv, Path resultDir, Path cppTiming) {
        System.out.println("[Sender] Background operation started...");

        try {
            // 다항식 연산
            int result = nativeService.intersect(storageDir.toString(), csv.toString());
            System.out.println("[Sender C++] Operation completed. Return code: " + result);

            // 다항식 결과 전송 걸리는 시간
            long transferNs = fileTransfer.sendBinFile(resultDir);

            // 시간 저장
            String content = new String(Files.readAllBytes(cppTiming));
            JSONObject senderTiming = new JSONObject(content);
            senderTiming.put("transferResult", Math.round(transferNs / 1000000.0 * 1000.0) / 1000.0);
            Files.writeString(cppTiming, senderTiming.toString());

            System.out.println("[Sender] Operation timing data file update complete");

            // 연산 시간 전송
            fileTransfer.sendJsonFile(cppTiming);

            System.out.println("[Sender] Final execution time, file transfer completed");

            return CompletableFuture.completedFuture("c++ execution completed");

        } catch (Exception e) {
            System.err.println("[Sender Error] Error occurred during background computation and transmission: " + e.getMessage());
            return CompletableFuture.failedFuture(e);
        }
    }
}
