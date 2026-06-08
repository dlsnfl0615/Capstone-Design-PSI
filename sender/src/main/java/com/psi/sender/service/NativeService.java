package com.psi.sender.service;

import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Service;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.nio.file.Path;
import java.nio.file.Paths;

@Service
public class NativeService {
    private static SseService sseService;

    public NativeService(SseService sseService) {
        NativeService.sseService = sseService;
    }

    private static final String FUNCTION_NAME = "intersect";
    private static final Path libraryPath = Paths.get("/app/libs/libsender-psi.so");

    public int callNative(String storageDir, String senderCsv, int alpha, int windowing) {
        System.out.println("callNative: " + FUNCTION_NAME + " storageDir=" + storageDir + " senderCsv=" + senderCsv);

        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup lookup = SymbolLookup.libraryLookup(libraryPath, arena);
            Linker linker = Linker.nativeLinker();

            MemorySegment funcSegment = lookup.find(FUNCTION_NAME)
                    .orElseThrow(() -> new RuntimeException("Native function search failed: " + FUNCTION_NAME));

            // void callback(int progress, char* message)
            FunctionDescriptor callbackDesc = FunctionDescriptor.ofVoid(
                    ValueLayout.JAVA_INT,
                    ValueLayout.ADDRESS
            );

            // 호출될 static 메서드를 MethodHandle로 획득
            MethodHandle callbackHandle = MethodHandles.lookup().findStatic(
                    NativeService.class,
                    "onProgress",
                    MethodType.methodType(void.class, int.class, MemorySegment.class)
            );

            // Java 메소드 → 네이티브 함수 포인터 변환
            MemorySegment callbackPtr = linker.upcallStub(callbackHandle, callbackDesc, arena);

            // FunctionDescriptor에 ADDRESS(콜백 포인터) 추가
            MethodHandle handle = linker.downcallHandle(
                    funcSegment,
                    FunctionDescriptor.of(
                            ValueLayout.JAVA_INT,
                            ValueLayout.ADDRESS,  // storageDir
                            ValueLayout.ADDRESS,  // senderCsv
                            ValueLayout.JAVA_INT, // alpha
                            ValueLayout.JAVA_INT, // windowing
                            ValueLayout.ADDRESS   // callback 추가
                    )
            );

            MemorySegment storageDirSeg = arena.allocateFrom(storageDir);
            MemorySegment senderCsvSeg = arena.allocateFrom(senderCsv);

            return (int) handle.invokeExact(storageDirSeg, senderCsvSeg, alpha, windowing, callbackPtr);
        } catch (Throwable e) {
            throw new RuntimeException("Native function execution failed: " + FUNCTION_NAME, e);
        }
    }

    public int intersect(String storageDir, String senderCsv, int alpha, int windowing) {
        return callNative(storageDir, senderCsv, alpha, windowing);
    }

    public static void onProgress(int progress, MemorySegment messagePtr) {
        try {
            String message = messagePtr.reinterpret(256).getString(0);
            System.out.println("[C++ 콜백] progress=" + progress + " message=" + message);
            if (progress == 40) {
                sseService.send("parameter-validation", progress);
            } else if (progress == 60) {
                sseService.send("load-coeff", progress);
            } else if (progress == 80) {
                sseService.send("psi", progress);
            } else {
                System.err.println("[C++ 콜백] wrong progress: " + progress);
            }
        } catch (Exception e) {
            System.err.println("[C++ 콜백] error: " + e.getMessage());
        }
    }
}
