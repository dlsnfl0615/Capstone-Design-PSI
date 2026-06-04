package com.psi.sender.service;

import org.springframework.stereotype.Service;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.nio.file.Paths;

@Service
public class NativeService {
    Path libraryPath = Paths.get("/app/libs/libsender-psi.so");

    public int callNative(String functionName, String storageDir, String senderCsv) {
        System.out.println("callNative: " + functionName + " storageDir=" + storageDir + " senderCsv=" + senderCsv);

        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup lookup = SymbolLookup.libraryLookup(libraryPath, arena);
            Linker linker = Linker.nativeLinker();

            MemorySegment funcSegment = lookup.find(functionName)
                    .orElseThrow(() -> new RuntimeException("Native function search failed: " + functionName));

            MethodHandle handle = linker.downcallHandle(
                    funcSegment,
                    FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS)
            );

            MemorySegment storageDirSeg = arena.allocateFrom(storageDir);
            MemorySegment senderCsvSeg = arena.allocateFrom(senderCsv);

            return (int) handle.invokeExact(storageDirSeg, senderCsvSeg);
        } catch (Throwable e) {
            throw new RuntimeException("Native function execution failed: " + functionName, e);
        }
    }

    public int intersect(String storageDir, String senderCsv) {
        return callNative("intersect", storageDir, senderCsv);
    }

    public int hashing(String storageDir, String senderCsv) {
        return callNative("preprocess", storageDir, senderCsv);
    }
}
