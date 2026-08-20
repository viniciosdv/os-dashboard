<div align="center">


# OS::DASHBOARD
### *Simulador de Sistema Operacional Distribuído em Camadas*

<img src="https://capsule-render.vercel.app/api?type=waving&color=gradient&customColorList=6,11,20,30&height=120&section=header&text=C%20•%20Java%20•%20WebSockets&fontSize=22&fontColor=00ffcc" width="100%" />

[![GitHub license](https://img.shields.io/github/license/viniciosdv/os-dashboard?style=flat-square&color=00ffcc&labelColor=0d0f18)](https://github.com/viniciosdv/os-dashboard/blob/main/LICENSE)
[![Build Status](https://img.shields.io/github/actions/workflow/status/viniciosdv/os-dashboard/docker-build.yml?style=flat-square&color=00ffcc&labelColor=0d0f18)](https://github.com/viniciosdv/os-dashboard/actions)
[![Language C](https://img.shields.io/badge/Core-C-A8B9CC?style=flat-square&labelColor=0d0f18)]()
[![Backend Java](https://img.shields.io/badge/Backend-Java%20Spring-ED8B00?style=flat-square&labelColor=0d0f18)]()
[![Frontend](https://img.shields.io/badge/Frontend-HTML%2FJS-E34F26?style=flat-square&labelColor=0d0f18)]()

</div>

---

### ⚡ Simulador de Processador & Monitor de Sistema em Tempo Real ⚡

*Um kernel simulado em **C**, uma ponte em **Java (Spring Boot)** e um HUD **cyberpunk** no navegador — tudo conversando ao vivo via socket TCP e WebSocket.*

<p>
  <img alt="C" src="https://img.shields.io/badge/core-C11-00f0ff?style=for-the-badge&logo=c&logoColor=white&labelColor=05070d">
  <img alt="Java" src="https://img.shields.io/badge/backend-Java%2017%20%2F%20Spring%20Boot-ff2ea6?style=for-the-badge&logo=spring&logoColor=white&labelColor=05070d">
  <img alt="JS" src="https://img.shields.io/badge/frontend-Vanilla%20JS%20%2B%20WebSocket-ffb020?style=for-the-badge&logo=javascript&logoColor=white&labelColor=05070d">
</p>
<p>
  <img alt="License" src="https://img.shields.io/badge/license-MIT-39ff88?style=flat-square&labelColor=05070d">
  <img alt="Status" src="https://img.shields.io/badge/status-active-39ff88?style=flat-square&labelColor=05070d">
  <img alt="Docker" src="https://img.shields.io/badge/docker-ready-00f0ff?style=flat-square&logo=docker&logoColor=white&labelColor=05070d">
  <img alt="PRs" src="https://img.shields.io/badge/PRs-welcome-ff2ea6?style=flat-square&labelColor=05070d">
</p>

<sub>Dica: grave um GIF do dashboard rodando e cole aqui — `![demo](docs/demo.gif)` — é o que mais vende o repositório à primeira vista.</sub>

</div>

<br>

## 📡 Sumário

- [Visão geral](#-visão-geral)
- [Arquitetura](#-arquitetura)
- [Como as linguagens se conectam](#-como-as-linguagens-se-conectam)
- [Protocolo C ↔ Java](#-protocolo-c--java)
- [Stack](#-stack)
- [Quick start](#-quick-start)
- [Rodando com Docker](#-rodando-com-docker-um-comando)
- [Rodando manualmente](#-rodando-manualmente)
- [Estrutura do repositório](#-estrutura-do-repositório)
- [Roadmap](#-roadmap)
- [Licença](#-licença)

<br>

## 🧠 Visão geral

**OS::DASHBOARD** simula, de verdade, o núcleo de um sistema operacional:

- 🔄 **Escalonamento de CPU** — Round Robin (preemptivo), Priority e SJF, trocáveis em tempo real
- 🧩 **Memória paginada** — alocação de frames, eviction e page faults simulados
- 🧵 **Concorrência real** — o núcleo em C usa `pthreads` de verdade, não é só um mock
- 📊 **Telemetria ao vivo** — cada tick (100ms) vira um snapshot JSON transmitido por socket
- 🎮 **HUD cyberpunk** — gauges em SVG, gráfico em `<canvas>`, glow, scanlines, sem libs de chart

<br>

## 🏗️ Arquitetura

```
┌─────────────────────┐   TCP socket     ┌───────────────────────┐   WebSocket/STOMP   ┌──────────────────────┐
│      C ENGINE         │  JSON por linha  │      JAVA BACKEND        │   JSON snapshots     │       FRONTEND         │
│ ────────────────────  │ ───────────────▶ │ ──────────────────────  │ ───────────────────▶ │ ─────────────────────  │
│  engine_core.c         │                  │  EngineClient (TCP)      │                       │  dashboard.js           │
│  memory.c               │ ◀─────────────── │  MetricsService           │ ◀──────────────────── │  (comandos via REST)     │
│  socket_server.c         │  SPAWN/KILL/ALGO │  Controllers REST          │                       │  gauges + canvas + tabela │
└─────────────────────┘                  └───────────────────────┘                       └──────────────────────┘
     porta 5051                               porta 8080                                     porta 8081 (ou local)
```

<br>

## 🔌 Como as linguagens se conectam

<details open>
<summary><b>🔧 C — o motor de simulação</b></summary>
<br>

Roda o loop de verdade: tabela de processos, fila de prontos, três algoritmos
de escalonamento e um gerenciador de memória paginada com alocação/eviction
de frames e page faults simulados. A cada tick (100ms) serializa o estado
inteiro para JSON — **parser escrito à mão em `metrics.c`, zero dependências
externas** — e transmite via socket TCP para todo cliente conectado. O mesmo
socket recebe comandos de controle em texto simples (`SPAWN`, `KILL`, `ALGO`).

</details>

<details>
<summary><b>☕ Java — a ponte</b></summary>
<br>

`EngineClient` mantém uma conexão TCP persistente com o núcleo em C, com
**reconexão automática** caso o engine caia ou ainda não tenha subido.
Desserializa o JSON com Jackson, guarda um histórico curto em memória e
republica cada snapshot no tópico STOMP `/topic/metrics`. Também expõe
endpoints REST que traduzem ações do usuário em comandos de texto enviados
de volta ao engine.

</details>

<details>
<summary><b>🎨 CSS/JS — o HUD</b></summary>
<br>

Conecta via SockJS + STOMP, atualiza gauges em SVG, uma tabela de processos
e um gráfico de CPU desenhado em `<canvas>` — sem dependências de charting —
com estética neon/glass inspirada em HUDs de jogos.

</details>

<br>

## 📦 Protocolo C ↔ Java

Uma linha JSON por tick:

```jsonc
{
  "type": "metrics", "algo": "ROUND_ROBIN", "tick": 812,
  "cpuUtilization": 73.40, "contextSwitches": 211, "pageFaults": 34,
  "freeFrames": 238, "totalFrames": 256,
  "runningPid": 4, "readyQueueSize": 3,
  "processes": [
    { "pid": 4, "name": "render_thread", "state": "RUNNING", "priority": 1,
      "burstTotalMs": 905, "burstRemainingMs": 320, "framesOwned": 2,
      "pageFaults": 1, "contextSwitches": 2, "waitTimeMs": 900 }
  ]
}
```

Comandos aceitos pelo engine (uma linha de texto por comando):

| Comando | Efeito |
|---|---|
| `SPAWN <nome> <prio 0-4> <ms> <pgs>` | Cria um novo processo simulado |
| `KILL <pid>` | Encerra e libera os frames do processo |
| `ALGO RR\|PRIORITY\|SJF` | Troca o algoritmo de escalonamento ativo |

<br>

## 🛠️ Stack

| Camada | Tecnologias |
|---|---|
| **Core** | C11 · POSIX threads · sockets TCP · Makefile |
| **Backend** | Java 17 · Spring Boot 3 · WebSocket/STOMP · Jackson · Bean Validation |
| **Frontend** | HTML5 · CSS3 (custom properties, glassmorphism) · JS vanilla · SockJS/STOMP.js · Canvas API |
| **Infra** | Docker · Docker Compose · Nginx |

<br>

## 🚀 Quick start

```bash
git clone <seu-repo>
cd os-dashboard
docker compose up --build
```

Abra **http://localhost:8081** e pronto — os três serviços já estão
conversando entre si.

<br>

## 🐳 Rodando com Docker (um comando)

```bash
docker compose up --build
```

| Serviço | Porta | Descrição |
|---|---|---|
| `c-engine` | `5051` | núcleo de simulação (TCP interno) |
| `backend` | `8080` | API REST + WebSocket |
| `frontend` | `8081` | dashboard cyberpunk |

<br>

## 🔨 Rodando manualmente

<details>
<summary><b>1️⃣ Núcleo em C</b></summary>

```bash
cd c-engine
make
./bin/os_engine 5051 8      # porta, nº de processos iniciais
```
</details>

<details>
<summary><b>2️⃣ Backend Java</b></summary>

```bash
cd java-backend
mvn spring-boot:run
# ou: mvn package && java -jar target/os-dashboard-backend-1.0.0.jar
```
Variáveis de ambiente: `ENGINE_HOST` (default `localhost`), `ENGINE_PORT` (default `5051`).
</details>

<details>
<summary><b>3️⃣ Frontend</b></summary>

```bash
cd frontend
npx serve .
```
Ou abra `index.html` direto no navegador — ele já aponta para `http://localhost:8080`.
</details>

<br>

## 🗂️ Estrutura do repositório

```
os-dashboard/
├── c-engine/              # núcleo de simulação (scheduler + memória + servidor TCP)
│   ├── include/engine.h
│   ├── src/{engine_core,memory,metrics,socket_server,main}.c
│   └── Makefile
├── java-backend/          # Spring Boot: WebSocket, REST, ponte TCP
│   └── src/main/java/com/osdashboard/{config,engine,model,dto,service,controller}
├── frontend/               # dashboard cyberpunk (vanilla JS + canvas)
│   ├── index.html
│   ├── css/dashboard.css
│   └── js/dashboard.js
└── docker-compose.yml
```

<br>

## 🗺️ Roadmap

- [ ] Persistir histórico de métricas em banco de série temporal (InfluxDB/Timescale)
- [ ] Multilevel Feedback Queue no núcleo em C
- [ ] Autenticação nos endpoints REST + rooms por usuário no WebSocket
- [ ] Testes automatizados (Unity no C, JUnit no Java) + CI no GitHub Actions
- [ ] GIF de demonstração no topo deste README

<br>

## 📄 Licença

Distribuído sob a licença MIT. Veja `LICENSE` para
