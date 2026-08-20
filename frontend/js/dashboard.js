/* =========================================================
   OS::DASHBOARD frontend
   Conecta via STOMP/SockJS em /ws, escuta /topic/metrics e
   /topic/status, e envia comandos via REST (POST/DELETE
   /api/processes) que o backend repassa ao núcleo em C.
   ========================================================= */

const API_BASE = window.location.origin.includes('file://') ? 'http://localhost:8080' : '';
const WS_URL = (API_BASE || window.location.origin) + '/ws';

const state = {
  cpuHistory: [],       // últimos N valores de cpuUtilization para o gráfico
  maxHistory: 150,
  currentAlgo: 'RR',
};

const el = {
  connDot: document.getElementById('connDot'),
  connLabel: document.getElementById('connLabel'),
  cpuRing: document.getElementById('cpuRing'),
  memRing: document.getElementById('memRing'),
  cpuValue: document.getElementById('cpuValue'),
  memValue: document.getElementById('memValue'),
  statTick: document.getElementById('statTick'),
  statAlgo: document.getElementById('statAlgo'),
  statCtx: document.getElementById('statCtx'),
  statFaults: document.getElementById('statFaults'),
  statReadyQ: document.getElementById('statReadyQ'),
  statRunning: document.getElementById('statRunning'),
  tableBody: document.getElementById('processTableBody'),
  chartCanvas: document.getElementById('cpuChart'),
  spawnForm: document.getElementById('spawnForm'),
};

const RING_CIRCUMFERENCE = 2 * Math.PI * 52; // r=52, ver dashboard.css

function setConnectionStatus(status) {
  // status: 'connecting' | 'connected' | 'error'
  el.connDot.className = 'dot' + (status === 'connected' ? ' connected' : status === 'error' ? ' error' : '');
  el.connLabel.textContent = {
    connecting: 'CONNECTING…',
    connected: 'LIVE',
    error: 'ENGINE OFFLINE',
  }[status];
}

function setRing(ringEl, percent) {
  const offset = RING_CIRCUMFERENCE * (1 - Math.max(0, Math.min(100, percent)) / 100);
  ringEl.style.strokeDasharray = RING_CIRCUMFERENCE.toFixed(1);
  ringEl.style.strokeDashoffset = offset.toFixed(1);
}

function updateGauges(metrics) {
  const cpu = metrics.cpuUtilization;
  const mem = metrics.totalFrames > 0
    ? ((metrics.totalFrames - metrics.freeFrames) / metrics.totalFrames) * 100
    : 0;

  el.cpuValue.textContent = cpu.toFixed(0) + '%';
  el.memValue.textContent = mem.toFixed(0) + '%';
  setRing(el.cpuRing, cpu);
  setRing(el.memRing, mem);
}

function updateStats(metrics) {
  el.statTick.textContent = metrics.tick;
  el.statAlgo.textContent = metrics.algo;
  el.statCtx.textContent = metrics.contextSwitches;
  el.statFaults.textContent = metrics.pageFaults;
  el.statReadyQ.textContent = metrics.readyQueueSize;
  el.statRunning.textContent = metrics.runningPid === -1 ? '— idle —' : ('PID ' + metrics.runningPid);

  // sincroniza destaque do botão de algoritmo caso tenha mudado por outro cliente
  const shortAlgo = { ROUND_ROBIN: 'RR', PRIORITY: 'PRIORITY', SJF: 'SJF' }[metrics.algo] || 'RR';
  if (shortAlgo !== state.currentAlgo) {
    state.currentAlgo = shortAlgo;
    document.querySelectorAll('.algo-btn').forEach(b => {
      b.classList.toggle('active', b.dataset.algo === shortAlgo);
    });
  }
}

function updateTable(processes) {
  if (!processes || processes.length === 0) {
    el.tableBody.innerHTML = '<tr class="empty-row"><td colspan="9">nenhum processo ativo — a fila está vazia</td></tr>';
    return;
  }

  el.tableBody.innerHTML = processes.map(p => {
    const progress = p.burstTotalMs > 0
      ? Math.min(100, Math.max(0, ((p.burstTotalMs - p.burstRemainingMs) / p.burstTotalMs) * 100))
      : 0;
    return `
      <tr>
        <td>${p.pid}</td>
        <td>${escapeHtml(p.name)}</td>
        <td><span class="badge badge-${p.state}">${p.state}</span></td>
        <td>${p.priority}</td>
        <td>
          <div class="progress-bar"><div class="progress-fill" style="width:${progress.toFixed(0)}%"></div></div>
        </td>
        <td>${p.framesOwned}</td>
        <td>${p.pageFaults}</td>
        <td>${p.waitTimeMs}</td>
        <td><button class="kill-btn" data-pid="${p.pid}">KILL</button></td>
      </tr>`;
  }).join('');

  el.tableBody.querySelectorAll('.kill-btn').forEach(btn => {
    btn.addEventListener('click', () => killProcess(btn.dataset.pid));
  });
}

function escapeHtml(str) {
  const d = document.createElement('div');
  d.textContent = str;
  return d.innerHTML;
}

/* ---------------- Canvas chart (sem dependências externas) ---------------- */

function pushHistory(cpuValue) {
  state.cpuHistory.push(cpuValue);
  if (state.cpuHistory.length > state.maxHistory) state.cpuHistory.shift();
}

function drawChart() {
  const canvas = el.chartCanvas;
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  if (canvas.width !== rect.width * dpr || canvas.height !== rect.height * dpr) {
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
  }
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const w = rect.width, h = rect.height;
  ctx.clearRect(0, 0, w, h);

  // grid horizontal
  ctx.strokeStyle = 'rgba(0, 240, 255, 0.08)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = (h / 4) * i;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  if (state.cpuHistory.length < 2) return;

  const stepX = w / (state.maxHistory - 1);
  const startIdx = state.maxHistory - state.cpuHistory.length;

  // área preenchida com gradiente
  const grad = ctx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0, 'rgba(0, 240, 255, 0.35)');
  grad.addColorStop(1, 'rgba(0, 240, 255, 0.0)');

  ctx.beginPath();
  state.cpuHistory.forEach((v, i) => {
    const x = (startIdx + i) * stepX;
    const y = h - (v / 100) * h;
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.lineTo((startIdx + state.cpuHistory.length - 1) * stepX, h);
  ctx.lineTo(startIdx * stepX, h);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // linha
  ctx.beginPath();
  state.cpuHistory.forEach((v, i) => {
    const x = (startIdx + i) * stepX;
    const y = h - (v / 100) * h;
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#00f0ff';
  ctx.lineWidth = 2;
  ctx.shadowColor = '#00f0ff';
  ctx.shadowBlur = 8;
  ctx.stroke();
  ctx.shadowBlur = 0;
}

/* ---------------- STOMP / WebSocket ---------------- */

let stompClient = null;

function connect() {
  setConnectionStatus('connecting');
  const socket = new SockJS(WS_URL);
  stompClient = Stomp.over(socket);
  stompClient.debug = null; // silencia logs verbosos do stomp.js no console

  stompClient.connect({}, () => {
    setConnectionStatus('connected');

    stompClient.subscribe('/topic/metrics', (message) => {
      const snapshot = JSON.parse(message.body);
      const metrics = snapshot.metrics;
      if (!metrics) return;
      pushHistory(metrics.cpuUtilization);
      updateGauges(metrics);
      updateStats(metrics);
      updateTable(metrics.processes);
      drawChart();
    });

    stompClient.subscribe('/topic/status', (message) => {
      const status = JSON.parse(message.body);
      if (status.connected === false) setConnectionStatus('error');
    });

    loadInitialState();
  }, () => {
    setConnectionStatus('error');
    setTimeout(connect, 3000); // reconecta
  });
}

async function loadInitialState() {
  try {
    const res = await fetch(`${API_BASE}/api/system`);
    if (res.status === 204) return;
    const snapshot = await res.json();
    if (snapshot?.metrics) {
      pushHistory(snapshot.metrics.cpuUtilization);
      updateGauges(snapshot.metrics);
      updateStats(snapshot.metrics);
      updateTable(snapshot.metrics.processes);
      drawChart();
    }
  } catch (e) {
    console.warn('Falha ao carregar estado inicial:', e);
  }
}

/* ---------------- Ações do usuário ---------------- */

async function killProcess(pid) {
  try {
    await fetch(`${API_BASE}/api/processes/${pid}`, { method: 'DELETE' });
  } catch (e) {
    console.error('Falha ao encerrar processo', e);
  }
}

async function spawnProcess(payload) {
  try {
    const res = await fetch(`${API_BASE}/api/processes`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      console.warn('Spawn rejeitado:', err);
    }
  } catch (e) {
    console.error('Falha ao criar processo', e);
  }
}

async function setAlgorithm(algo) {
  document.querySelectorAll('.algo-btn').forEach(b => b.classList.toggle('active', b.dataset.algo === algo));
  state.currentAlgo = algo;
  try {
    await fetch(`${API_BASE}/api/processes/algorithm`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ algo }),
    });
  } catch (e) {
    console.error('Falha ao trocar algoritmo', e);
  }
}

document.querySelectorAll('.algo-btn').forEach(btn => {
  btn.addEventListener('click', () => setAlgorithm(btn.dataset.algo));
});

el.spawnForm.addEventListener('submit', (e) => {
  e.preventDefault();
  const payload = {
    name: document.getElementById('spawnName').value.trim(),
    priority: parseInt(document.getElementById('spawnPriority').value, 10),
    burstMs: parseInt(document.getElementById('spawnBurst').value, 10),
    pages: parseInt(document.getElementById('spawnPages').value, 10),
  };
  spawnProcess(payload);
  e.target.reset();
  document.getElementById('spawnPriority').value = 2;
  document.getElementById('spawnBurst').value = 2000;
  document.getElementById('spawnPages').value = 3;
});

window.addEventListener('resize', drawChart);

connect();
