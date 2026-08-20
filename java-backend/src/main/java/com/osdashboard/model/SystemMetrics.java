package com.osdashboard.model;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

import java.util.List;

/**
 * Mapeia 1:1 o objeto JSON emitido pelo núcleo em C a cada tick
 * (veja {@code metrics.c} -> metrics_to_json). Não carrega timestamp de
 * recebimento aqui de propósito: isso é responsabilidade de
 * {@link com.osdashboard.model.MetricsSnapshot}, que envolve este record
 * com o horário em que o backend Java o recebeu.
 */
@JsonIgnoreProperties(ignoreUnknown = true)
public record SystemMetrics(
        String type,
        String algo,
        long tick,
        double cpuUtilization,
        long contextSwitches,
        long pageFaults,
        int freeFrames,
        int totalFrames,
        int runningPid,
        int readyQueueSize,
        List<ProcessMetric> processes
) {
    public double memoryUsedPercent() {
        if (totalFrames <= 0) return 0.0;
        return ((totalFrames - freeFrames) / (double) totalFrames) * 100.0;
    }
}
