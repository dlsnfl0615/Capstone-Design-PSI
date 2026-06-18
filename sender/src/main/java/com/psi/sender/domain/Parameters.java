package com.psi.sender.domain;

import lombok.Getter;
import lombok.Setter;

@Setter
@Getter
public class Parameters {
    private int alpha;
    private int windowing;

    public Parameters(int alpha, int windowing) {
        this.alpha = alpha;
        this.windowing = windowing;
    }
}
