package com.psi.receiver.controller;

import com.psi.receiver.service.NativeService;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Controller;

@Controller
@RequiredArgsConstructor
public class ReceiverController {
    private final NativeService nativeService;

    public void run() {
        nativeService.request();
    }
}
