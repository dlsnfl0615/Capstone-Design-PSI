package com.psi.receiver.service;

import org.springframework.stereotype.Service;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.nio.file.Paths;

@Service
public class NativeService {
    public void callNative(String functionName, String inputPath, String outputPath) {
        Path libPath = Paths.get("src/main/native/build/Release/request.dll").toAbsolutePath(); // receiver-request.cpp의 파일명
        System.load(libPath.toString()); // c++ 로드

        Linker linker = Linker.nativeLinker();
        SymbolLookup lookup = SymbolLookup.loaderLookup(); // 심볼 룩업 인스턴스 생성

        // C++ 함수 찾기. receiver-request.cpp의 request 함수 찾음.
        MemorySegment funcSegment = lookup.find("request").orElseThrow(() -> new RuntimeException("함수 탐색 실패"));

        MethodHandle readFileHandle = linker.downcallHandle(
                funcSegment,
                FunctionDescriptor.ofVoid(ValueLayout.ADDRESS)
        );
    }
}
