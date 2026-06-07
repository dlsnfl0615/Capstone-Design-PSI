package com.psi.receiver.controller;

import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Optional;

@Controller
@RequestMapping("/receiver")
public class ReceiverController {
    private static final Path STORAGE_DIR = Paths.get("storage").toAbsolutePath();
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
    public String processingPage(Model model) {
        String fileNameToShow = "업로드된 파일이 없습니다."; // 기본 메시지 세팅

        try {
            // 디렉터리가 존재하고 내부를 읽을 수 있는지 확인
            if (Files.exists(STORAGE_DIR) && Files.isDirectory(STORAGE_DIR)) {

                // 폴더 내 파일 스트림을 열어 확장자가 .csv인 파일 필터링 진행
                Optional<Path> latestCsvFile = Files. list(STORAGE_DIR)
                        .filter(f -> f.toString().toLowerCase().endsWith(".csv")) // csv 파일만 추출
                        .max((f1, f2) -> Long.compare(f1.toFile().lastModified(), f2.toFile().lastModified())); // 가장 최근 수정된 파일 정렬

                // 매칭되는 파일이 존재하면 파일 이름 추출
                if (latestCsvFile.isPresent()) {
                    fileNameToShow = latestCsvFile.get().getFileName().toString(); // 순수 파일명 변환 저장
                    System.out.println("fileNameToShow = " + fileNameToShow);
                }
            }
        } catch (IOException e) {
            e.printStackTrace(); // 예외 발생 시 콘솔 출력
            fileNameToShow = "파일명을 읽어오는 중 오류가 발생했습니다."; // 에러 메시지 치환
        }

        // 최종 결정된 파일 이름을 타임리프에 전달
        model.addAttribute("csvName", fileNameToShow); // 타임리프의 ${fileName} 변수와 매핑

        return "processing";
    }

    // 검증 완료 결과 화면 이동
    @GetMapping("/result")
    public String resultPage() {
        return "result";
    }
}
