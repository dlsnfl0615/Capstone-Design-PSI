package com.psi.receiver.service;

import org.springframework.stereotype.Service;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.nio.file.Paths;

@Service
public class NativeService {
    private static final Path LIB_PATH =
            Paths.get("src/main/native/build/Release/receiver-psi.dll").toAbsolutePath();
    static {
        // zstd.dll은 Windows DLL 탐색 경로에 없으므로 sender-psi.dll 로딩 전에 먼저 로드
        Path zstdPath = Paths.get("src/main/native/build/Release/zstd.dll").toAbsolutePath();
        System.load(zstdPath.toString());
    }
    public int callNative(String functionName) {
        System.out.println("callNative");
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

    public int request() {
        return callNative("request");
    }

    public int result() {
        return callNative("result");
    }
}
