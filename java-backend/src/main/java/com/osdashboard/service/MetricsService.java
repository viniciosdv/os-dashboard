package com.osdashboard.service;

import com.osdashboard.model.MetricsSnapshot;
import com.osdashboard.model.SystemMetrics;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.ArrayDeque;
import java.util.Collections;
import java.util.Deque;
import java.util.List;
import java.util.Map;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Fonte única de verdade do estado "ao vivo" do dashboard no lado do
 * backend. Recebe cada {@link SystemMetrics} vindo do {@link com.osdashboard.engine.EngineClient},
 * envelopa com timestamp, mantém um histórico limitado em memória (para
 * gráficos de série temporal no frontend) e publica no tópico STOMP
 * /topic/metrics para todos os clientes WebSocket conectados.
 */
@Service
public class MetricsService {

    private static final Logger log = LoggerFactory.getLogger(MetricsService.class);
    private static final int HISTORY_CAPACITY = 300; // ~30s de histórico a 100ms/tick

    private final SimpMessagingTemplate messagingTemplate;
    private final ReentrantLock lock = new ReentrantLock();
    private final Deque<MetricsSnapshot> history = new ArrayDeque<>(HISTORY_CAPACITY);

    private volatile MetricsSnapshot latest;

    public MetricsService(SimpMessagingTemplate messagingTemplate) {
        this.messagingTemplate = messagingTemplate;
    }

    public void publish(SystemMetrics metrics) {
        MetricsSnapshot snapshot = new MetricsSnapshot(metrics, Instant.now());

        lock.lock();
        try {
            latest = snapshot;
            if (history.size() >= HISTORY_CAPACITY) {
                history.removeFirst();
            }
            history.addLast(snapshot);
        } finally {
            lock.unlock();
        }

        messagingTemplate.convertAndSend("/topic/metrics", snapshot);
    }

    public void markDisconnected() {
        log.warn("[metrics-service] engine C desconectado; frontend será notificado");
        messagingTemplate.convertAndSend("/topic/status", Map.of(
                "connected", false,
                "at", Instant.now().toString()
        ));
    }

    public MetricsSnapshot getLatest() {
        return latest;
    }

    public List<MetricsSnapshot> getHistory() {
        lock.lock();
        try {
            return List.copyOf(history);
        } finally {
            lock.unlock();
        }
    }
}
