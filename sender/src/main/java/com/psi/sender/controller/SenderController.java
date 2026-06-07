package com.psi.sender.controller;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.GetMapping;

@Controller
public class SenderController {
    @GetMapping("/sender/dashboard")
    public String dashboard() {
        return "dashboard";
    }

    @GetMapping("/sender/metadata")
    public String metadata() {
        return "metadata";
    }
}
