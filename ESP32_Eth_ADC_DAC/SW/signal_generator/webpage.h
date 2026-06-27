/*
 * webpage.h
 * ---------
 * HTML/CSS/JS web stranica za projekat F - kao niz karaktera u flash memoriji.
 * Otvara se kada korisnik pristupi root URL-u ESP32 servera.
 *
 * Stranica koristi:
 *   - Chart.js (učitan sa CDN-a) za grafičke prikaze
 *   - WebSocket za real-time podatke od ESP32-a
 *   - Vanilla JavaScript (bez React/Vue/jQuery) da bude lakša za reviziju
 */

#ifndef WEBPAGE_H
#define WEBPAGE_H

const char WEBPAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Signal Generator - Projekat F</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: #1a1d2e;
      color: #e8e9f3;
      padding: 20px;
      min-height: 100vh;
    }
    h1 {
      color: #4dd0e1;
      text-align: center;
      margin-bottom: 8px;
      font-size: 28px;
    }
    .subtitle {
      text-align: center;
      color: #8a8fa3;
      margin-bottom: 24px;
      font-size: 14px;
    }
    .status-bar {
      background: #252840;
      padding: 12px 16px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 12px;
      border-left: 4px solid #4dd0e1;
    }
    .status-item { font-size: 14px; }
    .status-label { color: #8a8fa3; }
    .status-value { color: #fff; font-weight: 600; margin-left: 6px; }
    .ws-connected { color: #4caf50; }
    .ws-disconnected { color: #f44336; }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      margin-bottom: 20px;
    }
    @media (max-width: 800px) {
      .grid { grid-template-columns: 1fr; }
    }
    .panel {
      background: #252840;
      padding: 20px;
      border-radius: 8px;
      border-top: 3px solid #4dd0e1;
    }
    .panel.right { border-top-color: #ff7043; }
    .panel h2 {
      font-size: 18px;
      margin-bottom: 16px;
      color: #4dd0e1;
    }
    .panel.right h2 { color: #ff7043; }
    .control-group { margin-bottom: 16px; }
    .control-label {
      display: flex;
      justify-content: space-between;
      font-size: 13px;
      color: #b0b3c5;
      margin-bottom: 6px;
    }
    .control-value { color: #fff; font-weight: 600; }
    input[type="range"] {
      width: 100%;
      -webkit-appearance: none;
      appearance: none;
      height: 6px;
      background: #1a1d2e;
      border-radius: 3px;
      outline: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #4dd0e1;
      cursor: pointer;
    }
    .panel.right input[type="range"]::-webkit-slider-thumb { background: #ff7043; }
    input[type="range"]::-moz-range-thumb {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #4dd0e1;
      cursor: pointer;
      border: none;
    }
    .wave-buttons {
      display: flex;
      gap: 6px;
    }
    .wave-btn {
      flex: 1;
      padding: 8px;
      background: #1a1d2e;
      color: #b0b3c5;
      border: 1px solid #3a3d52;
      border-radius: 4px;
      cursor: pointer;
      font-size: 13px;
      transition: all 0.2s;
    }
    .wave-btn:hover { background: #2e3148; }
    .wave-btn.active {
      background: #4dd0e1;
      color: #1a1d2e;
      font-weight: 600;
      border-color: #4dd0e1;
    }
    .panel.right .wave-btn.active {
      background: #ff7043;
      border-color: #ff7043;
    }
    .mute-btn {
      width: 100%;
      padding: 10px;
      background: #1a1d2e;
      color: #b0b3c5;
      border: 1px solid #3a3d52;
      border-radius: 4px;
      cursor: pointer;
      font-size: 14px;
      font-weight: 600;
      margin-top: 8px;
      transition: all 0.2s;
    }
    .mute-btn:hover { background: #2e3148; }
    .mute-btn.muted { background: #f44336; color: #fff; border-color: #f44336; }
    .analysis-row {
      display: flex;
      justify-content: space-between;
      padding: 8px 0;
      border-bottom: 1px solid #3a3d52;
    }
    .analysis-row:last-child { border-bottom: none; }
    .analysis-label { color: #8a8fa3; font-size: 13px; }
    .analysis-value {
      color: #fff;
      font-weight: 600;
      font-family: 'Courier New', monospace;
    }
    .chart-panel {
      background: #252840;
      padding: 20px;
      border-radius: 8px;
      margin-bottom: 20px;
    }
    .chart-panel h2 {
      font-size: 18px;
      margin-bottom: 12px;
      color: #e8e9f3;
    }
    canvas {
      width: 100% !important;
      height: 250px !important;
      background: #1a1d2e;
      border-radius: 4px;
    }
    .footer {
      text-align: center;
      color: #5a5e75;
      font-size: 12px;
      margin-top: 20px;
    }
  </style>
</head>
<body>

  <h1>🎵 ESP32 Signal Generator</h1>
  <div class="subtitle">ESP32-ETH01 + WM8782S ADC + PCM5102A DAC</div>

  <!-- Status bar -->
  <div class="status-bar">
    <div class="status-item">
      <span class="status-label">Mreza:</span>
      <span class="status-value" id="network">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">IP:</span>
      <span class="status-value" id="ip">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">Sample rate:</span>
      <span class="status-value" id="samplerate">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">WebSocket:</span>
      <span class="status-value" id="wsstatus">povezivanje...</span>
    </div>
  </div>

  <!-- Kontrole za levi i desni kanal -->
  <div class="grid">

    <!-- LEVI KANAL -->
    <div class="panel">
      <h2>Levi kanal (L)</h2>

      <div class="control-group">
        <div class="control-label">
          <span>Oblik talasa</span>
        </div>
        <div class="wave-buttons">
          <button class="wave-btn active" data-channel="left" data-wave="0">Sinus</button>
          <button class="wave-btn" data-channel="left" data-wave="1">Kvadrat</button>
          <button class="wave-btn" data-channel="left" data-wave="2">Trougao</button>
        </div>
      </div>

      <div class="control-group">
        <div class="control-label">
          <span>Frekvencija</span>
          <span class="control-value"><span id="freq_left_v">1000</span> Hz</span>
        </div>
        <input type="range" id="freq_left" min="100" max="5000" value="1000" step="10">
      </div>

      <div class="control-group">
        <div class="control-label">
          <span>Amplituda</span>
          <span class="control-value"><span id="amp_left_v">50</span> %</span>
        </div>
        <input type="range" id="amp_left" min="0" max="100" value="50">
      </div>

      <button class="mute-btn" id="mute_left">🔊 Aktivan</button>

      <div style="margin-top: 16px; padding-top: 16px; border-top: 1px solid #3a3d52;">
        <div class="analysis-row">
          <span class="analysis-label">Detektovana frekvencija:</span>
          <span class="analysis-value" id="det_freq_left">-- Hz</span>
        </div>
        <div class="analysis-row">
          <span class="analysis-label">RMS amplituda:</span>
          <span class="analysis-value" id="det_rms_left">--</span>
        </div>
      </div>
    </div>

    <!-- DESNI KANAL -->
    <div class="panel right">
      <h2>Desni kanal (R)</h2>

      <div class="control-group">
        <div class="control-label">
          <span>Oblik talasa</span>
        </div>
        <div class="wave-buttons">
          <button class="wave-btn active" data-channel="right" data-wave="0">Sinus</button>
          <button class="wave-btn" data-channel="right" data-wave="1">Kvadrat</button>
          <button class="wave-btn" data-channel="right" data-wave="2">Trougao</button>
        </div>
      </div>

      <div class="control-group">
        <div class="control-label">
          <span>Frekvencija</span>
          <span class="control-value"><span id="freq_right_v">1000</span> Hz</span>
        </div>
        <input type="range" id="freq_right" min="100" max="5000" value="1000" step="10">
      </div>

      <div class="control-group">
        <div class="control-label">
          <span>Amplituda</span>
          <span class="control-value"><span id="amp_right_v">50</span> %</span>
        </div>
        <input type="range" id="amp_right" min="0" max="100" value="50">
      </div>

      <button class="mute-btn" id="mute_right">🔊 Aktivan</button>

      <div style="margin-top: 16px; padding-top: 16px; border-top: 1px solid #3a3d52;">
        <div class="analysis-row">
          <span class="analysis-label">Detektovana frekvencija:</span>
          <span class="analysis-value" id="det_freq_right">-- Hz</span>
        </div>
        <div class="analysis-row">
          <span class="analysis-label">RMS amplituda:</span>
          <span class="analysis-value" id="det_rms_right">--</span>
        </div>
      </div>
    </div>
  </div>

  <!-- Grafici signala -->
  <div class="chart-panel">
    <h2>Snimljeni signal (oscilogram)</h2>
    <canvas id="chart"></canvas>
  </div>

  <div class="footer">
    Marija Sekulic - RTRK - jun 2026
  </div>

  <!-- Chart.js iz CDN-a -->
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>

  <script>
    // ==================== WebSocket konekcija ====================
    const wsHost = window.location.hostname;
    const wsPort = 81;
    let ws = null;

    function connectWebSocket() {
      ws = new WebSocket('ws://' + wsHost + ':' + wsPort + '/');

      ws.onopen = () => {
        document.getElementById('wsstatus').textContent = 'povezan';
        document.getElementById('wsstatus').className = 'status-value ws-connected';
      };

      ws.onclose = () => {
        document.getElementById('wsstatus').textContent = 'iskljucen';
        document.getElementById('wsstatus').className = 'status-value ws-disconnected';
        // Reconnect posle 2 sekunde
        setTimeout(connectWebSocket, 2000);
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type === 'data') {
            updateAnalysis(data);
            updateChart(data.graph_left, data.graph_right);
          }
        } catch (e) {
          console.error('Greska parsiranja:', e);
        }
      };
    }

    function sendCommand(key, value) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(key + '=' + value);
      }
    }

    // ==================== Periodicno povlacenje statusa ====================
    function fetchStatus() {
      fetch('/status')
        .then(r => r.json())
        .then(s => {
          document.getElementById('network').textContent = s.network;
          document.getElementById('ip').textContent = s.ip;
          document.getElementById('samplerate').textContent = s.sample_rate + ' Hz';
        })
        .catch(e => console.error('Status fetch error:', e));
    }

    // ==================== Update prikaza analize ====================
    function updateAnalysis(data) {
      document.getElementById('det_freq_left').textContent  =
        data.detected_freq_left.toFixed(1) + ' Hz';
      document.getElementById('det_freq_right').textContent =
        data.detected_freq_right.toFixed(1) + ' Hz';
      document.getElementById('det_rms_left').textContent  =
        Math.round(data.rms_left);
      document.getElementById('det_rms_right').textContent =
        Math.round(data.rms_right);
    }

    // ==================== Chart.js setup ====================
    const ctx = document.getElementById('chart').getContext('2d');
    const xLabels = Array.from({length: 128}, (_, i) => i);

    const chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: xLabels,
        datasets: [
          {
            label: 'Levi kanal',
            data: new Array(128).fill(0),
            borderColor: '#4dd0e1',
            backgroundColor: 'rgba(77, 208, 225, 0.1)',
            borderWidth: 2,
            pointRadius: 0,
            tension: 0.1
          },
          {
            label: 'Desni kanal',
            data: new Array(128).fill(0),
            borderColor: '#ff7043',
            backgroundColor: 'rgba(255, 112, 67, 0.1)',
            borderWidth: 2,
            pointRadius: 0,
            tension: 0.1
          }
        ]
      },
      options: {
        animation: false,
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            labels: { color: '#e8e9f3' }
          }
        },
        scales: {
          y: {
            min: -33000,
            max: 33000,
            ticks: { color: '#8a8fa3' },
            grid: { color: '#3a3d52' }
          },
          x: {
            ticks: { color: '#8a8fa3', maxTicksLimit: 8 },
            grid: { color: '#3a3d52' }
          }
        }
      }
    });

    function updateChart(left, right) {
      chart.data.datasets[0].data = left;
      chart.data.datasets[1].data = right;
      chart.update('none');
    }

    // ==================== Event listeners za kontrole ====================
    // Wave buttons
    document.querySelectorAll('.wave-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const channel = btn.dataset.channel;
        const wave = btn.dataset.wave;
        // Toggle active state samo za ovaj kanal
        document.querySelectorAll(`.wave-btn[data-channel="${channel}"]`)
          .forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        sendCommand('wave_' + channel, wave);
      });
    });

    // Frequency sliders
    document.getElementById('freq_left').addEventListener('input', (e) => {
      document.getElementById('freq_left_v').textContent = e.target.value;
      sendCommand('freq_left', e.target.value);
    });
    document.getElementById('freq_right').addEventListener('input', (e) => {
      document.getElementById('freq_right_v').textContent = e.target.value;
      sendCommand('freq_right', e.target.value);
    });

    // Amplitude sliders
    document.getElementById('amp_left').addEventListener('input', (e) => {
      document.getElementById('amp_left_v').textContent = e.target.value;
      sendCommand('amp_left', (e.target.value / 100).toFixed(2));
    });
    document.getElementById('amp_right').addEventListener('input', (e) => {
      document.getElementById('amp_right_v').textContent = e.target.value;
      sendCommand('amp_right', (e.target.value / 100).toFixed(2));
    });

    // Mute buttons
    let muteL = false, muteR = false;
    document.getElementById('mute_left').addEventListener('click', (e) => {
      muteL = !muteL;
      e.target.textContent = muteL ? '🔇 Mute' : '🔊 Aktivan';
      e.target.classList.toggle('muted', muteL);
      sendCommand('mute_left', muteL ? '1' : '0');
    });
    document.getElementById('mute_right').addEventListener('click', (e) => {
      muteR = !muteR;
      e.target.textContent = muteR ? '🔇 Mute' : '🔊 Aktivan';
      e.target.classList.toggle('muted', muteR);
      sendCommand('mute_right', muteR ? '1' : '0');
    });

    // ==================== Pokretanje ====================
    fetchStatus();
    setInterval(fetchStatus, 5000);  // Refresh status svakih 5 sekundi
    connectWebSocket();
  </script>
</body>
</html>
)rawliteral";

#endif // WEBPAGE_H
