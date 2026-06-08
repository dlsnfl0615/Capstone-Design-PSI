package com.psi.receiver.component;

import org.springframework.stereotype.Component;

import java.util.concurrent.ConcurrentHashMap;

@Component
public class SessionFiles {
    private final ConcurrentHashMap<String, String> map = new ConcurrentHashMap<>();

    public void put(String sessionId, String sessionFileName) {
        map.put(sessionId, sessionFileName);
    }

    public String get(String sessionId) {
        return map.get(sessionId);
    }

    public void remove(String sessionId) {
        map.remove(sessionId);
    }
}
