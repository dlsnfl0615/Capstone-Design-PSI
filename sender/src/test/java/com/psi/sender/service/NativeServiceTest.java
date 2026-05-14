package com.psi.sender.service;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class NativeServiceTest {
    NativeService nativeService = new NativeService();

    @Test
    void 빌드_테스트() {
        String storageDir = java.nio.file.Paths.get("storage").toAbsolutePath().toString();
        String senderCsv = java.nio.file.Paths.get("storage/sender.csv").toAbsolutePath().toString();
        nativeService.intersect(storageDir, senderCsv);
    }
}