package com.psi.receiver.component;

import com.psi.receiver.domain.Parameters;
import org.springframework.stereotype.Component;

import java.util.concurrent.ConcurrentHashMap;

@Component
public class SessionParameters {
    private final ConcurrentHashMap<String, Parameters> map = new ConcurrentHashMap<>();

    public void put(String sessionId, int alpha, int windowing) {
        Parameters parameters = new Parameters();
        parameters.setAlpha(alpha);
        parameters.setWindowing(windowing);
        map.put(sessionId, parameters);
    }

    public Parameters get(String sessionId) {
        return map.get(sessionId);
    }

    public void remove(String sessionId) {
        map.remove(sessionId);
    }
}
