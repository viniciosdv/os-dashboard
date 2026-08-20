# OS::DASHBOARD — Simulador de Processador e Monitor de Sistema

Um simulador de sistema operacional em três camadas: um **núcleo em C** simula
escalonamento de CPU e paginação de memória em tempo real, um **backend em
Java (Spring Boot)** faz a ponte entre o núcleo e o mundo externo via REST e
WebSocket, e um **frontend em HTML/CSS/JS** exibe tudo em um painel estilo
cyberpunk, atualizado ao vivo.

```
┌──────────────────┐   TCP socket    ┌────────────────────┐   WebSocket/STOMP   ┌──────────────────┐
│   C ENGINE        │  JSON por linha │   JAVA BACKEND      │   JSON snapshots    │   FRONTEND        │
│  scheduler.c       │ ─────────────▶ │   Spring Boot        │ ──────────────────▶ │  dashboard.js      │
│  memory.c          │ ◀───────────── │   EngineClient        │ ◀────────────────── │  (comandos REST)   │
│  socket_server.c   │   comandos      │   MetricsService      │                     │                    │
└──────────────────┘  SPAWN/KILL/ALGO └────────────────────┘                      └──────────────────┘
```

## Como as linguagens se conectam

- **C** roda o loop de simulação de verdade: tabela de processos, fila de
  prontos, três algoritmos de escalonamento (Round Robin, Priority, SJF) e um
  gerenciador de memória paginada com alocação/eviction de frames e page
  faults simulados. A cada tick (100ms) ele serializa o estado inteiro para
  JSON (sem bibliotecas externas — parser escrito à mão em `metrics.c`) e
  transmite via socket TCP para todo cliente conectado. O mesmo socket recebe
  comandos de controle em texto simples (`SPAWN`, `KILL`, `ALGO`).
- **Java** mantém uma conexão TCP persistente com o núcleo em C
  (`EngineClient`, com reconexão automática), desserializa o JSON com Jackson,
  guarda um histórico curto em memória e republica cada snapshot no tópico
  STOMP `/topic/metrics`. Também expõe endpoints REST que traduzem ações do
  usuário em comandos de texto enviados de volta ao engine.
- **CSS/JS** conecta via SockJS + STOMP, atualiza gauges em SVG, uma tabela de
  processos e um gráfico de CPU desenhado em `<canvas>` sem dependências de
  charting — tudo com estética neon/glass inspirada em HUDs de jogos.

## Protocolo entre C e Java

Uma linha JSON por tick, por exemplo:

```json
{"type":"metrics","algo":"ROUND_ROBIN","tick":812,"cpuUtilization":73.40,
 "contextSwitches":211,"pageFaults":34,"freeFrames":238,"totalFrames":256,
 "runningPid":4,"readyQueueSize":3,
 "processes":[{"pid":4,"name":"render_thread","state":"RUNNING","priority":1,
   "burstTotalMs":905,"burstRemainingMs":320,"framesOwned":2,
   "pageFaults":1,"contextSwitches":2,"waitTimeMs":900}]}
```

Comandos aceitos pelo engine (uma linha de texto por comando):

| Comando                              | Efeito                                  |
|---------------------------------------|------------------------------------------|
| `SPAWN <nome> <prio 0-4> <ms> <pgs>`  | Cria um novo processo simulado           |
| `KILL <pid>`                          | Encerra e libera os frames do processo   |
| `ALGO RR\|PRIORITY\|SJF`              | Troca o algoritmo de escalonamento ativo |

## Rodando localmente

### 1. Núcleo em C
```bash
cd c-engine
make
./bin/os_engine 5051 8      # porta, nº de processos iniciais
```

### 2. Backend Java
```bash
cd java-backend
mvn spring-boot:run
# ou: mvn package && java -jar target/os-dashboard-backend-1.0.0.jar
```
Variáveis de ambiente: `ENGINE_HOST` (default `localhost`), `ENGINE_PORT`
(default `5051`).

### 3. Frontend
Basta servir a pasta `frontend/` estaticamente (ex: `npx serve frontend`) ou
abrir `index.html` diretamente — ele aponta para `http://localhost:8080`.

### Tudo junto com Docker Compose
```bash
docker compose up --build
```
- Engine C: `localhost:5051` (TCP interno)
- API/WebSocket Java: `localhost:8080`
- Dashboard: `localhost:8081`

## Estrutura do repositório

```
os-dashboard/
├── c-engine/           # núcleo de simulação (scheduler + memória + servidor TCP)
│   ├── include/engine.h
│   ├── src/{engine_core,memory,metrics,socket_server,main}.c
│   └── Makefile
├── java-backend/        # Spring Boot: WebSocket, REST, ponte TCP
│   └── src/main/java/com/osdashboard/{config,engine,model,dto,service,controller}
├── frontend/             # dashboard cyberpunk (vanilla JS + canvas)
│   ├── index.html
│   ├── css/dashboard.css
│   └── js/dashboard.js
└── docker-compose.yml
```

## Possíveis extensões

- Persistir histórico de métricas em um banco de série temporal (InfluxDB/Timescale)
- Adicionar mais algoritmos (Multilevel Feedback Queue) no núcleo em C
- Autenticação nos endpoints REST + rooms por usuário no WebSocket
- Testes: `ctest`/Unity no C, `spring-boot-starter-test` no Java
