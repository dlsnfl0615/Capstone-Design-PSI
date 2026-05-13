package com.psi.receiver.service;

import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;

@Service
public class BinFileTransfer {
    private static String SENDER_URL = "http://target-server.com";
    private final WebClient webClient;

    public BinFileTransfer(WebClient.Builder webClientBuilder) {
        this.webClient = webClientBuilder.baseUrl(SENDER_URL).build(); // 상대 서버 주소 설정
    }
}
