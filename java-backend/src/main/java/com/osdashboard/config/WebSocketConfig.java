package com.osdashboard.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.messaging.simp.config.MessageBrokerRegistry;
import org.springframework.web.socket.config.annotation.EnableWebSocketMessageBroker;
import org.springframework.web.socket.config.annotation.StompEndpointRegistry;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

/**
 * Registra o endpoint STOMP em /ws (com fallback SockJS) e habilita um
 * message broker simples em memória para o tópico /topic/metrics, para onde
 * o {@link com.osdashboard.service.MetricsService} publica cada snapshot
 * recebido do núcleo em C.
 */
@Configuration
@EnableWebSocketMessageBroker
public class WebSocketConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureMessageBroker(MessageBrokerRegistry registry) {
        registry.enableSimpleBroker("/topic");
        registry.setApplicationDestinationPrefixes("/app");
    }

    @Override
    public void registerStompEndpoints(StompEndpointRegistry registry) {
        registry.addEndpoint("/ws")
                .setAllowedOriginPatterns("*")
                .withSockJS();

        // Endpoint alternativo sem SockJS, para clientes WebSocket "cru"
        registry.addEndpoint("/ws-raw")
                .setAllowedOriginPatterns("*");
    }
}
