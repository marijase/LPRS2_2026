#include <Arduino.h>
#include <ETH.h>              // Ethernet (LAN8720)
#include <WiFi.h>             // WiFi (AP mod)
#include <WebServer.h>        // HTTP server for web page
#include <WebSocketsServer.h> // WebSocket for real-time data on graphic
#include <driver/i2s.h>       // I²S controllers (TX for DAC, RX for ADC)
#include <math.h>             // sin(), fabs()

#include "webpage.h"          // HTML/CSS/JS web page (array charactrers)


// ----- I²S TX (ESP32 → DAC) -----
#define I2S_TX_PORT       I2S_NUM_0
#define I2S_TX_BCLK_PIN   15      // → DAC pin 2 (BCK)
#define I2S_TX_LRCK_PIN   14      // → DAC pin 4 (LCK)
#define I2S_TX_DOUT_PIN    4      // → DAC pin 3 (DIN)

// ----- I²S RX (ADC → ESP32) -----
#define I2S_RX_PORT       I2S_NUM_1
#define I2S_RX_BCLK_PIN   36      // ← ADC pin 1 (B)
#define I2S_RX_LRCK_PIN   39      // ← ADC pin 2 (LR)
#define I2S_RX_DIN_PIN    35      // ← ADC pin 3 (D)

// ----- Audio parameters -----
#define SAMPLE_RATE       48000   // 48 kHz 
#define BITS_PER_SAMPLE   16      // 16-bit signed integer per sample per channel
#define BUFFER_SAMPLES    256     // size audio buffer (per channel)
#define DMA_BUF_COUNT     8       // DMA buffer number
#define DMA_BUF_LEN       64      // size of each DMA buff

// ----- Network -----
#define WIFI_AP_SSID      "ESP32-Signal-Generator"
#define WIFI_AP_PASSWORD  "12345678"
#define ETH_TIMEOUT_MS    3000    // 3 seconds for detection Ethernet connection
#define HTTP_PORT         80
#define WEBSOCKET_PORT    81

enum WaveformType {
  WAVEFORM_SINE     = 0,
  WAVEFORM_SQUARE   = 1,
  WAVEFORM_TRIANGLE = 2
};


bool eth_connected = false;
bool wifi_ap_active = false;
String active_network = "Initializing...";
String ip_address = "0.0.0.0";

WebServer http_server(HTTP_PORT);
WebSocketsServer ws_server(WEBSOCKET_PORT);

volatile WaveformType wave_left  = WAVEFORM_SINE;
volatile WaveformType wave_right = WAVEFORM_SINE;
volatile float freq_left  = 1000.0;  // Hz
volatile float freq_right = 1000.0;  // Hz
volatile float amp_left   = 0.5;     // 0.0 - 1.0
volatile float amp_right  = 0.5;     // 0.0 - 1.0
volatile bool  mute_left  = false;
volatile bool  mute_right = false;

double phase_left  = 0.0;
double phase_right = 0.0;

volatile float detected_freq_left  = 0.0;
volatile float detected_freq_right = 0.0;
volatile float rms_left            = 0.0;
volatile float rms_right           = 0.0;

// audio buffers  (interleaved L/R)
int16_t tx_buffer[BUFFER_SAMPLES * 2];  // 2 jer je stereo
int16_t rx_buffer[BUFFER_SAMPLES * 2];

#define GRAPH_SAMPLES 128
int16_t graph_buffer_left[GRAPH_SAMPLES];
int16_t graph_buffer_right[GRAPH_SAMPLES];

// time tracking 
unsigned long last_websocket_send = 0;
const unsigned long WEBSOCKET_INTERVAL_MS = 100;  // 10× per second

//  ETHERNET — handler event 
void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] Start");
      ETH.setHostname("esp32-signal-gen");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] Kabl povezan");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("[ETH] IP adresa: ");
      Serial.println(ETH.localIP());
      eth_connected = true;
      ip_address = ETH.localIP().toString();
      active_network = "Ethernet";
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[ETH] Kabl iskljucen");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[ETH] Stop");
      eth_connected = false;
      break;
    default:
      break;
  }
}

//  WiFi AP — as fallback
void startWiFiAP() {
  Serial.println("[WiFi] Pokretanje Access Point-a...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  delay(500);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("[WiFi] AP aktivan. SSID: ");
  Serial.println(WIFI_AP_SSID);
  Serial.print("[WiFi] Lozinka: ");
  Serial.println(WIFI_AP_PASSWORD);
  Serial.print("[WiFi] IP adresa: ");
  Serial.println(apIP);
  wifi_ap_active = true;
  ip_address = apIP.toString();
  active_network = "WiFi AP";
}

//  I²S init— TX channel (for DAC, master mode)
void setupI2S_TX() {
  Serial.println("[I2S] Inicijalizacija TX (DAC) - master mod...");

  i2s_config_t tx_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = true,           // Audio PLL for excact sample rate
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t tx_pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_TX_BCLK_PIN,
    .ws_io_num  = I2S_TX_LRCK_PIN,
    .data_out_num = I2S_TX_DOUT_PIN,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_TX_PORT, &tx_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S TX] Greska install: %d\n", err);
    return;
  }
  err = i2s_set_pin(I2S_TX_PORT, &tx_pins);
  if (err != ESP_OK) {
    Serial.printf("[I2S TX] Greska set_pin: %d\n", err);
    return;
  }
  i2s_zero_dma_buffer(I2S_TX_PORT);
  Serial.println("[I2S TX] OK");
}

//  I²S init — RX channel (from ADC-a, slave mode)
//  ADC master, ESP32  slave <-   recieving BCK/LRCK from ADC
void setupI2S_RX() {
  Serial.println("[I2S] Inicijalizacija RX (ADC) - slave mod...");

  i2s_config_t rx_config = {
    .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,          // slave modue = clk from ADC
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t rx_pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_RX_BCLK_PIN,
    .ws_io_num  = I2S_RX_LRCK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_RX_DIN_PIN
  };

  esp_err_t err = i2s_driver_install(I2S_RX_PORT, &rx_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S RX] Greska install: %d\n", err);
    return;
  }
  err = i2s_set_pin(I2S_RX_PORT, &rx_pins);
  if (err != ESP_OK) {
    Serial.printf("[I2S RX] Greska set_pin: %d\n", err);
    return;
  }
  Serial.println("[I2S RX] OK");
}

//  generating single data (sample) — returns 16-bit signed int
inline int16_t generateSample(WaveformType wave, double &phase,
                              float freq, float amp, bool muted) {
  if (muted) return 0;

  // Inkrement phase — phase in rad [0, 2π)
  double phase_inc = (2.0 * M_PI * freq) / (double)SAMPLE_RATE;
  phase += phase_inc;
  if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;

  double normalized = phase / (2.0 * M_PI);  // [0, 1)
  double value = 0.0;  // [-1, 1]

  switch (wave) {
    case WAVEFORM_SINE:
      value = sin(phase);
      break;
    case WAVEFORM_SQUARE:
      value = (normalized < 0.5) ? 1.0 : -1.0;
      break;
    case WAVEFORM_TRIANGLE:
      if (normalized < 0.5) {
        value = -1.0 + 4.0 * normalized;       //  -1 -->   1
      } else {
        value = 3.0 - 4.0 * normalized;        //  1  -->  -1
      }
      break;
  }

  // Skale with amplitude and convert in 16-bit signed
  //  0.9 max  (clipping DAC)
  double scaled = value * amp * 0.9 * 32767.0;
  if (scaled >  32767.0) scaled =  32767.0;
  if (scaled < -32768.0) scaled = -32768.0;
  return (int16_t)scaled;
}

//  analyze recieved signal — freq (zero-crossing) i RMS amplitude
void analyzeSignal(int16_t *samples, int count, int channel,
                   float &out_freq, float &out_rms) {
  // RMS = sqrt(mean(samples^2))
  double sum_sq = 0.0;
  for (int i = 0; i < count; i++) {
    double v = (double)samples[i];
    sum_sq += v * v;
  }
  out_rms = sqrt(sum_sq / (double)count);

  // Frekvencija pomoću zero-crossing detekcije
  // Brojimo koliko puta signal prelazi kroz nulu (sa - na +)
  int crossings = 0;
  for (int i = 1; i < count; i++) {
    if (samples[i-1] < 0 && samples[i] >= 0) {
      crossings++;
    }
  }
  // Vreme trajanja bafera (u sekundama)
  double duration = (double)count / (double)SAMPLE_RATE;
  if (duration > 0.0) {
    out_freq = (float)((double)crossings / duration);
  } else {
    out_freq = 0.0;
  }
}

// ============================================================================
//  POPUNJAVANJE TX bafera sa novim uzorcima
// ============================================================================
void fillTxBuffer() {
  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    int16_t sL = generateSample(wave_left,  phase_left,  freq_left,  amp_left,  mute_left);
    int16_t sR = generateSample(wave_right, phase_right, freq_right, amp_right, mute_right);
    // Interleaved format: L, R, L, R, ...
    tx_buffer[i*2 + 0] = sL;
    tx_buffer[i*2 + 1] = sR;
  }
}

// ============================================================================
//  WEB SERVER handlers
// ============================================================================
void handleRoot() {
  // Serviraj glavnu HTML stranicu iz webpage.h
  http_server.send_P(200, "text/html", WEBPAGE_HTML);
}

void handleStatus() {
  // Vrati JSON sa trenutnim stanjem
  String json = "{";
  json += "\"network\":\""  + active_network + "\",";
  json += "\"ip\":\""        + ip_address    + "\",";
  json += "\"freq_left\":"   + String(freq_left, 1) + ",";
  json += "\"freq_right\":"  + String(freq_right, 1) + ",";
  json += "\"amp_left\":"    + String(amp_left, 2) + ",";
  json += "\"amp_right\":"   + String(amp_right, 2) + ",";
  json += "\"wave_left\":"   + String((int)wave_left) + ",";
  json += "\"wave_right\":"  + String((int)wave_right) + ",";
  json += "\"mute_left\":"   + String(mute_left  ? "true" : "false") + ",";
  json += "\"mute_right\":"  + String(mute_right ? "true" : "false") + ",";
  json += "\"sample_rate\":" + String(SAMPLE_RATE);
  json += "}";
  http_server.send(200, "application/json", json);
}

void handleNotFound() {
  http_server.send(404, "text/plain", "Not Found");
}

// ============================================================================
//  WebSocket handler — prima komande sa web stranice, šalje rezultate analize
// ============================================================================
void onWebSocketEvent(uint8_t client_num, WStype_t type,
                     uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Klijent %u povezan\n", client_num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Klijent %u iskljucen\n", client_num);
      break;
    case WStype_TEXT: {
      // Format komande: "KEY=VALUE"
      String msg = String((char*)payload).substring(0, length);
      int eq = msg.indexOf('=');
      if (eq < 0) break;
      String key = msg.substring(0, eq);
      String val = msg.substring(eq + 1);

      if      (key == "wave_left")   wave_left   = (WaveformType)val.toInt();
      else if (key == "wave_right")  wave_right  = (WaveformType)val.toInt();
      else if (key == "freq_left")   freq_left   = val.toFloat();
      else if (key == "freq_right")  freq_right  = val.toFloat();
      else if (key == "amp_left")    amp_left    = val.toFloat();
      else if (key == "amp_right")   amp_right   = val.toFloat();
      else if (key == "mute_left")   mute_left   = (val == "1" || val == "true");
      else if (key == "mute_right")  mute_right  = (val == "1" || val == "true");

      Serial.printf("[WS] %s = %s\n", key.c_str(), val.c_str());
      break;
    }
    default:
      break;
  }
}

// ============================================================================
//  Slanje podataka analize i grafika preko WebSocket-a
// ============================================================================
void sendDataToClients() {
  // Formatiraj JSON sa svim podacima
  String json = "{\"type\":\"data\",";
  json += "\"network\":\"" + active_network + "\",";
  json += "\"detected_freq_left\":"  + String(detected_freq_left, 1) + ",";
  json += "\"detected_freq_right\":" + String(detected_freq_right, 1) + ",";
  json += "\"rms_left\":"  + String(rms_left, 1) + ",";
  json += "\"rms_right\":" + String(rms_right, 1) + ",";

  // Niz uzoraka za grafik (downsample-ovan)
  json += "\"graph_left\":[";
  for (int i = 0; i < GRAPH_SAMPLES; i++) {
    json += String(graph_buffer_left[i]);
    if (i < GRAPH_SAMPLES - 1) json += ",";
  }
  json += "],\"graph_right\":[";
  for (int i = 0; i < GRAPH_SAMPLES; i++) {
    json += String(graph_buffer_right[i]);
    if (i < GRAPH_SAMPLES - 1) json += ",";
  }
  json += "]}";

  ws_server.broadcastTXT(json);
}

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("================================================");
  Serial.println("  PROJEKAT F - Signal Generator + Monitor");
  Serial.println("  ESP32-ETH01 + WM8782S ADC + PCM5102A DAC");
  Serial.println("================================================");

  // ----- 1. Pokušaj Ethernet konekcije -----
  Serial.println("\n[INIT] Pokusavam Ethernet...");
  WiFi.onEvent(onEthEvent);
  ETH.begin();

  unsigned long start = millis();
  while (!eth_connected && (millis() - start < ETH_TIMEOUT_MS)) {
    delay(100);
  }

  // ----- 2. Ako Ethernet nije uspeo, pokreni WiFi AP -----
  if (!eth_connected) {
    Serial.println("[INIT] Ethernet nije aktivan posle 3s -> startujem WiFi AP");
    startWiFiAP();
  } else {
    Serial.println("[INIT] Ethernet aktivan");
  }

  // ----- 3. Inicijalizuj I²S kanale -----
  setupI2S_TX();
  setupI2S_RX();

  // ----- 4. Inicijalizuj HTTP server -----
  http_server.on("/", handleRoot);
  http_server.on("/status", handleStatus);
  http_server.onNotFound(handleNotFound);
  http_server.begin();
  Serial.printf("[HTTP] Server pokrenut na portu %d\n", HTTP_PORT);

  // ----- 5. Inicijalizuj WebSocket server -----
  ws_server.begin();
  ws_server.onEvent(onWebSocketEvent);
  Serial.printf("[WS] Server pokrenut na portu %d\n", WEBSOCKET_PORT);

  Serial.println("\n================================================");
  Serial.print("  Otvori u pretrazivacu: http://");
  Serial.println(ip_address);
  Serial.println("================================================\n");
}

// ============================================================================
//  LOOP — glavna petlja
// ============================================================================
void loop() {
  // ----- 1. Obradi HTTP i WebSocket -----
  http_server.handleClient();
  ws_server.loop();

  // ----- 2. Generiši i pošalji nove TX uzorke ka DAC-u -----
  fillTxBuffer();
  size_t bytes_written = 0;
  i2s_write(I2S_TX_PORT, tx_buffer, sizeof(tx_buffer), &bytes_written,
            10 / portTICK_PERIOD_MS);

  // ----- 3. Primi nove RX uzorke od ADC-a -----
  size_t bytes_read = 0;
  i2s_read(I2S_RX_PORT, rx_buffer, sizeof(rx_buffer), &bytes_read,
           10 / portTICK_PERIOD_MS);

  // ----- 4. Razdvoj L/R kanale za analizu -----
  int16_t samples_left[BUFFER_SAMPLES];
  int16_t samples_right[BUFFER_SAMPLES];
  for (int i = 0; i < BUFFER_SAMPLES; i++) {
    samples_left[i]  = rx_buffer[i*2 + 0];
    samples_right[i] = rx_buffer[i*2 + 1];
  }

  // ----- 5. Analiziraj signale -----
  float f_l, f_r, rms_l, rms_r;
  analyzeSignal(samples_left,  BUFFER_SAMPLES, 0, f_l, rms_l);
  analyzeSignal(samples_right, BUFFER_SAMPLES, 1, f_r, rms_r);
  detected_freq_left  = f_l;
  detected_freq_right = f_r;
  rms_left  = rms_l;
  rms_right = rms_r;

  // ----- 6. Pripremi grafik buffer (downsample) -----
  int step = BUFFER_SAMPLES / GRAPH_SAMPLES;
  if (step < 1) step = 1;
  for (int i = 0; i < GRAPH_SAMPLES; i++) {
    int src = i * step;
    if (src >= BUFFER_SAMPLES) src = BUFFER_SAMPLES - 1;
    graph_buffer_left[i]  = samples_left[src];
    graph_buffer_right[i] = samples_right[src];
  }

  // ----- 7. Pošalji podatke na web stranicu (10×/sekund) -----
  unsigned long now = millis();
  if (now - last_websocket_send >= WEBSOCKET_INTERVAL_MS) {
    last_websocket_send = now;
    sendDataToClients();
  }
}
