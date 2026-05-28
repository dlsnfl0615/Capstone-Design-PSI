package com.psi.sender.service;

import org.springframework.core.io.ByteArrayResource;
import org.springframework.core.io.FileSystemResource;
import org.springframework.http.MediaType;
import org.springframework.http.client.MultipartBodyBuilder;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.BodyInserters;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.publisher.Mono;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

@Service
public class FileTransfer {
    private static final String SENDER_URL = "http://3.34.152.64:8080";
    private final WebClient webClient;

    public FileTransfer() {
        this.webClient = WebClient.builder().baseUrl(SENDER_URL).build();
    }

    public long sendBinFile(Path filePath) {
        byte[] bytes;
        try {
            bytes = Files.readAllBytes(filePath);
        } catch (IOException e) {
            throw new RuntimeException("파일 읽기 실패: " + filePath, e);
        }

        MultipartBodyBuilder builder = new MultipartBodyBuilder();
        builder.part("binFile", new ByteArrayResource(bytes) {
            @Override
            public String getFilename() {
                return filePath.getFileName().toString();
            }
        }).contentType(MediaType.APPLICATION_OCTET_STREAM);

        long start = System.nanoTime();
        webClient.post()
                .uri("/api/files/result")
                .contentType(MediaType.MULTIPART_FORM_DATA)
                .body(BodyInserters.fromMultipartData(builder.build()))
                .retrieve()
                .bodyToMono(String.class)
                .doOnSuccess(response -> System.out.println("전송 성공: " + response))
                .doOnError(error -> System.err.println("전송 실패: " + error.getMessage()))
                .block();

        return System.nanoTime() - start;
    }

    public void sendJsonFile(Path filePath) {
        webClient.post()
                .uri("/api/files/sender-timing")
                .contentType(MediaType.APPLICATION_JSON)
                .body(BodyInserters.fromResource(new FileSystemResource(filePath))) // 리소스 삽입함
                .retrieve()
                .bodyToMono(String.class)
                .block();
    }
}