package com.psi.sender.service;

import lombok.RequiredArgsConstructor;
import org.springframework.core.io.FileSystemResource;
import org.springframework.http.MediaType;
import org.springframework.http.client.MultipartBodyBuilder;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.BodyInserters;
import org.springframework.web.reactive.function.client.WebClient;

import java.nio.file.Path;

@Service
@RequiredArgsConstructor
public class SenderClient {
    private final WebClient webClient;

    public long sendBinFile(Path filePath) {
        MultipartBodyBuilder builder = new MultipartBodyBuilder();
        builder.part("binFile", new FileSystemResource(filePath));

        long startTime = System.nanoTime();
        String response = webClient.post()
                .uri("/files/result")
                .body(BodyInserters.fromMultipartData(builder.build()))
                .retrieve()
                .bodyToMono(String.class)
                .doOnSuccess(success -> System.out.println("sender result transfer success: " + success))
                .doOnError(error -> System.err.println("sender result transfer failed: " + error.getMessage()))
                .block();
        long endTime = System.nanoTime();

        System.out.println(response);

        return endTime - startTime;
    }

    public void sendJsonFile(Path filePath) {
        webClient.post()
                .uri("/files/sender-timing")
                .contentType(MediaType.APPLICATION_JSON)
                .body(BodyInserters.fromResource(new FileSystemResource(filePath))) // 리소스 삽입함
                .retrieve()
                .bodyToMono(String.class)
                .doOnSuccess(success -> System.out.println("sender timing transfer success: " + success))
                .doOnError(error -> System.err.println("sender timing transfer failed: " + error.getMessage()))
                .block();
    }
}