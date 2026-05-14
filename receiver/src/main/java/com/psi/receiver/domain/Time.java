package com.psi.receiver.domain;

import lombok.Setter;

@Setter
public class Time {
    private double transferTime; // 윈도잉, 해시테이블, 키, 결과 보내는데 걸리는 시간
    private double initTime; // 전처리 및 윈도잉 연산하는데 걸리는 시간
    private double checkTime; // sender로부터 다항식 받아와서 교집합 검증하는데 걸리는 시간
}
