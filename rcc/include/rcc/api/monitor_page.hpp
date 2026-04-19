#pragma once
#include <string_view>

namespace rcc::api {

constexpr std::string_view RADIO_MONITOR_HTML = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Radio Control Monitor</title>
  <style>
    :root {
      --accent: #ff5a1f;
      --accent-strong: #d94817;
      --ink: #25303a;
      --ink-soft: #61707d;
      --surface: #ffffff;
      --surface-soft: #f2f5f7;
      --line: #ccd5dd;
      --bg-top: #f8fafc;
      --bg-bottom: #eef3f7;
      --shadow: 0 16px 40px rgba(25, 35, 45, 0.08);
      --ok: #2d8a42;
      --warn: #b26a00;
      --err: #c4382b;
    }

    body {
      margin: 0;
      font-family: "Barlow", "Segoe UI", Arial, sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at top left, rgba(255, 90, 31, 0.08), transparent 34%),
        linear-gradient(180deg, var(--bg-top), var(--bg-bottom));
      min-height: 100vh;
    }

    .shell {
      max-width: 1280px;
      margin: 0 auto;
      padding: 24px;
    }

    .hero {
      display: grid;
      grid-template-columns: minmax(0, 1.35fr) minmax(320px, 0.9fr);
      gap: 18px;
      margin-bottom: 18px;
    }

    .hero-card,
    .panel {
      background: rgba(255, 255, 255, 0.94);
      border: 1px solid rgba(204, 213, 221, 0.85);
      border-radius: 18px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(10px);
    }

    .hero-card {
      padding: 20px 22px;
    }

    .hero-kicker {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      font-size: 0.82rem;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      color: var(--accent);
      font-weight: 700;
      margin-bottom: 10px;
    }

    h1 {
      margin: 0 0 10px;
      font-size: clamp(1.8rem, 2vw, 2.5rem);
      line-height: 1.05;
    }

    .hero-copy {
      margin: 0;
      color: var(--ink-soft);
      font-size: 1rem;
      max-width: 56ch;
    }

    .identity-grid,
    .state-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px;
    }

    .metric {
      padding: 12px 14px;
      border-radius: 14px;
      background: var(--surface-soft);
      border: 1px solid var(--line);
    }

    .metric-label {
      display: block;
      font-size: 0.8rem;
      color: var(--ink-soft);
      text-transform: uppercase;
      letter-spacing: 0.06em;
      margin-bottom: 6px;
    }

    .metric-value {
      font-size: 1.02rem;
      font-weight: 700;
      word-break: break-word;
    }

    .layout {
      display: grid;
      grid-template-columns: minmax(0, 0.95fr) minmax(0, 1.05fr);
      gap: 18px;
      align-items: start;
    }

    .panel {
      padding: 18px;
    }

    .panel h2 {
      margin: 0 0 12px;
      font-size: 1.05rem;
      letter-spacing: 0.02em;
    }

    .stack {
      display: grid;
      gap: 14px;
    }

    .control-row,
    .button-row {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      align-items: center;
    }

    label {
      display: block;
      font-size: 0.82rem;
      font-weight: 700;
      color: var(--ink-soft);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      margin-bottom: 6px;
    }

    input,
    select,
    button,
    textarea {
      font: inherit;
      border-radius: 12px;
    }

    input,
    select,
    textarea {
      width: 100%;
      box-sizing: border-box;
      padding: 10px 12px;
      border: 1px solid var(--line);
      background: var(--surface);
      color: var(--ink);
    }

    button {
      border: 1px solid var(--accent);
      background: var(--accent);
      color: #fff;
      padding: 10px 14px;
      font-weight: 700;
      cursor: pointer;
      transition: transform 0.16s ease, background 0.16s ease, border-color 0.16s ease;
    }

    button:hover {
      transform: translateY(-1px);
      background: var(--accent-strong);
      border-color: var(--accent-strong);
    }

    button.secondary {
      background: var(--surface);
      color: var(--ink);
      border-color: var(--line);
    }

    button.secondary:hover {
      background: var(--surface-soft);
      border-color: var(--ink-soft);
    }

    button:disabled {
      cursor: not-allowed;
      opacity: 0.55;
      transform: none;
    }

    .state-grid {
      margin-bottom: 14px;
    }

    .state-list {
      display: grid;
      gap: 10px;
    }

    .state-row {
      display: grid;
      grid-template-columns: 140px minmax(0, 1fr);
      gap: 12px;
      align-items: start;
      padding: 10px 12px;
      border-radius: 12px;
      background: var(--surface-soft);
      border: 1px solid var(--line);
    }

    .state-row dt {
      margin: 0;
      font-size: 0.82rem;
      color: var(--ink-soft);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      font-weight: 700;
    }

    .state-row dd {
      margin: 0;
      font-weight: 600;
      word-break: break-word;
    }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      border-radius: 999px;
      padding: 6px 10px;
      font-size: 0.82rem;
      font-weight: 700;
      border: 1px solid transparent;
    }

    .badge-online {
      color: var(--ok);
      background: rgba(45, 138, 66, 0.1);
      border-color: rgba(45, 138, 66, 0.2);
    }

    .badge-recovering {
      color: var(--warn);
      background: rgba(178, 106, 0, 0.12);
      border-color: rgba(178, 106, 0, 0.18);
    }

    .badge-offline {
      color: var(--err);
      background: rgba(196, 56, 43, 0.1);
      border-color: rgba(196, 56, 43, 0.16);
    }

    .hint {
      color: var(--ink-soft);
      font-size: 0.92rem;
      margin: 0;
    }

    .inline-note {
      font-size: 0.86rem;
      color: var(--ink-soft);
    }

    .filter-row {
      display: flex;
      flex-wrap: wrap;
      gap: 10px 14px;
      margin: 10px 0 4px;
    }

    .filter-chip {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: var(--surface-soft);
      font-size: 0.86rem;
    }

    .filter-chip input {
      width: auto;
      margin: 0;
      padding: 0;
    }

    .console {
      min-height: 260px;
      max-height: 560px;
      overflow: auto;
      padding: 12px;
      border-radius: 16px;
      background:
        linear-gradient(180deg, rgba(37, 48, 58, 0.97), rgba(20, 28, 35, 0.98));
      color: #edf3f8;
      border: 1px solid rgba(37, 48, 58, 0.12);
      font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
      font-size: 0.82rem;
      line-height: 1.45;
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.04);
    }

    .log-line {
      margin-bottom: 6px;
      white-space: pre-wrap;
      word-break: break-word;
    }

    .log-info { color: #b8d1e2; }
    .log-ok { color: #84d7a8; }
    .log-warn { color: #f0c36d; }
    .log-error { color: #ff9f93; }

    .event-card {
      border-left: 3px solid var(--accent);
      padding: 10px 12px;
      border-radius: 12px;
      background: rgba(255, 255, 255, 0.05);
      margin-bottom: 10px;
    }

    .event-header {
      display: flex;
      justify-content: space-between;
      gap: 8px;
      margin-bottom: 6px;
      color: #fff;
      font-weight: 700;
    }

    .event-meta {
      color: #a8bac9;
      font-size: 0.78rem;
    }

    .event-payload {
      margin: 0;
      white-space: pre-wrap;
      word-break: break-word;
      color: #d9e7f2;
    }

    @media (max-width: 980px) {
      .hero,
      .layout,
      .identity-grid,
      .state-grid {
        grid-template-columns: 1fr;
      }

      .state-row {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
<div class="shell">
  <section class="hero">
    <article class="hero-card">
      <div class="hero-kicker">EDGE RCC</div>
      <h1>Radio Control Monitor</h1>
      <p class="hero-copy">
        Field-facing diagnostics for radio selection, channel and power control,
        and the live SSE stream exposed by the RCC C++ service.
      </p>
    </article>
    <article class="hero-card">
      <div class="identity-grid">
        <div class="metric">
          <span class="metric-label">Service</span>
          <span id="serviceName" class="metric-value">radio-control</span>
        </div>
        <div class="metric">
          <span class="metric-label">Health</span>
          <span id="serviceHealth" class="metric-value">Loading...</span>
        </div>
        <div class="metric">
          <span class="metric-label">Container Version</span>
          <span id="serviceVersion" class="metric-value">Loading...</span>
        </div>
        <div class="metric">
          <span class="metric-label">Git Version</span>
          <span id="serviceGitVersion" class="metric-value">Loading...</span>
        </div>
        <div class="metric">
          <span class="metric-label">Build Time</span>
          <span id="serviceBuildDate" class="metric-value">Loading...</span>
        </div>
        <div class="metric">
          <span class="metric-label">Schema</span>
          <span id="serviceSchema" class="metric-value">Loading...</span>
        </div>
      </div>
    </article>
  </section>

  <section class="layout">
    <article class="panel stack">
      <div>
        <h2>Control Surface</h2>
        <p class="hint">
          REST commands target the RCC `/api/v1` contract directly. When auth is enabled,
          paste a bearer token or use the bench-only dev token helper.
        </p>
      </div>

      <div>
        <label for="apiToken">Bearer Token</label>
        <input id="apiToken" type="text" placeholder="JWT for /api/v1/radios, /power, /channel" />
      </div>

      <div class="button-row">
        <button class="secondary" type="button" onclick="fetchDevToken()">Fetch Dev Token</button>
        <button class="secondary" type="button" onclick="loadServiceIdentity()">Refresh Health</button>
        <button class="secondary" type="button" onclick="loadRadios()">Refresh Radios</button>
      </div>

      <div>
        <label for="radioSel">Known Radios</label>
        <div class="control-row">
          <select id="radioSel" style="flex: 1 1 280px;" onchange="onRadioSelected()"></select>
          <button type="button" onclick="selectRadio()">Select Active</button>
        </div>
      </div>

      <div class="state-grid">
        <div class="metric">
          <span class="metric-label">Selected Radio</span>
          <span id="selectedRadioId" class="metric-value">-</span>
        </div>
        <div class="metric">
          <span class="metric-label">Active Radio</span>
          <span id="activeRadioId" class="metric-value">-</span>
        </div>
        <div class="metric">
          <span class="metric-label">Status</span>
          <span id="radioStatus" class="metric-value">-</span>
        </div>
        <div class="metric">
          <span class="metric-label">Derived Channel</span>
          <span id="radioChannelIndex" class="metric-value">-</span>
        </div>
      </div>

      <dl class="state-list">
        <div class="state-row">
          <dt>Frequency</dt>
          <dd id="radioFrequency">-</dd>
        </div>
        <div class="state-row">
          <dt>Power</dt>
          <dd id="radioPower">-</dd>
        </div>
        <div class="state-row">
          <dt>Power Range</dt>
          <dd id="radioPowerRange">-</dd>
        </div>
        <div class="state-row">
          <dt>Supported Channels</dt>
          <dd id="radioChannels">-</dd>
        </div>
      </dl>

      <div class="control-row">
        <div style="flex: 1 1 280px;">
          <label for="chanSel">Set Channel</label>
          <select id="chanSel"></select>
        </div>
        <div style="align-self: end;">
          <button type="button" onclick="setChannel()">POST /radios/{id}/channel</button>
        </div>
      </div>

      <div class="control-row">
        <div style="flex: 1 1 220px;">
          <label for="pwrDbm">Set Power (dBm)</label>
          <input id="pwrDbm" type="number" min="0" max="39" step="1" value="10" />
        </div>
        <div style="align-self: end;">
          <button type="button" onclick="setPower()">POST /radios/{id}/power</button>
        </div>
      </div>

      <div>
        <h2>REST Activity</h2>
        <div id="radioConsole" class="console"></div>
      </div>
    </article>

    <article class="panel stack">
      <div>
        <h2>Telemetry Stream</h2>
        <p class="hint">
          Watches the SSE v1 stream with `ready`, `state`, `channelChanged`, `powerChanged`,
          `fault`, and `heartbeat`, and stores `Last-Event-ID` in localStorage for resume.
        </p>
      </div>

      <div>
        <label for="sseUrl">SSE URL</label>
        <input id="sseUrl" type="text" />
      </div>

      <div>
        <label for="sseToken">SSE Bearer Token</label>
        <input id="sseToken" type="text" placeholder="If blank, connect without Authorization header" />
      </div>

      <div class="control-row">
        <div style="flex: 1 1 220px;">
          <label>Connection</label>
          <div id="sseStatus" class="badge badge-offline">Disconnected</div>
        </div>
        <div style="flex: 1 1 220px;">
          <label>Last Event ID</label>
          <div id="lastEventId" class="inline-note">none</div>
        </div>
        <div style="flex: 1 1 220px;">
          <label>Event Count</label>
          <div id="eventCount" class="inline-note">0</div>
        </div>
      </div>

      <div class="button-row">
        <button id="connectBtn" type="button" onclick="connectSse()">Connect SSE</button>
        <button id="disconnectBtn" class="secondary" type="button" onclick="disconnectSse()" disabled>Disconnect</button>
        <button id="clearEventsBtn" class="secondary" type="button" onclick="clearSseEvents()">Clear Console</button>
        <button id="resetResumeBtn" class="secondary" type="button" onclick="resetResumeState()">Reset Resume</button>
      </div>

      <div>
        <label>Event Filters</label>
        <div class="filter-row">
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="ready" checked />ready</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="state" checked />state</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="channelChanged" checked />channelChanged</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="powerChanged" checked />powerChanged</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="fault" checked />fault</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="heartbeat" checked />heartbeat</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="SYSTEM" checked />SYSTEM</label>
          <label class="filter-chip"><input class="event-filter" type="checkbox" value="ERROR" checked />ERROR</label>
        </div>
      </div>

      <div>
        <h2>Event Console</h2>
        <div id="sseConsole" class="console"></div>
      </div>
    </article>
  </section>
</div>
<script>
const API_BASE = "/api/v1";
const LAST_EVENT_ID_KEY = "rcc.monitor.lastEventId";
let globalToken = "";
let radiosCache = [];
let sseController = null;
let eventCount = 0;
let lastEventId = window.localStorage.getItem(LAST_EVENT_ID_KEY) || "";
let activeRadioId = null;
const eventFilters = new Set();

function setToken(value) {
  globalToken = value.trim();
  document.getElementById("apiToken").value = globalToken;
  document.getElementById("sseToken").value = globalToken;
}

function apiHeaders(includeBody = false) {
  const headers = {};
  if (globalToken) {
    headers["Authorization"] = "Bearer " + globalToken;
  }
  if (includeBody) {
    headers["Content-Type"] = "application/json";
  }
  return headers;
}

function selectedRadioId() {
  return document.getElementById("radioSel").value;
}

function logLine(targetId, message, level = "info") {
  const target = document.getElementById(targetId);
  const line = document.createElement("div");
  line.className = "log-line log-" + level;
  line.textContent = new Date().toISOString() + " | " + message;
  target.appendChild(line);
  target.scrollTop = target.scrollHeight;
}

function formatNullable(value, suffix = "") {
  if (value === null || value === undefined || value === "") {
    return "-";
  }
  return String(value) + suffix;
}

function statusBadge(status) {
  const normalized = status || "offline";
  const css = normalized === "online"
    ? "badge-online"
    : normalized === "recovering"
    ? "badge-recovering"
    : "badge-offline";
  return `<span class="badge ${css}">${normalized}</span>`;
}

function deriveChannelIndex(radio) {
  if (!radio) return null;
  if (radio.state && Number.isInteger(radio.state.channelIndex)) {
    return radio.state.channelIndex;
  }
  const frequency = radio.state?.frequencyMhz;
  const channels = radio.capabilities?.channels || [];
  if (frequency === null || frequency === undefined) {
    return null;
  }
  const match = channels.find((channel) => Math.abs(channel.frequencyMhz - frequency) < 0.001);
  return match ? match.index : null;
}

function populateChannelDropdown(radio) {
  const select = document.getElementById("chanSel");
  select.innerHTML = "";

  if (!radio || !(radio.capabilities?.channels || []).length) {
    const option = document.createElement("option");
    option.textContent = "No supported channels";
    option.value = "";
    select.appendChild(option);
    return;
  }

  radio.capabilities.channels.forEach((channel) => {
    const option = document.createElement("option");
    option.value = String(channel.index);
    option.textContent = `Ch ${channel.index} | ${channel.frequencyMhz} MHz`;
    select.appendChild(option);
  });

  const selectedIndex = deriveChannelIndex(radio);
  if (selectedIndex !== null) {
    select.value = String(selectedIndex);
  }
}

function renderRadioState(radio) {
  document.getElementById("selectedRadioId").textContent = selectedRadioId() || "-";
  document.getElementById("activeRadioId").textContent = activeRadioId || "-";

  if (!radio) {
    document.getElementById("radioStatus").textContent = "-";
    document.getElementById("radioChannelIndex").textContent = "-";
    document.getElementById("radioFrequency").textContent = "-";
    document.getElementById("radioPower").textContent = "-";
    document.getElementById("radioPowerRange").textContent = "-";
    document.getElementById("radioChannels").textContent = "-";
    populateChannelDropdown(null);
    return;
  }

  const derivedIndex = deriveChannelIndex(radio);
  const channels = radio.capabilities?.channels || [];
  const powerMin = radio.capabilities?.minPowerDbm;
  const powerMax = radio.capabilities?.maxPowerDbm;

  document.getElementById("radioStatus").innerHTML = statusBadge(radio.status || "offline");
  document.getElementById("radioChannelIndex").textContent =
    derivedIndex === null ? "unknown" : "Ch " + derivedIndex;
  document.getElementById("radioFrequency").textContent =
    formatNullable(radio.state?.frequencyMhz, " MHz");
  document.getElementById("radioPower").textContent =
    formatNullable(radio.state?.powerDbm, " dBm");
  document.getElementById("radioPowerRange").textContent =
    powerMin === undefined || powerMax === undefined
      ? "-"
      : `${powerMin} .. ${powerMax} dBm`;
  document.getElementById("radioChannels").textContent =
    channels.length
      ? channels.map((channel) => `Ch ${channel.index} (${channel.frequencyMhz} MHz)`).join(", ")
      : "-";

  populateChannelDropdown(radio);
}

function applyReadySnapshot(snapshot) {
  if (!snapshot || !Array.isArray(snapshot.radios)) {
    return;
  }
  activeRadioId = snapshot.activeRadioId || activeRadioId;
  snapshot.radios.forEach((incoming) => {
    const existing = radiosCache.find((radio) => radio.id === incoming.id);
    if (!existing) {
      radiosCache.push({
        id: incoming.id,
        model: incoming.model,
        status: incoming.status,
        capabilities: { channels: [] },
        state: incoming.state || {}
      });
      return;
    }
    existing.model = incoming.model || existing.model;
    existing.status = incoming.status || existing.status;
    existing.state = { ...(existing.state || {}), ...(incoming.state || {}) };
  });
  refreshRadioDropdown();
}

function applyTelemetryEvent(tag, payload) {
  if (!payload || typeof payload !== "object") {
    return;
  }

  if (tag === "ready") {
    applyReadySnapshot(payload.snapshot);
    return;
  }

  const radioId = payload.radioId;
  if (!radioId) {
    return;
  }

  let radio = radiosCache.find((item) => item.id === radioId);
  if (!radio) {
    radio = {
      id: radioId,
      model: radioId,
      status: "offline",
      capabilities: { channels: [] },
      state: {}
    };
    radiosCache.push(radio);
  }

  if (!radio.state) {
    radio.state = {};
  }

  if (tag === "state") {
    radio.status = payload.status || radio.status;
    if (payload.frequencyMhz !== undefined) {
      radio.state.frequencyMhz = payload.frequencyMhz;
    }
    if (payload.powerDbm !== undefined) {
      radio.state.powerDbm = payload.powerDbm;
    }
    if (payload.channelIndex !== undefined) {
      radio.state.channelIndex = payload.channelIndex;
    }
  } else if (tag === "channelChanged") {
    if (payload.frequencyMhz !== undefined) {
      radio.state.frequencyMhz = payload.frequencyMhz;
    }
    if (payload.channelIndex !== undefined) {
      radio.state.channelIndex = payload.channelIndex;
    }
  } else if (tag === "powerChanged") {
    if (payload.powerDbm !== undefined) {
      radio.state.powerDbm = payload.powerDbm;
    }
  }

  refreshRadioDropdown();
}

function refreshRadioDropdown() {
  const select = document.getElementById("radioSel");
  const previous = select.value;
  select.innerHTML = "";

  if (!radiosCache.length) {
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "No radios loaded";
    select.appendChild(option);
    renderRadioState(null);
    return;
  }

  radiosCache.forEach((radio) => {
    const option = document.createElement("option");
    option.value = radio.id;
    const suffix = radio.id === activeRadioId ? " [active]" : "";
    option.textContent = `${radio.id} (${radio.status || "offline"})${suffix}`;
    select.appendChild(option);
  });

  if (previous && radiosCache.some((radio) => radio.id === previous)) {
    select.value = previous;
  } else if (activeRadioId && radiosCache.some((radio) => radio.id === activeRadioId)) {
    select.value = activeRadioId;
  } else {
    select.selectedIndex = 0;
  }

  const selected = radiosCache.find((radio) => radio.id === selectedRadioId()) || null;
  renderRadioState(selected);
}

function onRadioSelected() {
  const selected = radiosCache.find((radio) => radio.id === selectedRadioId()) || null;
  renderRadioState(selected);
}

function updateIdentityFromHealth(snapshot) {
  const version = snapshot?.version || {};
  document.getElementById("serviceName").textContent = "radio-control";
  document.getElementById("serviceHealth").textContent = snapshot?.status || "-";
  document.getElementById("serviceVersion").textContent = version.container || "-";
  document.getElementById("serviceGitVersion").textContent = version.gitVersion || "-";
  document.getElementById("serviceBuildDate").textContent = version.buildTime || "-";
  document.getElementById("serviceSchema").textContent = version.schema || "-";
}

async function loadServiceIdentity() {
  logLine("radioConsole", "GET " + API_BASE + "/health", "info");
  try {
    const response = await fetch(API_BASE + "/health");
    const snapshot = await response.json();
    if (!response.ok || !snapshot || typeof snapshot !== "object") {
      throw new Error("unexpected health payload");
    }
    updateIdentityFromHealth(snapshot);
    logLine("radioConsole", "Health status -> " + (snapshot.status || "unknown"), "ok");
  } catch (error) {
    logLine("radioConsole", "Health unavailable: " + error.message, "warn");
  }
}

async function loadRadios() {
  logLine("radioConsole", "GET " + API_BASE + "/radios", "info");
  try {
    const response = await fetch(API_BASE + "/radios", { headers: apiHeaders() });
    const body = await response.json();
    if (!response.ok || body.result !== "ok") {
      throw new Error(body.message || "request failed");
    }
    radiosCache = Array.isArray(body.data?.items) ? body.data.items : [];
    activeRadioId = body.data?.activeRadioId || null;
    refreshRadioDropdown();
    logLine("radioConsole", `Loaded ${radiosCache.length} radio(s). Active=${activeRadioId || "none"}`, "ok");
  } catch (error) {
    logLine("radioConsole", "Radio load failed: " + error.message, "error");
  }
}

async function selectRadio() {
  const radioId = selectedRadioId();
  if (!radioId) {
    logLine("radioConsole", "Select a radio before POST /radios/select", "warn");
    return;
  }

  logLine("radioConsole", "POST " + API_BASE + "/radios/select id=" + radioId, "info");
  try {
    const response = await fetch(API_BASE + "/radios/select", {
      method: "POST",
      headers: apiHeaders(true),
      body: JSON.stringify({ id: radioId })
    });
    const body = await response.json();
    if (!response.ok || body.result !== "ok") {
      throw new Error(body.message || "request failed");
    }
    activeRadioId = body.data?.activeRadioId || radioId;
    refreshRadioDropdown();
    logLine("radioConsole", "Active radio updated to " + activeRadioId, "ok");
  } catch (error) {
    logLine("radioConsole", "Select failed: " + error.message, "error");
  }
}

async function setChannel() {
  const radioId = selectedRadioId();
  const select = document.getElementById("chanSel");
  const channelIndex = Number.parseInt(select.value, 10);

  if (!radioId) {
    logLine("radioConsole", "Select a radio before setting channel", "warn");
    return;
  }
  if (!Number.isInteger(channelIndex) || channelIndex < 1) {
    logLine("radioConsole", "Choose a supported channel index first", "warn");
    return;
  }

  const route = `${API_BASE}/radios/${encodeURIComponent(radioId)}/channel`;
  logLine("radioConsole", `POST ${route} channelIndex=${channelIndex}`, "info");
  try {
    const response = await fetch(route, {
      method: "POST",
      headers: apiHeaders(true),
      body: JSON.stringify({ channelIndex })
    });
    const body = await response.json();
    if (!response.ok || body.result !== "ok") {
      throw new Error(body.message || "request failed");
    }
    logLine("radioConsole", "Channel command accepted", "ok");
    await loadRadios();
  } catch (error) {
    logLine("radioConsole", "Set channel failed: " + error.message, "error");
  }
}

async function setPower() {
  const radioId = selectedRadioId();
  const powerDbm = Number(document.getElementById("pwrDbm").value);

  if (!radioId) {
    logLine("radioConsole", "Select a radio before setting power", "warn");
    return;
  }
  if (!Number.isFinite(powerDbm) || powerDbm < 0 || powerDbm > 39) {
    logLine("radioConsole", "Power must be between 0 and 39 dBm", "warn");
    return;
  }

  const route = `${API_BASE}/radios/${encodeURIComponent(radioId)}/power`;
  logLine("radioConsole", `POST ${route} powerDbm=${powerDbm}`, "info");
  try {
    const response = await fetch(route, {
      method: "POST",
      headers: apiHeaders(true),
      body: JSON.stringify({ powerDbm })
    });
    const body = await response.json();
    if (!response.ok || body.result !== "ok") {
      throw new Error(body.message || "request failed");
    }
    logLine("radioConsole", "Power command accepted", "ok");
    await loadRadios();
  } catch (error) {
    logLine("radioConsole", "Set power failed: " + error.message, "error");
  }
}

function updateSseStatus(connected, label) {
  const status = document.getElementById("sseStatus");
  status.textContent = label;
  status.className = connected ? "badge badge-online" : "badge badge-offline";
  document.getElementById("connectBtn").disabled = connected;
  document.getElementById("disconnectBtn").disabled = !connected;
}

function updateLastEventIdDisplay() {
  document.getElementById("lastEventId").textContent = lastEventId || "none";
}

function storeLastEventId(id) {
  if (!id) return;
  lastEventId = String(id);
  window.localStorage.setItem(LAST_EVENT_ID_KEY, lastEventId);
  updateLastEventIdDisplay();
}

function clearSseEvents() {
  document.getElementById("sseConsole").innerHTML = "";
  eventCount = 0;
  document.getElementById("eventCount").textContent = "0";
}

function resetResumeState() {
  lastEventId = "";
  window.localStorage.removeItem(LAST_EVENT_ID_KEY);
  updateLastEventIdDisplay();
  logLine("radioConsole", "Cleared Last-Event-ID resume state", "ok");
}

function addSseEvent(tag, payload, id) {
  if (id) {
    storeLastEventId(id);
  }

  const consoleNode = document.getElementById("sseConsole");
  const card = document.createElement("div");
  card.className = "event-card " + tag;

  const header = document.createElement("div");
  header.className = "event-header";

  const name = document.createElement("span");
  name.textContent = tag;

  const meta = document.createElement("span");
  meta.className = "event-meta";
  meta.textContent = `${new Date().toISOString()}${id ? " | id " + id : ""}`;

  header.appendChild(name);
  header.appendChild(meta);

  const pre = document.createElement("pre");
  pre.className = "event-payload";
  pre.textContent = typeof payload === "string" ? payload : JSON.stringify(payload, null, 2);

  card.appendChild(header);
  card.appendChild(pre);

  if (!eventFilters.has(tag)) {
    card.style.display = "none";
  }

  consoleNode.prepend(card);
  while (consoleNode.children.length > 200) {
    consoleNode.removeChild(consoleNode.lastChild);
  }

  eventCount += 1;
  document.getElementById("eventCount").textContent = String(eventCount);
}

function applyFilterVisibility() {
  document.querySelectorAll("#sseConsole .event-card").forEach((card) => {
    const type = Array.from(card.classList).find((value) => value !== "event-card");
    card.style.display = type && eventFilters.has(type) ? "" : "none";
  });
}

async function connectSse() {
  disconnectSse();

  const url = document.getElementById("sseUrl").value.trim();
  const token = document.getElementById("sseToken").value.trim();
  const headers = {
    "Accept": "text/event-stream",
    "Cache-Control": "no-cache"
  };
  if (token) {
    headers["Authorization"] = "Bearer " + token;
  }
  if (lastEventId) {
    headers["Last-Event-ID"] = lastEventId;
  }

  sseController = new AbortController();
  updateSseStatus(false, "Connecting...");

  try {
    const response = await fetch(url, {
      headers,
      signal: sseController.signal
    });

    if (!response.ok) {
      let detail = response.statusText || "connect failed";
      try {
        const body = await response.text();
        if (body) {
          detail = `${response.status} ${detail}: ${body}`;
        }
      } catch (_) {}
      throw new Error(detail);
    }

    if (!response.body) {
      throw new Error("ReadableStream not supported by this browser");
    }

    updateSseStatus(true, "Connected");
    addSseEvent("SYSTEM", { message: "SSE connection established" }, null);

    const reader = response.body.getReader();
    const decoder = new TextDecoder("utf-8");
    let buffer = "";

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      buffer += decoder.decode(value, { stream: true });

      const parts = buffer.split("\n\n");
      buffer = parts.pop() || "";

      for (const part of parts) {
        if (!part.trim()) {
          continue;
        }

        let tag = "SYSTEM";
        let eventId = null;
        const dataLines = [];

        part.split("\n").forEach((line) => {
          if (line.startsWith("event:")) {
            tag = line.slice(6).trim();
          } else if (line.startsWith("id:")) {
            eventId = line.slice(3).trim();
          } else if (line.startsWith("data:")) {
            dataLines.push(line.slice(5).trim());
          }
        });

        const payloadText = dataLines.join("\n");
        let payload = payloadText;
        if (payloadText) {
          try {
            payload = JSON.parse(payloadText);
          } catch (_) {}
        }

        applyTelemetryEvent(tag, payload);
        if (selectedRadioId()) {
          renderRadioState(radiosCache.find((radio) => radio.id === selectedRadioId()) || null);
        }
        addSseEvent(tag, payload, eventId);
      }
    }

    updateSseStatus(false, "Disconnected");
    addSseEvent("SYSTEM", { message: "SSE stream closed by server" }, null);
  } catch (error) {
    if (error.name === "AbortError") {
      addSseEvent("SYSTEM", { message: "SSE connection closed by operator" }, null);
    } else {
      updateSseStatus(false, "Disconnected");
      addSseEvent("ERROR", { message: error.message }, null);
      logLine("radioConsole", "SSE connect failed: " + error.message, "error");
    }
  }
}

function disconnectSse() {
  if (!sseController) {
    updateSseStatus(false, "Disconnected");
    return;
  }
  sseController.abort();
  sseController = null;
  updateSseStatus(false, "Disconnected");
}

async function fetchDevToken() {
  try {
    const response = await fetch(API_BASE + "/dev-token");
    const body = await response.json();
    if (response.ok && body.result === "ok" && body.data?.token) {
      setToken(body.data.token);
      logLine("radioConsole", "Loaded dev token for monitor bench use", "ok");
    } else {
      logLine("radioConsole", "Dev token unavailable: " + (body.message || "request failed"), "warn");
    }
  } catch (error) {
    logLine("radioConsole", "Could not fetch dev token: " + error.message, "warn");
  }
}

function defaultSseUrl() {
  const currentPort = Number.parseInt(window.location.port || "8080", 10);
  const ssePort = Number.isFinite(currentPort) ? currentPort + 1 : 8081;
  return `${window.location.protocol}//${window.location.hostname}:${ssePort}/api/v1/telemetry`;
}

function initializeFilters() {
  document.querySelectorAll(".event-filter").forEach((checkbox) => {
    if (checkbox.checked) {
      eventFilters.add(checkbox.value);
    }
    checkbox.addEventListener("change", () => {
      if (checkbox.checked) {
        eventFilters.add(checkbox.value);
      } else {
        eventFilters.delete(checkbox.value);
      }
      applyFilterVisibility();
    });
  });
}

async function init() {
  document.getElementById("sseUrl").value = defaultSseUrl();
  document.getElementById("apiToken").addEventListener("input", (event) => setToken(event.target.value));
  initializeFilters();
  updateLastEventIdDisplay();
  updateSseStatus(false, "Disconnected");
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
