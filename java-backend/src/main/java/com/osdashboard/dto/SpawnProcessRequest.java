package com.osdashboard.dto;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;

public record SpawnProcessRequest(
        @NotBlank @jakarta.validation.constraints.Size(max = 31) String name,
        @NotNull @Min(0) @Max(4) Integer priority,
        @NotNull @Min(100) @Max(20000) Integer burstMs,
        @NotNull @Min(1) @Max(20) Integer pages
) {
}
