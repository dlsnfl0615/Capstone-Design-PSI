package com.psi.receiver.service;

import org.springframework.stereotype.Service;

import java.io.IOException;
import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

@Service
public class NativeService {
    private static final Path LIB_PATH =
            Paths.get("src/main/native/build/Release/receiver-psi.dll").toAbsolutePath();
    private static final Path CPP_TIMING_PATH =
            Paths.get("storage/cpp_timing.json").toAbsolutePath();

    static {
        // zstd.dll은 Windows DLL 탐색 경로에 없으므로 sender-psi.dll 로딩 전에 먼저 로드
        Path zstdPath = Paths.get("src/main/native/build/Release/zstd.dll").toAbsolutePath();
        System.load(zstdPath.toString());
        Path zlib1Path = Paths.get("src/main/native/build/Release/zlib1.dll").toAbsolutePath();
        System.load(zlib1Path.toString());
    }

    private int callNative(String functionName) {
        System.out.println("callNative: " + functionName);
        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup lookup = SymbolLookup.libraryLookup(LIB_PATH, arena);
            Linker linker = Linker.nativeLinker();

            MemorySegment funcSegment = lookup.find(functionName)
                    .orElseThrow(() -> new RuntimeException("함수 탐색 실패: " + functionName));

            MethodHandle handle = linker.downcallHandle(
                    funcSegment,
                    FunctionDescriptor.of(ValueLayout.JAVA_INT)
            );

            return (int) handle.invoke();
        } catch (Throwable e) {
            throw new RuntimeException("네이티브 함수 실행 실패: " + functionName, e);
        }
    }

    /**
     * C++이 기록한 storage/cpp_timing.json을 읽어 Map으로 반환.
     * 형식: {"key1":123.456,"key2":78.9,...}
     */
    private Map<String, Double> readCppTiming() {
        try {
            String json = Files.readString(CPP_TIMING_PATH).trim();
            // 중괄호 제거 후 "key":value 쌍 파싱
            json = json.substring(1, json.length() - 1);
            Map<String, Double> map = new HashMap<>();
            for (String pair : json.split(",")) {
                String[] kv = pair.split(":");
                String key = kv[0].trim().replace("\"", "");
                double value = Double.parseDouble(kv[1].trim());
                map.put(key, value);
            }
            return map;
        } catch (IOException e) {
            throw new RuntimeException("cpp_timing.json 읽기 실패", e);
        }
    }

    public Map<String, Double> request() {
        int code = callNative("request");
        if (code != 0) {
            throw new RuntimeException("request() 실패 (반환값: " + code + ")");
        }
        return readCppTiming();
    }

    public Map<String, Double> result() {
        int code = callNative("result");
        if (code != 0) {
            throw new RuntimeException("result() 실패 (반환값: " + code + ")");
        }
        return readCppTiming();
    }
}