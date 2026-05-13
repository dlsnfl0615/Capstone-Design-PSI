package com.psi.receiver.service;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class NativeServiceTest {
    NativeService nativeService = new NativeService();

    @Test
    void 빌드_테스트() {
        nativeService.request();
    }
}