package com.psi.receiver.controller;

import com.psi.receiver.component.SessionFiles;
import jakarta.servlet.http.HttpSession;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileTime;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.Optional;

@Controller
@RequestMapping("/receiver")
@RequiredArgsConstructor
public class ReceiverController {
    private final SessionFiles sessionFiles;
    private static final Path STORAGE_DIR = Paths.get("storage/sessions").toAbsolutePath();

    // 로그인 화면 이동
    @GetMapping("/login")
    public String loginPage() {
        return "login";
    }

    // 홈(대시보드) 화면 이동
    @GetMapping("/home")
    public String homePage() {
        return "home";
    }

    // 검증 업로드 화면 이동
    @GetMapping("/upload")
    public String uploadPage() {
        return "upload";
    }

    // 검증 진행 중 화면 이동
    @GetMapping("/processing")
    public String processingPage(HttpSession session, Model model) {
        String fileNameToShow = "업로드된 파일이 없습니다."; // 기본 메시지 세팅
        Path sessionDir = Paths.get(STORAGE_DIR.toString(), session.getId());
        long fileSize = 0; //MB 단위로

        try {
            // 디렉터리가 존재하고 내부를 읽을 수 있는지 확인
            if (Files.exists(sessionDir) && Files.isDirectory(sessionDir)) {

                // 폴더 내 파일 스트림을 열어 확장자가 .csv인 파일 필터링 진행
                Optional<Path> latestCsvFile = Files. list(sessionDir)
                        .filter(f -> f.toString().toLowerCase().endsWith(".csv")) // csv 파일만 추출
                        .max((f1, f2) -> Long.compare(f1.toFile().lastModified(), f2.toFile().lastModified())); // 가장 최근 수정된 파일 정렬

                // 매칭되는 파일이 존재하면 파일 이름 추출
                if (latestCsvFile.isPresent()) {
                    fileNameToShow = latestCsvFile.get().getFileName().toString(); // 순수 파일명 변환 저장
                    fileSize = Files.size(latestCsvFile.get());
                    sessionFiles.put(session.getId(), fileNameToShow);
                }
            }
        } catch (IOException e) {
            e.printStackTrace(); // 예외 발생 시 콘솔 출력
            fileNameToShow = "파일명을 읽어오는 중 오류가 발생했습니다."; // 에러 메시지 치환
        }

        // 최종 결정된 파일 이름을 타임리프에 전달
        model.addAttribute("csvName", fileNameToShow); // 타임리프의 ${fileName} 변수와 매핑
        model.addAttribute("sessionId", session.getId());
        model.addAttribute("fileSize", Math.round(fileSize / 1000000.0 * 100.0) / 100.0);

        return "processing";
    }

    // 검증 완료 결과 화면 이동
    @GetMapping("/result")
    public String resultPage(HttpSession session, Model model) throws IOException {
        String sessionId = session.getId();

        String fileName = sessionFiles.get(sessionId);
        Path orgCsvPath = Paths.get(STORAGE_DIR.toString(), sessionId, fileName);

        Path mismatchCsvPath = Paths.get(STORAGE_DIR.toString(), sessionId, "intersections.csv");

        long orgLength = Files.lines(orgCsvPath).count() - 1; // 헤더 빼기
        long mismatchLength = Files.lines(mismatchCsvPath).count();
        long matchCount = orgLength - mismatchLength;
        long mismatchSizeBytes = Files.size(mismatchCsvPath);
        double ratio = (double)matchCount / orgLength * 100;

        BasicFileAttributes attrs = Files.readAttributes(mismatchCsvPath, BasicFileAttributes.class);
        FileTime time = attrs.creationTime();
        String createdTime = LocalDateTime
                .ofInstant(time.toInstant(), ZoneId.of("Asia/Seoul"))
                .format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));

        model.addAttribute("orgLength", orgLength);
        model.addAttribute("mismatchLength", mismatchLength);
        model.addAttribute("mismatchSizeBytes", Math.round(mismatchSizeBytes / 1000.0));
        model.addAttribute("ratio", Math.round(ratio * 100.0) / 100.0);
        model.addAttribute("createdTime", createdTime);

        return "result";
    }
}
