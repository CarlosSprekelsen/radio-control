#include "web_assets.hpp"
std::string get_web_index_html() {
    return R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Silvus Mock Web UI</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 24px; }
    .card { border: 1px solid #ccc; border-radius: 8px; padding: 16px; margin-bottom: 16px; max-width: 680px; }
    .card h2 { margin-top: 0; }
    label { display: block; margin-bottom: 8px; }
    input[type=text], input[type=number] { width: 100%; padding: 8px; box-sizing: border-box; }
    button { padding: 10px 14px; margin-right: 8px; margin-top: 8px; }
    pre { background: #f4f4f4; padding: 12px; overflow-x: auto; }
  </style>
</head>
<body>
  <h1>Silvus Mock Web UI</h1>
  <div class="card">
    <h2>Radio Status</h2>
    <p><strong>Frequency:</strong> <span id="frequency">-</span> MHz</p>
    <p><strong>Power:</strong> <span id="power">-</span> dBm</p>
    <p><strong>Availability:</strong> <span id="available">-</span></p>
    <p><strong>Blackout:</strong> <span id="blackout">-</span>s remaining</p>
    <p><strong>Supported Profiles:</strong></p>
    <pre id="profiles">Loading...</pre>
  </div>

  <div class="card">
    <h2>Control</h2>
    <label>Set Frequency (MHz): <input id="freq-input" type="text" value="4700.0"></label>
    <button id="set-freq">Set Frequency</button>
    <label>Set Power (dBm): <input id="power-input" type="number" min="0" max="39" value="30"></label>
    <button id="set-power">Set Power</button>
    <div style="margin-top: 12px;">
      <button id="refresh-status">Refresh Status</button>
      <button id="zeroize">Zeroize</button>
      <button id="radio-reset">Radio Reset</button>
      <button id="factory-reset">Factory Reset</button>
    </div>
  </div>

  <div class="card">
    <h2>JSON-RPC Log</h2>
    <pre id="log">Ready.</pre>
  </div>

  <script>
    async function fetchStatus() {
      const resp = await fetch('/status');
      const json = await resp.json();
      document.getElementById('frequency').textContent = json.frequency;
      document.getElementById('power').textContent = json.power_dBm;
      document.getElementById('available').textContent = json.available ? 'yes' : 'no';
      document.getElementById('blackout').textContent = json.blackoutUntil > 0 ? json.blackoutUntil : '0';
      document.getElementById('profiles').textContent = JSON.stringify(json.supported_frequency_profiles, null, 2);
    }

    async function sendRpc(method, params = []) {
      const payload = { jsonrpc: '2.0', method, params, id: method + '-' + Date.now() };
      const resp = await fetch('/streamscape_api', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await resp.json();
      const log = document.getElementById('log');
      log.textContent = JSON.stringify(data, null, 2) + '\n\n' + log.textContent;
      await fetchStatus();
    }

    document.getElementById('set-freq').addEventListener('click', async () => {
      const value = document.getElementById('freq-input').value.trim();
      await sendRpc('freq', [value]);
    });

    document.getElementById('set-power').addEventListener('click', async () => {
      const value = document.getElementById('power-input').value.trim();
      await sendRpc('power_dBm', [value]);
    });

    document.getElementById('refresh-status').addEventListener('click', fetchStatus);
    document.getElementById('zeroize').addEventListener('click', () => sendRpc('zeroize'));
    document.getElementById('radio-reset').addEventListener('click', () => sendRpc('radio_reset'));
    document.getElementById('factory-reset').addEventListener('click', () => sendRpc('factory_reset'));

    fetchStatus().catch(err => {
      document.getElementById('log').textContent = 'Failed to load status: ' + err;
    });
  </script>
</body>
</html>
)html";
}
