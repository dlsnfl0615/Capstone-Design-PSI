package com.psi.sender.component;

import com.psi.sender.domain.Parameters;
import org.springframework.stereotype.Component;

import java.util.concurrent.ConcurrentHashMap;

@Component
public class RequestParms {
    private final ConcurrentHashMap<String, Parameters> map = new ConcurrentHashMap<>();

    public void put(String id, int alpha, int windowing) {
        map.put(id, new Parameters(alpha, windowing));
    }

    public Parameters get(String id) {
        return map.get(id);
    }
}
