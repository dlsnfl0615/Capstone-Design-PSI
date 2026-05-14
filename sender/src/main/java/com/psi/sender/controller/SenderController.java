package com.psi.sender.controller;

import com.psi.sender.service.FileTransfer;
import com.psi.sender.service.NativeService;
import lombok.RequiredArgsConstructor;
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
public class SenderController {
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
    private static final Path RESULT = Paths.get("storage/result.bin").toAbsolutePath();
    private static final Path CPP_TIMING = Paths.get("storage/cpp_timing.json").toAbsolutePath();
    private final NativeService nativeService;
    private final FileTransfer fileTransfer;

    @PostMapping("/files/upload")
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

        return "수신 완료";
    }

    @PostMapping("/product")
    public String product() throws IOException {
        int result = nativeService.intersect();

        long transferNs = fileTransfer.sendBinFile(STORAGE_DIR.resolve(RESULT));
        double transferMs = transferNs / 1000000.0;

        String json = Files.readString(CPP_TIMING);
        String updated = json.substring(0, json.lastIndexOf('}'))
                + String.format(",\"transferMs\":%.3f}", transferMs);
        Files.writeString(CPP_TIMING, updated);

        return "교집합 연산 완료. 반환 코드: " + result;
    }

    @PostMapping("/timing")
    public void sendTiming() {
        fileTransfer.sendJsonFile(CPP_TIMING);
    }
}
