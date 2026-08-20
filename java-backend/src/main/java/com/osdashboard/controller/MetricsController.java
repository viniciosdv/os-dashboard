package com.osdashboard.controller;

import com.osdashboard.model.MetricsSnapshot;
import com.osdashboard.service.MetricsService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api")
@CrossOrigin(origins = "*")
public class MetricsController {

    private final MetricsService metricsService;

    public MetricsController(MetricsService metricsService) {
        this.metricsService = metricsService;
    }

    /** Snapshot mais recente recebido do núcleo em C. Útil para o load inicial do dashboard,
     * antes do WebSocket assumir as atualizações em tempo real. */
    @GetMapping("/system")
    public ResponseEntity<MetricsSnapshot> getCurrentSystemState() {
        MetricsSnapshot latest = metricsService.getLatest();
        if (latest == null) {
            return ResponseEntity.noContent().build();
        }
        return ResponseEntity.ok(latest);
    }

    /** Lista apenas os processos ativos do snapshot mais recente. */
    @GetMapping("/processes")
    public ResponseEntity<?> getProcesses() {
        MetricsSnapshot latest = metricsService.getLatest();
        if (latest == null) {
            return ResponseEntity.noContent().build();
        }
        return ResponseEntity.ok(latest.metrics().processes());
    }

    /** Histórico em memória (últimos ~30s de snapshots), usado para popular gráficos ao conectar. */
    @GetMapping("/history")
    public ResponseEntity<List<MetricsSnapshot>> getHistory() {
        return ResponseEntity.ok(metricsService.getHistory());
    }
}
