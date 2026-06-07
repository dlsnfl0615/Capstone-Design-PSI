package com.psi.sender.service;

import org.springframework.stereotype.Service;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.nio.file.Paths;

@Service
public class NativeService {
    private static final String FUNCTION_NAME = "intersect";
    Path libraryPath = Paths.get("/app/libs/libsender-psi.so");

    public int callNative(String storageDir, String senderCsv, int alpha, int windowing) {
        System.out.println("callNative: " + FUNCTION_NAME + " storageDir=" + storageDir + " senderCsv=" + senderCsv);

        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup lookup = SymbolLookup.libraryLookup(libraryPath, arena);
            Linker linker = Linker.nativeLinker();

            MemorySegment funcSegment = lookup.find(FUNCTION_NAME)
                    .orElseThrow(() -> new RuntimeException("Native function search failed: " + FUNCTION_NAME));

            MethodHandle handle = linker.downcallHandle(
                    funcSegment,
                    FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS, ValueLayout.JAVA_INT, ValueLayout.JAVA_INT)
            );

            MemorySegment storageDirSeg = arena.allocateFrom(storageDir);
            MemorySegment senderCsvSeg = arena.allocateFrom(senderCsv);

            return (int) handle.invokeExact(storageDirSeg, senderCsvSeg, alpha, windowing);
        } catch (Throwable e) {
            throw new RuntimeException("Native function execution failed: " + FUNCTION_NAME, e);
        }
    }

    public int intersect(String storageDir, String senderCsv, int alpha, int windowing) {
        return callNative(storageDir, senderCsv, alpha, windowing);
    }

//    public int hashing(String storageDir, String senderCsv) {
//        return callNative("preprocess", storageDir, senderCsv);
//    }
}
