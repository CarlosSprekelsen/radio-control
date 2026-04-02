#pragma once
#include <string_view>

namespace rcc::api {

constexpr std::string_view RADIO_MONITOR_HTML = R"html(
<!DOCTYPE html>
<html>
<head>
  <title>Radio Control Console</title>
  <style>
    :root {
      --edge-accent: #ff4713;
      --edge-accent-strong: #d73a10;
      --edge-ink: #2f3944;
      --edge-ink-muted: #5a6673;
      --edge-surface: #ffffff;
      --edge-surface-soft: #f4f6f8;
      --edge-line: #cfd5dc;
      --edge-bg: #f7f9fb;
      --edge-shadow-soft: 0 10px 24px rgba(29, 35, 41, 0.08);
    }

    html, body {
      height: 100%;
      margin: 0;
    }

    body {
      font-family: "Segoe UI", Arial, sans-serif;
      background: var(--edge-bg);
      color: var(--edge-ink);
      padding: 20px;
      display: flex;
      flex-direction: column;
    }

    .brand-header {
      display: flex;
      align-items: center;
      gap: 14px;
      margin-bottom: 14px;
      flex-wrap: wrap;
    }

    .brand-logo {
      height: 28px;
      width: auto;
      object-fit: contain;
    }

    h1 {
      margin: 0;
      color: var(--edge-ink);
      font-size: 1.35rem;
    }

    h2 {
      margin: 20px 0 10px;
      color: var(--edge-ink);
      font-size: 1.05rem;
    }

    .tabs {
      display: flex;
      border-bottom: 1px solid var(--edge-line);
      margin-bottom: 15px;
      gap: 4px;
      flex-wrap: wrap;
    }

    .tab {
      padding: 10px 14px;
      cursor: pointer;
      background: var(--edge-surface-soft);
      border-radius: 6px 6px 0 0;
      transition: 0.2s;
      border: 1px solid var(--edge-line);
      border-bottom: none;
      color: var(--edge-ink);
    }

    .tab:hover { background: #edf2f6; }

    .tab.active {
      background: var(--edge-surface);
      color: var(--edge-accent);
      box-shadow: inset 0 2px 0 var(--edge-accent);
      font-weight: 600;
    }

    .tab-content {
      display: none;
      flex: 1;
      flex-direction: column;
      overflow: hidden;
    }

    .tab-content.active { display: flex; }

    .section { margin-bottom: 20px; }

    .row {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
      margin-bottom: 10px;
    }

    .grid-2 {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }

    select, button, input {
      padding: 8px 10px;
      font-family: "Segoe UI", Arial, sans-serif;
      border-radius: 6px;
      font-size: 0.92rem;
    }

    input, select {
      background: var(--edge-surface);
      color: var(--edge-ink);
      border: 1px solid var(--edge-line);
    }

    button {
      background: var(--edge-accent);
      color: white;
      border: 1px solid var(--edge-accent);
      cursor: pointer;
      transition: 0.2s;
    }

    button:hover { background: var(--edge-accent-strong); border-color: var(--edge-accent-strong); }

    button:disabled {
      background: #c8cfd7 !important;
      border-color: #c8cfd7 !important;
      color: #7e8791 !important;
      cursor: not-allowed;
    }

    button.btn-secondary {
      background: #4ec9b0;
      border-color: #3ab09c;
    }
    button.btn-secondary:hover { background: #3ab09c; border-color: #2d9080; }

    .card {
      background: var(--edge-surface);
      padding: 12px;
      border-radius: 8px;
      border: 1px solid var(--edge-line);
      box-shadow: var(--edge-shadow-soft);
      margin-bottom: 10px;
    }

    .service-identity {
      margin-bottom: 14px;
    }

    .status-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 8px 20px;
    }

    .label { color: var(--edge-ink-muted); }
    .value { color: var(--edge-ink); font-weight: 500; }

    .console {
      background: var(--edge-surface-soft);
      padding: 12px;
      flex: 1;
      overflow-y: auto;
      font-size: 12px;
      border-radius: 8px;
      border: 1px solid var(--edge-line);
      color: var(--edge-ink);
      min-height: 120px;
    }

    .ok, .connected { color: #2e7d32; }
    .err, .disconnected { color: #c62828; }
    .warn { color: #b26a00; }

    .badge {
      display: inline-block;
      padding: 2px 8px;
      border-radius: 10px;
      font-size: 0.78rem;
      font-weight: 600;
    }
    .badge-ready   { background: #e8f5e9; color: #2e7d32; }
    .badge-offline { background: #ffebee; color: #c62828; }
    .badge-busy    { background: #fff8e1; color: #b26a00; }
    .badge-recovering { background: #fff3e0; color: #e65100; }
    .badge-discovering { background: #e3f2fd; color: #1565c0; }

    @media (max-width: 900px) {
      .grid-2, .status-grid { grid-template-columns: 1fr; }
      .brand-logo { height: 24px; }
    }
  </style>
</head>
<body>

<div class="brand-header">
  <img class="brand-logo" src="data:image/svg+xml;base64,PHN2ZyB2ZXJzaW9uPSIxLjIiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDYyNSAxNzAiPgo8cGF0aCBmaWxsPSIjRTk0QTI2IiBjbGFzcz0iYSIgZD0ibTYyNC4zIDAuOHYyNS45aC04OC44di0yNS45eiIvPgo8cGF0aCBmaWxsPSIjNEI1MDU1IiBjbGFzcz0iYiIgZD0ibTQ1LjIgMTI3LjVsLTEyLjMgMTMuMnYyOS4yaC0zMi43di0xMTYuN2gzMi43djQ4LjVsNDUuMi00OC41aDM2LjNsLTQ3LjcgNTEuNyA1MC4yIDY1aC0zOC4zeiIvPgo8cGF0aCBmaWxsPSIjNEI1MDU1IiBmaWxsLXJ1bGU9ImV2ZW5vZGQiIGNsYXNzPSJiIiBkPSJtMjE3LjQgMTQ3LjNoLTQ5LjRsLTkuMSAyMi42aC0zMy43bDUxLjUtMTE2LjFoMzIuNWw1MS43IDExNi4xaC0zNC40em0tOS42LTI0LjJsLTE1LTM3LjItMTUgMzcuMnoiLz4KPHBhdGggZmlsbD0iIzRCNTA1NSIgY2xhc3M9ImIiIGQ9Im0yODYuOCA3OS40aC0zNS45di0yNi4yaDEwNC42djI2LjJoLTM1Ljd2OTAuNWgtMzN6Ii8+CjxwYXRoIGZpbGw9IiM0QjUwNTUiIGNsYXNzPSJiIiBkPSJtMzc2LjEgNTMuMmgzM3YxMTYuN2gtMzN6Ii8+CjxwYXRoIGZpbGw9IiM0QjUwNTUiIGNsYXNzPSJiIiBkPSJtNTQ2LjMgMTY5LjlsLTAuMy02MS45LTMwIDUwLjRoLTE0LjdsLTI5LjgtNDguN3Y2MC4yaC0zMC41di0xMTYuN2gyNy4ybDQxIDY3LjMgNDAtNjcuM2gyNy4xbDAuNCAxMTYuN3oiLz4KPC9zdmc+Cg==" alt="KATIM logo" />
  <h1>Radio Control Console</h1>
</div>

<div class="card service-identity">
  <div class="status-grid">
    <div><span class="label">Service:</span> <span id="serviceName" class="value">Loading...</span></div>
    <div><span class="label">Version:</span> <span id="serviceVersion" class="value">Loading...</span></div>
    <div><span class="label">Git Version:</span> <span id="serviceGitVersion" class="value">Loading...</span></div>
    <div><span class="label">Build Date:</span> <span id="serviceBuildDate" class="value">Loading...</span></div>
  </div>
</div>

<div class="tabs">
  <div class="tab active" onclick="showTab('radioTab')">Radio Control</div>
  <div class="tab" onclick="showTab('sseTab')">SSE Events</div>
</div>

<!-- ================== RADIO CONTROL TAB ================== -->
<div id="radioTab" class="tab-content active">

  <h2>Radio Selection</h2>
  <div class="card">
    <div class="row">
      <select id="radioSel" onchange="onRadioSelected()" style="flex:1;max-width:320px;"></select>
      <button class="btn-secondary" onclick="loadRadios()">Refresh</button>
      <button onclick="connectRadio()">Connect</button>
    </div>
    <div class="row">
      <label class="label">Bearer Token:</label>
      <input type="text" id="apiToken" placeholder="paste JWT for control commands..."
             style="flex:1;min-width:200px;" oninput="onTokenChange()">
    </div>
  </div>

  <h2>Current State</h2>
  <div class="card status-grid" id="stateCard">
    <div><span class="label">Status:</span> <span id="stStatus" class="value">-</span></div>
    <div><span class="label">Channel Index:</span> <span id="stChannel" class="value">-</span></div>
    <div><span class="label">Power (W):</span> <span id="stPower" class="value">-</span></div>
    <div><span class="label">Active radio:</span> <span id="stActive" class="value">-</span></div>
    <div><span class="label">Frequencies (MHz):</span> <span id="stFreqs" class="value">-</span></div>
    <div><span class="label">Power Range (W):</span> <span id="stPwrRange" class="value">-</span></div>
  </div>

  <div class="grid-2">
    <div>
      <h2>Set Channel</h2>
      <div class="card">
        <div class="row">
          <select id="chanSel" style="flex:1;max-width:280px;"></select>
          <button onclick="setChannel()">Set Channel</button>
        </div>
      </div>
    </div>
    <div>
      <h2>Set Power</h2>
      <div class="card">
        <div class="row">
          <label class="label">Power (W):</label>
          <input type="number" id="pwrWatts" value="1.0" step="0.1" min="0.0" style="width:80px;">
          <button onclick="setPower()">Set Power</button>
        </div>
      </div>
    </div>
  </div>

  <h2>API Console</h2>
  <div class="console" id="radioConsole" style="min-height:160px;"></div>
</div>

<!-- ================== SSE EVENTS TAB ================== -->
<div id="sseTab" class="tab-content">
  <h2>Event Console (SSE)</h2>
  <div class="card" style="margin-bottom:10px;">
    <div class="row">
      <input type="text" id="sseUrl" style="flex:1;min-width:240px;">
      <input type="text" id="sseToken" placeholder="Bearer token" style="flex:1;min-width:180px;">
      <button id="btnConnect" onclick="connect()">Connect</button>
      <button id="btnDisconnect" onclick="disconnect()" disabled>Disconnect</button>
      <button onclick="clearEvents()">Clear</button>
    </div>
    <div class="row">
      <span id="sseStatus" class="disconnected">● Disconnected</span>
      <span id="counter" style="color:var(--edge-ink-muted);">Events: 0</span>
    </div>
  </div>

  <div class="card" style="margin-bottom:10px;">
    <strong>Filters:</strong>
    <div style="margin-top:6px;display:flex;flex-wrap:wrap;gap:10px;">
      <label><input type="checkbox" class="event-filter" value="rcc.ready" checked> rcc.ready</label>
      <label><input type="checkbox" class="event-filter" value="rcc.radio.state" checked> rcc.radio.state</label>
      <label><input type="checkbox" class="event-filter" value="rcc.radio.channel" checked> rcc.radio.channel</label>
      <label><input type="checkbox" class="event-filter" value="rcc.radio.power" checked> rcc.radio.power</label>
      <label><input type="checkbox" class="event-filter" value="rcc.fault" checked> rcc.fault</label>
      <label><input type="checkbox" class="event-filter" value="SYSTEM" checked> SYSTEM</label>
      <label><input type="checkbox" class="event-filter" value="ERROR" checked> ERROR</label>
    </div>
  </div>

  <div class="console" id="sseConsole" style="flex:1;min-height:300px;"></div>
</div>

<script>
/* =====================================================
   Globals
===================================================== */
const API_BASE = "/api/v1";
let globalToken = "";
let radiosCache = [];

function updateServiceIdentity(identity) {
  document.getElementById("serviceName").textContent = identity?.service ?? "Unknown";
  document.getElementById("serviceVersion").textContent = identity?.version ?? "-";
  document.getElementById("serviceGitVersion").textContent = identity?.gitVersion ?? "-";
  document.getElementById("serviceBuildDate").textContent = identity?.buildDate ?? "-";
}

async function loadServiceIdentity() {
  try {
    const res = await fetch(API_BASE + "/health");
    const json = await res.json();
    if (json.result !== "ok" || !json.data) {
      throw new Error(json.message ?? "unexpected response");
    }
    updateServiceIdentity(json.data);
  } catch (e) {
    document.getElementById("serviceBuildDate").textContent = "Unavailable";
    radioLog("Service identity unavailable: " + e.message, "warn");
  }
}

function apiHeaders(includeBody = false) {
  const h = {};
  if (globalToken) h["Authorization"] = "Bearer " + globalToken;
  if (includeBody) h["Content-Type"] = "application/json";
  return h;
}

function selectedRadioId() {
  return document.getElementById("radioSel").value;
}

function onTokenChange() {
  globalToken = document.getElementById("apiToken").value.trim();
  // Sync token field to SSE tab
  document.getElementById("sseToken").value = globalToken;
}

function showTab(tabId) {
  document.querySelectorAll(".tab-content").forEach(tc => tc.classList.remove("active"));
  document.querySelectorAll(".tab").forEach(t => t.classList.remove("active"));
  document.getElementById(tabId).classList.add("active");
  document.querySelector(`.tab[onclick*="${tabId}"]`).classList.add("active");
}

/* =====================================================
   Radio console logger
===================================================== */
function radioLog(msg, type = "ok") {
  const div = document.getElementById("radioConsole");
  const line = document.createElement("div");
  line.className = type;
  line.style.fontSize = "11px";
  line.style.marginBottom = "2px";
  const time = new Date().toISOString().split("T")[1].substring(0, 12);
  line.textContent = time + " | " + msg;
  div.appendChild(line);
  div.scrollTop = div.scrollHeight;
}

/* =====================================================
   Radio state display
===================================================== */
function statusBadge(status) {
  const cls = {
    ready: "badge-ready",
    offline: "badge-offline",
    busy: "badge-busy",
    recovering: "badge-recovering",
    discovering: "badge-discovering"
  }[status] || "badge-offline";
  return `<span class="badge ${cls}">${status}</span>`;
}

function updateStateDisplay(radio) {
  if (!radio) {
    ["stStatus","stChannel","stPower","stActive","stFreqs","stPwrRange"].forEach(id => {
      document.getElementById(id).textContent = "-";
    });
    return;
  }
  document.getElementById("stStatus").innerHTML = statusBadge(radio.status ?? "offline");
  document.getElementById("stChannel").textContent = radio.channelIndex ?? "-";
  document.getElementById("stPower").textContent =
    radio.powerWatts != null ? radio.powerWatts.toFixed(2) + " W" : "-";
  document.getElementById("stActive").textContent = radio.isActive ? "Yes" : "No";

  const cap = radio.capabilities ?? {};
  document.getElementById("stFreqs").textContent =
    (cap.frequenciesMhz ?? []).join(", ") || "-";
  const pr = cap.powerRangeWatts;
  document.getElementById("stPwrRange").textContent =
    pr ? `${pr.min} – ${pr.max} W` : "-";
}

function populateChannelDropdown(radio) {
  const sel = document.getElementById("chanSel");
  sel.innerHTML = "";
  if (!radio) return;
  const freqs = radio.capabilities?.frequenciesMhz ?? [];
  if (freqs.length === 0) {
    const opt = document.createElement("option");
    opt.textContent = "No channels available";
    sel.appendChild(opt);
    return;
  }
  freqs.forEach((freq, idx) => {
    const opt = document.createElement("option");
    opt.value = idx;
    opt.dataset.freq = freq;
    opt.textContent = `Ch ${idx} \u2014 ${freq} MHz`;
    sel.appendChild(opt);
  });
  if (radio.channelIndex != null) sel.value = radio.channelIndex;
}

function onRadioSelected() {
  const id = selectedRadioId();
  const radio = radiosCache.find(r => r.id === id);
  updateStateDisplay(radio ?? null);
  populateChannelDropdown(radio ?? null);
}

/* =====================================================
   REST API calls
===================================================== */
async function loadRadios() {
  radioLog("GET /api/v1/radios");
  try {
    const res = await fetch(API_BASE + "/radios", { headers: apiHeaders() });
    const json = await res.json();
    if (json.result !== "ok") {
      radioLog("Error: " + (json.message ?? "unknown"), "err");
      return;
    }
    radiosCache = json.data.radios ?? [];
    const active = json.data.active ?? null;

    // Mark active
    radiosCache.forEach(r => { r.isActive = (r.id === active); });

    const sel = document.getElementById("radioSel");
    const prev = sel.value;
    sel.innerHTML = "";
    radiosCache.forEach(r => {
      const opt = document.createElement("option");
      opt.value = r.id;
      opt.textContent = r.id + (r.isActive ? " ★" : "") + " [" + (r.status ?? "offline") + "]";
      sel.appendChild(opt);
    });
    // Try to restore selection, or pick active
    if (radiosCache.some(r => r.id === prev)) {
      sel.value = prev;
    } else if (active && radiosCache.some(r => r.id === active)) {
      sel.value = active;
    }

    onRadioSelected();
    radioLog("Loaded " + radiosCache.length + " radio(s). Active: " + (active ?? "none"), "ok");
  } catch (e) {
    radioLog("Exception: " + e.message, "err");
  }
}

async function connectRadio() {
  const radioId = selectedRadioId();
  if (!radioId) { radioLog("Select a radio first", "warn"); return; }
  radioLog("POST /api/v1/radio/connect  radioId=" + radioId);
  try {
    const res = await fetch(API_BASE + "/radio/connect", {
      method: "POST",
      headers: apiHeaders(true),
      body: JSON.stringify({ radioId })
    });
    const json = await res.json();
    if (json.result === "ok") {
      radioLog("Connect accepted: " + (json.message ?? "ok"), "ok");
      await loadRadios();
    } else {
      radioLog("Error: " + (json.message ?? "rejected"), "err");
    }
  } catch (e) {
    radioLog("Exception: " + e.message, "err");
  }
}

async function setChannel() {
  const radioId = selectedRadioId();
  if (!radioId) { radioLog("Select a radio first", "warn"); return; }
  const chanSel = document.getElementById("chanSel");
  const channelIndex = parseInt(chanSel.value);
  const frequencyMhz = parseFloat(chanSel.selectedOptions[0]?.dataset.freq ?? "0");
  if (isNaN(channelIndex)) {
    radioLog("Select a channel from the dropdown", "warn"); return;
  }
  radioLog(`PUT /api/v1/radio/channel  radioId=${radioId} channelIndex=${channelIndex} frequencyMhz=${frequencyMhz}`);
  try {
    const res = await fetch(API_BASE + "/radio/channel", {
      method: "PUT",
      headers: apiHeaders(true),
      body: JSON.stringify({ radioId, channelIndex, frequencyMhz })
    });
    const json = await res.json();
    if (json.result === "ok") {
      radioLog("Channel set: " + (json.message ?? "ok"), "ok");
      await loadRadios();
    } else {
      radioLog("Error: " + (json.message ?? "rejected"), "err");
    }
  } catch (e) {
    radioLog("Exception: " + e.message, "err");
  }
}

async function setPower() {
  const radioId = selectedRadioId();
  if (!radioId) { radioLog("Select a radio first", "warn"); return; }
  const watts = parseFloat(document.getElementById("pwrWatts").value);
  if (isNaN(watts) || watts < 0) {
    radioLog("Enter a valid power value (watts >= 0)", "warn"); return;
  }
  radioLog(`PUT /api/v1/radio/power  radioId=${radioId} watts=${watts}`);
  try {
    const res = await fetch(API_BASE + "/radio/power", {
      method: "PUT",
      headers: apiHeaders(true),
      body: JSON.stringify({ radioId, watts })
    });
    const json = await res.json();
    if (json.result === "ok") {
      radioLog("Power set: " + (json.message ?? "ok"), "ok");
      await loadRadios();
    } else {
      radioLog("Error: " + (json.message ?? "rejected"), "err");
    }
  } catch (e) {
    radioLog("Exception: " + e.message, "err");
  }
}

/* =====================================================
   SSE event handling (updates Radio Control tab live)
===================================================== */
function handleRadioEvent(tag, data) {
  const id = data.radioId ?? "";
  if (!id || id !== selectedRadioId()) return;

  const radio = radiosCache.find(r => r.id === id);
  if (!radio) return;

  if (tag === "rcc.radio.state") {
    radio.status = data.status ?? radio.status;
    radio.channelIndex = data.channelIndex ?? radio.channelIndex;
    radio.powerWatts = data.powerWatts ?? radio.powerWatts;
  } else if (tag === "rcc.radio.channel") {
    radio.channelIndex = data.channelIndex ?? radio.channelIndex;
  } else if (tag === "rcc.radio.power") {
    radio.powerWatts = data.powerWatts ?? radio.powerWatts;
  }
  updateStateDisplay(radio);
}

/* =====================================================
   SSE Connection
===================================================== */
const defaultSsePort = parseInt(window.location.port) + 1 || 8083;
document.getElementById("sseUrl").value =
  `http://${window.location.hostname}:${defaultSsePort}/api/v1/telemetry`;

const eventFilters = new Set(
  Array.from(document.querySelectorAll(".event-filter"))
    .filter(cb => cb.checked).map(cb => cb.value)
);

document.addEventListener("change", (e) => {
  if (!e.target.classList.contains("event-filter")) return;
  if (e.target.checked) eventFilters.add(e.target.value);
  else eventFilters.delete(e.target.value);
  applyFilters();
});

let sseController = null;
let eventCount = 0;

function addSseEvent(type, data, id = null) {
  const consoleDiv = document.getElementById("sseConsole");
  const eventDiv = document.createElement("div");
  eventDiv.className = `event ${type}`;
  eventDiv.style.borderLeft = "3px solid var(--edge-accent)";
  eventDiv.style.marginBottom = "6px";
  eventDiv.style.padding = "6px";
  eventDiv.style.background = "var(--edge-surface)";

  const ts = new Date().toISOString();
  const header = document.createElement("div");
  header.style.color = type === "rcc.fault" ? "#c62828" : "var(--edge-ink)";
  header.textContent = `[${ts}] ${type}${id ? "  #" + id : ""}`;

  const body = document.createElement("pre");
  body.style.margin = "4px 0 0 0";
  body.style.color = "var(--edge-ink-muted)";
  body.style.fontSize = "12px";
  body.style.fontFamily = "monospace";
  body.textContent = JSON.stringify(data, null, 2);

  eventDiv.appendChild(header);
  eventDiv.appendChild(body);
  eventCount++;
  document.getElementById("counter").textContent = `Events: ${eventCount}`;

  if (!eventFilters.has(type)) eventDiv.style.display = "none";

  consoleDiv.insertBefore(eventDiv, consoleDiv.firstChild);
  while (consoleDiv.children.length > 200) consoleDiv.removeChild(consoleDiv.lastChild);
}

function applyFilters() {
  document.querySelectorAll("#sseConsole .event").forEach(ev => {
    const type = Array.from(ev.classList).find(c => c !== "event");
    if (!type) return;
    ev.style.display = eventFilters.has(type) ? "" : "none";
  });
}

async function connect() {
  disconnect();
  const url = document.getElementById("sseUrl").value;
  const token = document.getElementById("sseToken").value;
  sseController = new AbortController();

  try {
    const response = await fetch(url, {
      headers: { "Authorization": `Bearer ${token}` },
      signal: sseController.signal
    });
    if (!response.body) throw new Error("ReadableStream not supported");

    updateSseStatus(true);
    addSseEvent("SYSTEM", { message: "SSE connection established" });

    const reader = response.body.getReader();
    const decoder = new TextDecoder("utf-8");
    let buffer = "";

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      buffer += decoder.decode(value, { stream: true });
      const parts = buffer.split("\n\n");
      buffer = parts.pop();
      for (const part of parts) {
        if (!part.trim()) continue;
        let tag = "MESSAGE", eventData = "", eventId = null;
        part.split("\n").forEach(line => {
          if (line.startsWith("event:")) tag = line.replace("event:", "").trim();
          else if (line.startsWith("data:")) eventData += line.replace("data:", "").trim();
          else if (line.startsWith("id:")) eventId = line.replace("id:", "").trim();
        });
        let parsed;
        try { parsed = JSON.parse(eventData); } catch { parsed = eventData; }
        if (tag === "MESSAGE") tag = "SYSTEM";

        // Live-update radio tab for radio events
        if (tag.startsWith("rcc.radio.") && typeof parsed === "object") {
          handleRadioEvent(tag, parsed);
        }
        addSseEvent(tag, parsed, eventId);
      }
    }
  } catch (err) {
    if (err.name !== "AbortError") {
      updateSseStatus(false);
      addSseEvent("ERROR", { message: "Connection error: " + err.message });
    }
  }
}

function disconnect() {
  if (sseController) {
    sseController.abort();
    sseController = null;
    updateSseStatus(false);
  }
}

function updateSseStatus(connected) {
  const el = document.getElementById("sseStatus");
  el.textContent = connected ? "● Connected" : "● Disconnected";
  el.className = connected ? "connected" : "disconnected";
  document.getElementById("btnConnect").disabled = connected;
  document.getElementById("btnDisconnect").disabled = !connected;
}

function clearEvents() {
  document.getElementById("sseConsole").innerHTML = "";
  eventCount = 0;
  document.getElementById("counter").textContent = "Events: 0";
}

/* =====================================================
   Init
===================================================== */
async function fetchDevToken() {
  try {
    const res = await fetch("/api/v1/dev-token");
    const json = await res.json();
    if (json.result === "ok" && json.data?.token) {
      globalToken = json.data.token;
      document.getElementById("apiToken").value = globalToken;
      document.getElementById("sseToken").value = globalToken;
      radioLog("Dev token loaded automatically (operator scope)", "ok");
    } else {
      radioLog("Dev token unavailable: " + (json.message ?? "no token in response"), "warn");
    }
  } catch (e) {
    radioLog("Could not fetch dev token: " + e.message, "warn");
  }
}

async function init() {
  await loadServiceIdentity();
  await fetchDevToken();
  await loadRadios();
}
init();
</script>
</body>
</html>
)html";

} // namespace rcc::api
