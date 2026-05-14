package com.psi.sender.service;

import org.springframework.stereotype.Service;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.nio.file.Paths;

@Service
public class NativeService {
    private static final Path LIB_PATH = Paths.get("src/main/native/build/Release/sender-psi.dll").toAbsolutePath();
    static {
        // zstd.dll은 Windows DLL 탐색 경로에 없으므로 sender-psi.dll 로딩 전에 먼저 로드
        Path zstdPath = Paths.get("src/main/native/build/Release/zstd.dll").toAbsolutePath();
        System.load(zstdPath.toString());
        Path zlib1Path = Paths.get("src/main/native/build/Release/zlib1.dll").toAbsolutePath();
        System.load(zlib1Path.toString());
    }

    public int callNative(String functionName, String storageDir, String senderCsv) {
        System.out.println("callNative: " + functionName + " storageDir=" + storageDir + " senderCsv=" + senderCsv);

        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup lookup = SymbolLookup.libraryLookup(LIB_PATH, arena);
            Linker linker = Linker.nativeLinker();

            MemorySegment funcSegment = lookup.find(functionName)
                    .orElseThrow(() -> new RuntimeException("함수 탐색 실패: " + functionName));

            MethodHandle handle = linker.downcallHandle(
                    funcSegment,
                    FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS)
            );

            MemorySegment storageDirSeg = arena.allocateFrom(storageDir);
            MemorySegment senderCsvSeg = arena.allocateFrom(senderCsv);

            return (int) handle.invokeExact(storageDirSeg, senderCsvSeg);
        } catch (Throwable e) {
            throw new RuntimeException("네이티브 함수 실행 실패: " + functionName, e);
        }
    }

    public int intersect(String storageDir, String senderCsv) {
        return callNative("intersect", storageDir, senderCsv);
    }
}
