package com.psi.receiver.controller;

import com.psi.receiver.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Controller;
import org.springframework.web.reactive.function.client.WebClient;

@Controller
@RequiredArgsConstructor
public class ReceiverController {
    private final NativeService nativeService;
    private final WebClient webClient = WebClient.builder()
            .codecs(configurer -> configurer.defaultCodecs().maxInMemorySize(10 * 1024 * 1024)) // 10MB 제한 확장
            .build();
    public void run() {
        nativeService.request();
    }
}
