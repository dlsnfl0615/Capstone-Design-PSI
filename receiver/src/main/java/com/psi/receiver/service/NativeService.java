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
    Path libraryPath = Paths.get("/app/libs/libreceiver-psi.so");

    private int callNative(String functionName, String storageDir, String receiverCsv, int alpha, int windowing) {
        System.out.println("callNative: " + functionName + " storageDir=" + storageDir + " receiverCsv=" + receiverCsv);
        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup lookup = SymbolLookup.libraryLookup(libraryPath, arena);
            Linker linker = Linker.nativeLinker();

            MemorySegment funcSegment = lookup.find(functionName)
                    .orElseThrow(() -> new RuntimeException("Native function search failed: " + functionName));

            MethodHandle handle = linker.downcallHandle(
                    funcSegment,
                    FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS, ValueLayout.JAVA_INT, ValueLayout.JAVA_INT)
            );

            MemorySegment storageDirSeg = arena.allocateFrom(storageDir);
            MemorySegment receiverCsvSeg = arena.allocateFrom(receiverCsv);

            return (int) handle.invokeExact(storageDirSeg, receiverCsvSeg, alpha, windowing);
        } catch (Throwable e) {
            throw new RuntimeException("Native Function Execution Failed: " + functionName, e);
        }
    }

    /**
     * C++이 기록한 cpp_timing.json을 읽어 Map으로 반환.
     * 형식: {"key1":123.456,"key2":78.9,...}
     */
    private Map<String, Double> readCppTiming(String storageDir) {
        Path timingPath = Paths.get(storageDir).resolve("cpp_timing.json");
        try {
            String json = Files.readString(timingPath).trim();
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
            throw new RuntimeException("cpp_timing.json read failed", e);
        }
    }

    public Map<String, Double> request(String storageDir, String receiverCsvPath, int alpha, int windowing) {
        int code = callNative("request", storageDir, receiverCsvPath, alpha, windowing);
        if (code != 0) {
            throw new RuntimeException("request() failed (return code: " + code + ")");
        }
        return readCppTiming(storageDir);
    }

    public Map<String, Double> result(String storageDir, String receiverCsvPath, int alpha, int windowing) {
        int code = callNative("result", storageDir, receiverCsvPath, alpha, windowing);
        if (code != 0) {
            throw new RuntimeException("result() failed (return code: " + code + ")");
        }
        return readCppTiming(storageDir);
    }
}