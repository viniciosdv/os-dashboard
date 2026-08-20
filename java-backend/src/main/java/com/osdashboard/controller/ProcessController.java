package com.osdashboard.controller;

import com.osdashboard.dto.AlgorithmRequest;
import com.osdashboard.dto.SpawnProcessRequest;
import com.osdashboard.engine.EngineClient;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

/**
 * Traduz ações do usuário no dashboard (criar processo, matar processo,
 * trocar algoritmo de escalonamento) em comandos de texto enviados ao
 * núcleo em C via {@link EngineClient}. O núcleo é a única fonte de
 * verdade sobre o estado da simulação; este controller não guarda estado.
 */
@RestController
@RequestMapping("/api/processes")
@CrossOrigin(origins = "*")
public class ProcessController {

    private final EngineClient engineClient;

    public ProcessController(EngineClient engineClient) {
        this.engineClient = engineClient;
    }

    @PostMapping
    public ResponseEntity<?> spawn(@Valid @RequestBody SpawnProcessRequest req) {
        String command = String.format("SPAWN %s %d %d %d",
                sanitize(req.name()), req.priority(), req.burstMs(), req.pages());
        engineClient.sendCommand(command);
        return ResponseEntity.status(HttpStatus.ACCEPTED)
                .body(Map.of("status", "queued", "command", command));
    }

    @DeleteMapping("/{pid}")
    public ResponseEntity<?> kill(@PathVariable int pid) {
        engineClient.sendCommand("KILL " + pid);
        return ResponseEntity.accepted().body(Map.of("status", "queued", "pid", pid));
    }

    @PostMapping("/algorithm")
    public ResponseEntity<?> setAlgorithm(@Valid @RequestBody AlgorithmRequest req) {
        engineClient.sendCommand("ALGO " + req.algo());
        return ResponseEntity.accepted().body(Map.of("status", "queued", "algo", req.algo()));
    }

    /** Comandos são texto puro delimitado por linha - removemos espaços do nome para não quebrar o parser do engine. */
    private String sanitize(String name) {
        return name.trim().replaceAll("\\s+", "_");
    }
}
