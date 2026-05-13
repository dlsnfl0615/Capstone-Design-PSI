package com.psi.sender.controller;

import com.psi.sender.service.BinFileTransfer;
import com.psi.sender.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestPart;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;

@RestController
@RequiredArgsConstructor
public class SenderController {
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
    private static final Path RESULT = Paths.get("storage/result.bin").toAbsolutePath();
    private final NativeService nativeService;
    private final BinFileTransfer binFileTransfer;

    @PostMapping("/api/files/upload")
    public String load(@RequestPart("binFiles") List<MultipartFile> binFiles) throws IOException {
        Files.createDirectories(STORAGE_DIR);

        for (MultipartFile file : binFiles) {
            String filename = file.getOriginalFilename();
            if (filename == null || filename.isBlank()) {
                throw new IllegalArgumentException("파일 이름이 없습니다.");
            }
            Path dest = STORAGE_DIR.resolve(Paths.get(filename).getFileName());
            file.transferTo(dest);
        }

        int result = nativeService.intersect();

        binFileTransfer.sendBinFile(STORAGE_DIR.resolve(RESULT));

        return "교집합 연산 완료. 반환 코드: " + result;
    }
}
