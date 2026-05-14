package com.psi.receiver.domain;

import lombok.Getter;
import lombok.Setter;

@Getter
@Setter
public class Time {
    // Phase 1 — request(): C++ 측정
    private double keygenMs;
    private double hashingMs;
    private double windowingMs;

    // Phase 2 — send(): Java 측정
    private double transferMs;

    // Phase 3 — result(): C++ 측정
    private double loadMs;
    private double decryptMs;
    private double intersectMs;
}