package com.osdashboard.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

/**
 * Espelha exatamente o objeto de processo emitido por {@code metrics.c}
 * no núcleo em C. Usamos um record para imutabilidade e desserialização
 * direta via Jackson (suporte nativo a records desde Jackson 2.12).
 */
@JsonIgnoreProperties(ignoreUnknown = true)
public record ProcessMetric(
        int pid,
        String name,
        String state,
        int priority,
        int burstTotalMs,
        int burstRemainingMs,
        int framesOwned,
        int pageFaults,
        int contextSwitches,
        long waitTimeMs
) {
    /** Percentual de conclusão do burst de CPU, usado nas barras de progresso do dashboard. */
    public double progressPercent() {
        if (burstTotalMs <= 0) return 0.0;
        double done = burstTotalMs - burstRemainingMs;
        return Math.max(0.0, Math.min(100.0, (done / burstTotalMs) * 100.0));
    }
}
