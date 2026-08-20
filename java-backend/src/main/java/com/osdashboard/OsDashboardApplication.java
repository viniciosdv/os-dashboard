package com.osdashboard;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.scheduling.annotation.EnableScheduling;

/**
 * Ponto de entrada do backend. Responsável por:
 *  - Subir a API REST e o endpoint WebSocket/STOMP.
 *  - Iniciar o {@link com.osdashboard.engine.EngineClient}, que mantém uma
 *    conexão TCP persistente com o núcleo de simulação em C e retransmite
 *    as métricas em tempo real para o frontend.
 */
@SpringBootApplication
@EnableScheduling
@EnableAsync
public class OsDashboardApplication {
    public static void main(String[] args) {
        SpringApplication.run(OsDashboardApplication.class, args);
    }
}
