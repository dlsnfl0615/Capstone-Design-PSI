package com.psi.receiver.service;

import lombok.RequiredArgsConstructor;
import org.springframework.core.ParameterizedTypeReference;
import org.springframework.core.io.FileSystemResource;
import org.springframework.http.client.MultipartBodyBuilder;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.BodyInserters;
import org.springframework.web.reactive.function.client.WebClient;

import java.nio.file.Path;
import java.util.List;

@Service
@RequiredArgsConstructor // webClient 자동 주입
public class ReceiverClient {
    private final WebClient webClient;

    /** 윈도잉 결과(powers.bin), 객체 생성에 필요한 키를 각각 따로 호출해서 전송 */
    public long sendBinFiles(List<Path> filePaths, String sessionId) {
        MultipartBodyBuilder builder = new MultipartBodyBuilder();

        builder.part("sessionId", sessionId);

        for (Path path : filePaths) {
            builder.part("binFiles", new FileSystemResource(path));
        }

        long startTime = System.nanoTime();
        String response = webClient.post()
                .uri("/files/result-polynomial")
                .body(BodyInserters.fromMultipartData(builder.build()))
                .retrieve()
                .bodyToMono(String.class)
                .doOnSuccess(success -> System.out.println("receiver powers and key transfer success: " + success))
                .doOnError(error -> System.err.println("receiver powers and key transfer failed: " + error.getMessage()))
                .block();
        long endTime = System.nanoTime();

        System.out.println(response);

        return endTime - startTime;
    }

    public List<Integer> checkCongestion(String sessionId) {
        MultipartBodyBuilder builder = new MultipartBodyBuilder();
        builder.part("sessionId", sessionId);

        return webClient.post()
                .uri("/parameters/congestion")
                .body(BodyInserters.fromMultipartData(builder.build()))
                .retrieve()
                .bodyToMono(new ParameterizedTypeReference<List<Integer>>() {})
                .block();
    }
}