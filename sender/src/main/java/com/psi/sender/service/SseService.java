package com.psi.sender.service;

import org.springframework.stereotype.Service;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

@Service
public class SseService {
    private final AtomicReference<SseEmitter> emitterRef = new AtomicReference<>();

    public SseEmitter connect() {
        SseEmitter emitter = new SseEmitter(180_000L);

        SseEmitter old = emitterRef.getAndSet(emitter);
        if (old != null) {
            old.complete();
        }

        emitter.onCompletion(() -> emitterRef.compareAndSet(emitter, null));
        emitter.onTimeout(() -> emitterRef.compareAndSet(emitter, null));

        return emitter;
    }

    public void send(String eventName, Object data) {
        SseEmitter emitter = emitterRef.get();
        if (emitter == null) return;

        try {
            emitter.send(SseEmitter.event().name(eventName).data(data));
        } catch (IOException e) {
            emitterRef.compareAndSet(emitter, null);
        }
    }

    public boolean isConnected() {
        return emitterRef.get() != null;
    }
}
