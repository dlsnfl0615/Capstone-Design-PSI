package com.psi.sender.advice;

import org.json.JSONException;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

@RestControllerAdvice
public class GlobalExceptionHandler {

    @ExceptionHandler(JSONException.class)
    public ResponseEntity<String> handleJSONException(JSONException e) {
        // 로그 출력 등 공통 작업 수행 가능
        return ResponseEntity
                .status(HttpStatus.BAD_REQUEST)
                .body("error occurred while parsing Json: " + e.getMessage());
    }
}
