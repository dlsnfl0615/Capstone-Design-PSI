package com.psi.receiver.controller;

import com.psi.receiver.service.BinFileTransfer;
import com.psi.receiver.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestPart;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;

@RestController
@RequestMapping("/api")
@RequiredArgsConstructor
public class ReceiverController {
    private final NativeService nativeService;
    private final BinFileTransfer binFileTransfer;
    private static final Path PUBLIC_KEY = Paths.get("storage/public_key.bin").toAbsolutePath();
    private static final Path POWERS = Paths.get("storage/powers.bin").toAbsolutePath();
    private static final Path PARMS = Paths.get("storage/parms.bin").toAbsolutePath();
    private static final Path HASH_TABLE = Paths.get("storage/receiver_hash.bin").toAbsolutePath();
    private static final Path RELIN_KEY = Paths.get("storage/relin_key.bin").toAbsolutePath();
    private static final Path SECRET_KEY = Paths.get("storage/secret_key.bin").toAbsolutePath();
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();

    @PostMapping("/send")
    public String send() {
        binFileTransfer.sendBinFiles(List.of(PUBLIC_KEY, POWERS, PARMS, HASH_TABLE, RELIN_KEY, SECRET_KEY));
        return "전송 완료";
    }

    @PostMapping("/files/result")
    public String loadResult(@RequestPart("binFile") MultipartFile binFile) throws IOException {
        System.out.println("[loadResult] STORAGE_DIR: " + STORAGE_DIR);
        Files.createDirectories(STORAGE_DIR);

        String filename = binFile.getOriginalFilename();
        System.out.println("[loadResult] filename: " + filename + ", size: " + binFile.getSize());
        if (filename == null || filename.isBlank()) {
            throw new IllegalArgumentException("파일 이름이 없습니다.");
        }
        Path dest = STORAGE_DIR.resolve(Paths.get(filename).getFileName());
        System.out.println("[loadResult] dest: " + dest);
        Files.write(dest, binFile.getBytes());

        return "교집합 결과 로드.";
    }

    @PostMapping("/check")
    public String check() {
        int code = nativeService.result();
        return "교집합 찾기 완료. (반환값: " + code + ")";
    }
}
