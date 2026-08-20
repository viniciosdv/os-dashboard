package com.osdashboard.engine;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.osdashboard.model.SystemMetrics;
import com.osdashboard.service.MetricsService;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import java.io.*;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Ponte entre o backend Java e o núcleo de simulação em C.
 *
 * Mantém uma conexão TCP persistente com {@code os_engine} (ver
 * c-engine/src/socket_server.c). O engine publica um objeto JSON por linha
 * a cada tick (~100ms); esta classe lê essa stream continuamente em uma
 * thread dedicada, desserializa para {@link SystemMetrics} e repassa ao
 * {@link MetricsService}, que por sua vez transmite via WebSocket/STOMP.
 *
 * A mesma conexão é reutilizada para enviar comandos de controle
 * (SPAWN / KILL / ALGO) que chegam via API REST.
 *
 * Reconecta automaticamente com backoff fixo caso o engine em C ainda não
 * tenha subido ou caia — isso permite iniciar os dois processos em
 * qualquer ordem (útil em docker-compose).
 */
@Component
public class EngineClient {

    private static final Logger log = LoggerFactory.getLogger(EngineClient.class);

    @Value("${engine.host:localhost}")
    private String host;

    @Value("${engine.port:5051}")
    private int port;

    @Value("${engine.reconnect-delay-ms:2000}")
    private long reconnectDelayMs;

    private final MetricsService metricsService;
    private final ObjectMapper objectMapper = new ObjectMapper();

    private final AtomicBoolean running = new AtomicBoolean(true);
    private volatile Socket socket;
    private volatile PrintWriter out;
    private volatile boolean connected = false;

    private Thread readerThread;

    public EngineClient(MetricsService metricsService) {
        this.metricsService = metricsService;
    }

    @PostConstruct
    public void start() {
        readerThread = new Thread(this::runLoop, "engine-client-reader");
        readerThread.setDaemon(true);
        readerThread.start();
    }

    @PreDestroy
    public void stop() {
        running.set(false);
        closeQuietly();
        if (readerThread != null) {
            readerThread.interrupt();
        }
    }

    public boolean isConnected() {
        return connected;
    }

    /** Envia um comando de linha única (SPAWN/KILL/ALGO) para o engine em C. */
    public synchronized void sendCommand(String command) {
        if (!connected || out == null) {
            throw new IllegalStateException("Engine C não está conectado no momento");
        }
        out.println(command);
        if (out.checkError()) {
            connected = false;
            throw new IllegalStateException("Falha ao enviar comando ao engine (conexão perdida)");
        }
    }

    private void runLoop() {
        while (running.get()) {
            try {
                connect();
                readLoop();
            } catch (IOException e) {
                log.warn("[engine-client] conexão perdida com {}:{} ({}). Tentando reconectar em {}ms...",
                        host, port, e.getMessage(), reconnectDelayMs);
            } finally {
                closeQuietly();
                connected = false;
                metricsService.markDisconnected();
            }

            if (running.get()) {
                sleepQuietly(reconnectDelayMs);
            }
        }
    }

    private void connect() throws IOException {
        socket = new Socket(host, port);
        out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream()), true);
        connected = true;
        log.info("[engine-client] conectado ao núcleo C em {}:{}", host, port);
    }

    private void readLoop() throws IOException {
        try (BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()))) {
            String line;
            while (running.get() && (line = in.readLine()) != null) {
                if (line.isBlank()) continue;
                try {
                    SystemMetrics metrics = objectMapper.readValue(line, SystemMetrics.class);
                    metricsService.publish(metrics);
                } catch (Exception parseEx) {
                    log.debug("[engine-client] linha ignorada (JSON inválido): {}", parseEx.getMessage());
                }
            }
        }
    }

    private void closeQuietly() {
        try {
            if (socket != null) socket.close();
        } catch (IOException ignored) {
        }
    }

    private void sleepQuietly(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
