package com.psi.receiver.service;

import lombok.RequiredArgsConstructor;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.core.io.FileSystemResource;
import org.springframework.http.MediaType;
import org.springframework.http.client.MultipartBodyBuilder;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.BodyInserters;
import org.springframework.web.reactive.function.client.WebClient;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

@Service
@RequiredArgsConstructor // webClient 자동 주입
public class BinFileTransfer {
    private static final String SENDER_URL = "http://43.202.123.98:8081";
    private final WebClient webClient;

    /** 윈도잉 결과(powers.bin), 객체 생성에 필요한 키를 각각 따로 호출해서 전송 */
    public long sendBinFiles(List<Path> filePaths) {
        MultipartBodyBuilder builder = new MultipartBodyBuilder();

        for (Path path : filePaths) {
            builder.part("binFiles", new FileSystemResource(path));
        }

        long startTime = System.nanoTime();
        String response = webClient.post()
                .uri("/files/powers-keys")
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
}