package com.psi.sender.controller;

import com.psi.sender.service.SseService;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

@Controller
public class SseController {
    private final SseService sseService;

    public SseController(SseService sseService) {
        this.sseService = sseService;
    }

    // 페이지 로드 Model로 타임리프에 초기값 전달
    @GetMapping("/dashboard")
    public String dashboard(Model model) {
        model.addAttribute("connected", sseService.isConnected());
        return "dashboard";
    }

    // SSE 연결
    @GetMapping(value = "/sse", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public SseEmitter connect() {
        return sseService.connect();
    }

    // 커스텀 메시지 전송
    @PostMapping("/sse/message")
    public ResponseEntity<Void> sendMessage(@RequestParam String eventName,
                                            @RequestParam String data) {
        sseService.send(eventName, data);
        return ResponseEntity.ok().build();
    }
}
