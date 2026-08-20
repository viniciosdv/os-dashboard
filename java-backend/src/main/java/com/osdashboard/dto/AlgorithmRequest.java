package com.osdashboard.dto;

import jakarta.validation.constraints.Pattern;

public record AlgorithmRequest(
        @Pattern(regexp = "RR|PRIORITY|SJF", message = "algo deve ser RR, PRIORITY ou SJF") String algo
) {
}
