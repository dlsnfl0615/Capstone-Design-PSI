package com.psi.sender.component;

import org.springframework.stereotype.Component;

import java.util.concurrent.ConcurrentHashMap;

@Component
public class RequestId {
    private ConcurrentHashMap<String, String> map = new ConcurrentHashMap<>();

    public void put(String sessionId) {

    }

    private void getRequestId(String sessionId) {

    }
}
