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
    private static final String SENDER_URL = "http://43.202.123.98:8081";
    private final WebClient webClient;

    public BinFileTransfer() {
        this.webClient = WebClient.builder().baseUrl(SENDER_URL).build();
    }

    public long sendBinFiles(List<Path> filePaths) {
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
                throw new RuntimeException("File read failed: " + path, e);
            }
        });

        long start = System.nanoTime();
        webClient.post()
                .uri("/api/files/upload")
                .contentType(MediaType.MULTIPART_FORM_DATA)
                .body(BodyInserters.fromMultipartData(builder.build()))
                .retrieve()
                .bodyToMono(String.class)
                .doOnSuccess(response -> System.out.println("Transfer success: " + response))
                .doOnError(error -> System.err.println("Transfer failed: " + error.getMessage()))
                .block();
        return System.nanoTime() - start;
    }
}