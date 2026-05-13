package com.psi.receiver.service;

import org.springframework.core.io.ByteArrayResource;
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
public class BinFileTransfer {
    private static final String SENDER_URL = "http://localhost:8081";
    private final WebClient webClient;

    public BinFileTransfer() {
        this.webClient = WebClient.builder().baseUrl(SENDER_URL).build();
    }

    public void sendBinFiles(List<Path> filePaths) {
        MultipartBodyBuilder builder = new MultipartBodyBuilder();

        filePaths.forEach(path -> {
            try {
                byte[] bytes = Files.readAllBytes(path);
                builder.part("binFiles", new ByteArrayResource(bytes) {
                    @Override
                    public String getFilename() {
                        return path.getFileName().toString();
                    }
                }).contentType(MediaType.APPLICATION_OCTET_STREAM);
            } catch (IOException e) {
                throw new RuntimeException("파일 읽기 실패: " + path, e);
            }
        });

        webClient.post()
                .uri("/api/files/upload")
                .contentType(MediaType.MULTIPART_FORM_DATA)
                .body(BodyInserters.fromMultipartData(builder.build()))
                .retrieve()
                .bodyToMono(String.class)
                .doOnSuccess(response -> System.out.println("전송 성공: " + response))
                .doOnError(error -> System.err.println("전송 실패: " + error.getMessage()))
                .subscribe();
    }
}