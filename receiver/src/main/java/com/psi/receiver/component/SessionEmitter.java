package com.psi.receiver.component;

import org.springframework.stereotype.Component;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.concurrent.ConcurrentHashMap;

@Component
public class SessionEmitter {
    private final ConcurrentHashMap<String, SseEmitter> emitters = new ConcurrentHashMap<>();

    /** sessionId에 맞게 sse emitter 연결 */
    public SseEmitter subscribe(String sessionId) {
        SseEmitter emitter = new SseEmitter(30 * 60 * 1000L); // 만료 시간 30분

        // 연결이 정상 종료되거나 타임아웃 발생 시 맵에서 삭제 처리
        emitter.onCompletion(() -> emitters.remove(sessionId));
        emitter.onTimeout(() -> emitters.remove(sessionId));
        emitter.onError((e) -> emitters.remove(sessionId));

        emitters.put(sessionId, emitter);

        return emitter;
    }

    /** sessionId에 맞게 완료 메세지 전송 */
    public void sendCompletionMessage(String sessionId) {
        SseEmitter emitter = emitters.get(sessionId);

        if (emitter != null) {
            try {
                emitter.send(SseEmitter.event().name("done").data("complete"));
                // 완료 메시지 전송 후 SSE 스트림을 정상적으로 닫아줌
                emitter.complete();
            } catch (IOException e) {
                // 전송 실패 시 맵에서 제거
                emitters.remove(sessionId);
            }
        }
    }
}
