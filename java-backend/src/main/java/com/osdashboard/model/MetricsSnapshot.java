package com.osdashboard.model;

import java.time.Instant;

/** Envelope enviado ao frontend: métricas cruas do engine + timestamp do backend. */
public record MetricsSnapshot(SystemMetrics metrics, Instant receivedAt) {
}
