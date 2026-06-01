// ============================================================
//  TEMPLATE JOC — Consola ESPectro (ESP32-S3)
//  Copia aquest fitxer i omple les seccions marcades amb TODO
//
//  INSTRUCCIONS:
//  1. Copia aquest fitxer al teu projecte PlatformIO
//  2. Omple les seccions marcades amb TODO
//  3. Compila i puja el .bin via Game Loader
//
//  NOTES IMPORTANTS:
//  - NO canviïs les seccions marcades com NO MODIFICAR
//  - El WiFi arrenca automàticament en segon pla (FreeRTOS)
//  - El dashboard és accessible a http://192.168.4.1 sempre
//  - Usa saveRecord(score) per guardar puntuació
//  - runGame() ha de fer return per tornar al menú
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <nvs_flash.h>
#include <nvs.h>

// ============================================================
//  PANTALLA — NO MODIFICAR
// ============================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_SPI       _bus;
    lgfx::Panel_ILI9488 _panel;
    lgfx::Light_PWM     _light;
public:
    LGFX() {
        { auto cfg = _bus.config();
          cfg.spi_host   = SPI2_HOST;
          cfg.spi_mode   = 0;
          cfg.freq_write = 40000000;
          cfg.pin_sclk   = 47;
          cfg.pin_mosi   = 38;
          cfg.pin_miso   = 48;
          cfg.pin_dc     = 2;
          _bus.config(cfg);
          _panel.setBus(&_bus); }
        { auto cfg = _panel.config();
          cfg.pin_cs   = 1;
          cfg.pin_rst  = 0;
          cfg.pin_busy = -1;
          cfg.memory_width  = 320;
          cfg.memory_height = 480;
          cfg.panel_width   = 320;
          cfg.panel_height  = 480;
          cfg.invert    = false;
          cfg.rgb_order = false;
          _panel.config(cfg); }
        { auto cfg = _light.config();
          cfg.pin_bl      = 39;
          cfg.invert      = false;
          cfg.freq        = 44100;
          cfg.pwm_channel = 0;
          _light.config(cfg);
          _panel.setLight(&_light); }
        setPanel(&_panel);
    }
};

LGFX tft;

// ============================================================
//  PINS — NO MODIFICAR
// ============================================================
#define JOY_X_PIN  5
#define JOY_Y_PIN  4
#define JOY_SW_PIN 42
#define BTN_A_PIN  40
#define BTN_B_PIN  41

#define SCREEN_W 320
#define SCREEN_H 480

// ============================================================
//  I2S AUDIO — NO MODIFICAR
// ============================================================
#define I2S_BCLK    8
#define I2S_LRCLK  16
#define I2S_DIN    18
#define SAMPLE_RATE 44100
#define I2S_PORT    I2S_NUM_0

void audioInit() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 512,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRCLK,
        .data_out_num = I2S_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
}

// To sinusoïdal — freq(Hz), durada(ms), volum(0.0-0.2)
void playTone(float freq, int durationMs, float volume = 0.1f) {
    const int bufSize = 256;
    int16_t buf[bufSize * 2];
    const int samples = SAMPLE_RATE * durationMs / 1000;
    int written = 0;
    while (written < samples) {
        int chunk = min(bufSize, samples - written);
        for (int i = 0; i < chunk; i++) {
            float t   = (float)(written + i) / SAMPLE_RATE;
            int16_t v = (int16_t)(sinf(2.0f * M_PI * freq * t) * 32767.0f * volume);
            buf[i*2] = v; buf[i*2+1] = v;
        }
        size_t bw;
        i2s_write(I2S_PORT, buf, chunk * 4, &bw, portMAX_DELAY);
        written += chunk;
    }
}

void playSilence(int durationMs) {
    int16_t buf[512] = {0};
    const int samples = SAMPLE_RATE * durationMs / 1000;
    int written = 0;
    while (written < samples) {
        int chunk = min(256, samples - written);
        size_t bw;
        i2s_write(I2S_PORT, buf, chunk * 4, &bw, portMAX_DELAY);
        written += chunk;
    }
}

// ============================================================
//  SPLASH SCREEN — NO MODIFICAR
// ============================================================
void showSplash() {
    tft.fillScreen(TFT_BLACK);
    const uint16_t TARONJA = tft.color565(255, 80, 0);
    tft.fillRect(0, 0, SCREEN_W, 6, TARONJA);
    tft.setTextSize(5);
    int y_titol = 140;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(SCREEN_W/2 - tft.textWidth("ESPectro")/2, y_titol);
    tft.print("ESP");
    tft.setTextColor(TARONJA, TFT_BLACK);
    tft.print("ectro");
    tft.drawFastHLine(40, y_titol+55, SCREEN_W-80, TARONJA);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    const char* slogan = "Consola portatil ESP32-S3";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(slogan)/2, y_titol+70);
    tft.print(slogan);
    const char* autors = "Noel Medina & Bernat Figuerola - UPC 2026";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(autors)/2, y_titol+86);
    tft.print(autors);
    tft.fillRect(0, SCREEN_H-6, SCREEN_W, 6, TARONJA);
    playTone(220.0f, 120, 0.15f); playSilence(30);
    playTone(277.2f, 120, 0.15f); playSilence(30);
    playTone(329.6f, 120, 0.15f);
    delay(500);
}

// ============================================================
//  FREERTOS — NO MODIFICAR
// ============================================================
// Sincronització:
//   audioQueue  (Queue) — efectes de so des del joc a la tasca música
//   recordMutex (Mutex) — protegeix NVS entre wifiTask i runGame()
SemaphoreHandle_t recordMutex;
volatile bool     wifiActiu = false;

// ── WiFi / Game Loader ────────────────────────────────────────
#define AP_SSID "ESPectro"
#define AP_PASS "gameloader"
#define AP_IP   "192.168.4.1"

#define MAX_HISTORY 20

// ── Clau del rècord — TODO: canvia pel nom del teu joc ───────
#define RECORD_KEY "flappy_bird"

// ============================================================
//  RECORDS NVS — NO MODIFICAR
// ============================================================
int loadRecord() {
    nvs_handle_t h;
    nvs_flash_init();
    int32_t r = 0;
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, RECORD_KEY, &r);
        nvs_close(h);
    }
    return (int)r;
}

String loadHistory() {
    nvs_handle_t h;
    nvs_flash_init();
    String hist = "[]";
    String hkey = String(RECORD_KEY) + "_h";
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        size_t len = 0;
        if (nvs_get_str(h, hkey.c_str(), nullptr, &len) == ESP_OK && len > 0) {
            char* buf = new char[len];
            nvs_get_str(h, hkey.c_str(), buf, &len);
            hist = String(buf);
            delete[] buf;
        }
        nvs_close(h);
    }
    return hist;
}

void registerGame(const char* key) {
    nvs_handle_t h;
    nvs_flash_init();
    if (nvs_open("records", NVS_READWRITE, &h) == ESP_OK) {
        char buf[256] = "";
        size_t len = sizeof(buf);
        nvs_get_str(h, "game_list", buf, &len);
        String list = String(buf);
        if (list.indexOf(key) < 0) {
            if (list.length() > 0) list += ",";
            list += key;
            nvs_set_str(h, "game_list", list.c_str());
            nvs_commit(h);
        }
        nvs_close(h);
    }
}

// Guarda sempre a l'historial; actualitza el màxim si cal
// Guarda sempre a l'historial; actualitza el màxim i el comptador
void saveRecord(int score) {
    registerGame(RECORD_KEY);
    nvs_handle_t h;
    nvs_flash_init();
    if (nvs_open("records", NVS_READWRITE, &h) == ESP_OK) {
        int32_t current = 0;
        nvs_get_i32(h, RECORD_KEY, &current);
        if (score > current)
            nvs_set_i32(h, RECORD_KEY, (int32_t)score);

        String hkey = String(RECORD_KEY) + "_h";
        String hist = "[]";
        size_t len = 0;
        if (nvs_get_str(h, hkey.c_str(), nullptr, &len) == ESP_OK && len > 0) {
            char* buf = new char[len];
            nvs_get_str(h, hkey.c_str(), buf, &len);
            hist = String(buf);
            delete[] buf;
        }
        String inner = hist.substring(1, hist.length()-1);
        String nova;
        if (inner.length() == 0) {
            nova = "[" + String(score) + "]";
        } else {
            int count = 1;
            for (int i = 0; i < (int)inner.length(); i++)
                if (inner[i] == ',') count++;
            if (count >= MAX_HISTORY) {
                int lc = inner.lastIndexOf(',');
                inner = (lc >= 0) ? inner.substring(0, lc) : "";
            }
            nova = (inner.length() > 0)
                ? "[" + String(score) + "," + inner + "]"
                : "[" + String(score) + "]";
        }
        nvs_set_str(h, hkey.c_str(), nova.c_str());

        // ── Comptador de partides jugades (clau <joc>_c) ──
        String ckey = String(RECORD_KEY) + "_c";
        int32_t total = 0;
        nvs_get_i32(h, ckey.c_str(), &total);
        nvs_set_i32(h, ckey.c_str(), total + 1);

        nvs_commit(h);
        nvs_close(h);
    }
}
String getAllRecords() {
    if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return "{}";

    nvs_handle_t h;
    nvs_flash_init();
    String json = "{";
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        char buf[256] = "";
        size_t len = sizeof(buf);
        nvs_get_str(h, "game_list", buf, &len);
        String list = String(buf);
        bool first = true;
        int start = 0;
        while (start <= (int)list.length()) {
            int comma = list.indexOf(',', start);
            String key = (comma < 0)
                ? list.substring(start)
                : list.substring(start, comma);
            if (key.length() > 0) {
                // Record maxim
                int32_t best = 0;
                nvs_get_i32(h, key.c_str(), &best);
                // Comptador total
                String ckey = key + "_c";
                int32_t total = 0;
                nvs_get_i32(h, ckey.c_str(), &total);
                // Historial
                String hkey = key + "_h";
                String hist = "[]";
                size_t hlen = 0;
                if (nvs_get_str(h, hkey.c_str(), nullptr, &hlen) == ESP_OK && hlen > 0) {
                    char* hbuf = new char[hlen];
                    nvs_get_str(h, hkey.c_str(), hbuf, &hlen);
                    hist = String(hbuf);
                    delete[] hbuf;
                }
                if (!first) json += ",";
                json += "\"" + key + "\":{\"best\":" + String(best) +
                        ",\"total\":" + String(total) +
                        ",\"history\":" + hist + "}";
                first = false;
            }
            if (comma < 0) break;
            start = comma + 1;
        }
        nvs_close(h);
    }
    json += "}";
    xSemaphoreGive(recordMutex);
    return json;
}

// ============================================================
//  DASHBOARD WEB — NO MODIFICAR
// ============================================================
WebServer server(80);

const char PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ca">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESPectro — Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;}
body{background:#0d0d0d;color:#eee;font-family:'Courier New',monospace;
     min-height:100vh;padding:1em;}
h1{color:#ff5000;text-align:center;font-size:1.8em;letter-spacing:4px;
   padding:0.5em 0;border-bottom:2px solid #ff5000;margin-bottom:1em;}
h1 span{color:#fff;}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:1em;max-width:800px;margin:0 auto;}
@media(max-width:600px){.grid{grid-template-columns:1fr;}}
.card{background:#1a1a1a;border:1px solid #333;border-radius:10px;padding:1.2em;}
.card h2{color:#ff5000;font-size:0.9em;letter-spacing:2px;margin-bottom:0.8em;}
.stat{display:flex;justify-content:space-between;align-items:center;
      padding:0.4em 0;border-bottom:1px solid #222;}
.stat:last-child{border:none;}
.stat-label{color:#888;font-size:0.85em;}
.stat-val{color:#0f0;font-weight:bold;font-size:1.1em;}
.stat-val.gold{color:#ffd700;}
.chart{margin-top:0.5em;}
.chart-title{color:#888;font-size:0.75em;margin-bottom:0.5em;}
.bars{display:flex;align-items:flex-end;gap:3px;height:80px;}
.bar-wrap{display:flex;flex-direction:column;align-items:center;flex:1;}
.bar{width:100%;background:#ff5000;border-radius:2px 2px 0 0;min-height:2px;}
.bar-val{color:#888;font-size:0.55em;margin-top:2px;}
input[type=file]{display:none;}
label.btn,button.btn{display:inline-block;padding:0.6em 1.2em;margin:0.3em 0;
  background:#ff5000;color:#fff;border:none;border-radius:6px;
  font-size:0.9em;font-family:monospace;cursor:pointer;width:100%;}
#filename{color:#ff5000;margin:0.4em 0;font-size:0.85em;min-height:1.2em;}
#progress{width:100%;background:#222;border-radius:4px;height:12px;
          margin:0.5em 0;display:none;}
#bar{height:100%;width:0;background:#ff5000;border-radius:4px;transition:width 0.3s;}
#status{min-height:1.5em;font-size:0.85em;color:#ff0;}
.ok{color:#0f0!important;} .err{color:#f44!important;}
</style>
</head>
<body>
<h1><span>ESP</span>ectro — Dashboard</h1>
<div class="grid">
  <div id="games-col">
    <div id="games-container">
      <div class="card"><span style="color:#555">Carregant...</span></div>
    </div>
  </div>
  <div class="card">
    <h2>⬆ Carregar joc</h2>
    <label class="btn" for="file">📂 Triar .bin</label>
    <input type="file" id="file" accept=".bin">
    <div id="filename">Cap arxiu seleccionat</div>
    <button class="btn" onclick="upload()">Pujar joc</button>
    <div id="progress"><div id="bar"></div></div>
    <div id="status"></div>
  </div>
</div>
<script>
function avg(arr){return arr.length?Math.round(arr.reduce((a,b)=>a+b,0)/arr.length):0;}
function renderGame(key,data){
  const hist=data.history||[];
  const best=data.best||0;
  const total=data.total!==undefined?data.total:hist.length;
  const mitjana=avg(hist);
  const darrera=hist[0]||0;
  const last10=hist.slice(0,10).reverse();
  const maxVal=Math.max(...last10,1);
  const bars=last10.length?last10.map(v=>{
    const h=Math.round((v/maxVal)*70);
    return`<div class="bar-wrap"><div class="bar" style="height:${h}px;background:${v===best&&best>0?'#ffd700':'#ff5000'}"></div><div class="bar-val">${v}</div></div>`;
  }).join(''):'<span style="color:#555;font-size:0.8em">Sense dades</span>';
  return`<div class="card" style="margin-bottom:1em">
    <h2>${key.replace(/_/g,' ').toUpperCase()}</h2>
    <div class="stat"><span class="stat-label">Record</span><span class="stat-val gold">${best} pts</span></div>
    <div class="stat"><span class="stat-label">Partides</span><span class="stat-val">${total}</span></div>
    <div class="stat"><span class="stat-label">Mitjana</span><span class="stat-val">${mitjana} pts</span></div>
    <div class="stat"><span class="stat-label">Darrera</span><span class="stat-val ${darrera===best&&best>0?'gold':''}">${darrera} pts</span></div>
    <div class="chart"><div class="chart-title">Ultimes partides</div>
    <div class="bars">${bars}</div></div>
  </div>`;
}
function load(){
  fetch('/records').then(r=>r.json()).then(data=>{
    const entries=Object.entries(data);
    const container=document.getElementById('games-container');
    if(!entries.length){container.innerHTML='<div class="card"><span style="color:#555">Cap joc registrat</span></div>';return;}
    container.innerHTML=entries.map(([k,d])=>renderGame(k,d)).join('');
  }).catch(()=>{});
}
load();
setInterval(load,10000);
const fi=document.getElementById('file');
fi.addEventListener('change',()=>{document.getElementById('filename').textContent=fi.files[0]?.name||'Cap arxiu';});
function upload(){
  const file=fi.files[0];
  const status=document.getElementById('status');
  const bar=document.getElementById('bar');
  const prog=document.getElementById('progress');
  if(!file){status.textContent='Selecciona un arxiu';return;}
  if(!file.name.endsWith('.bin')){status.textContent='Ha de ser .bin';return;}
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/update',true);
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){const pct=Math.round(e.loaded/e.total*100);prog.style.display='block';bar.style.width=pct+'%';status.textContent='Pujant... '+pct+'%';}
  };
  xhr.onload=()=>{
    if(xhr.status===200){status.textContent='✅ Instal·lat. Reiniciant...';status.className='ok';}
    else{status.textContent='❌ Error: '+xhr.responseText;status.className='err';}
  };
  xhr.onerror=()=>{status.textContent='❌ Error connexió';status.className='err';};
  const fd=new FormData();fd.append('firmware',file,file.name);xhr.send(fd);
}
</script>
</body>
</html>
)rawhtml";

void handleRoot()    { server.send_P(200, "text/html", PAGE_HTML); }
void handleRecords() { server.send(200, "application/json", getAllRecords()); }
void handleUpdate()  {
    server.send(200, "text/plain", Update.hasError() ? "FALLO" : "OK");
    delay(500); ESP.restart();
}
void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        tft.fillRect(0, 260, 320, 60, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 270);
        tft.print("Rebent firmware...");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
        static size_t total = 0;
        total += upload.currentSize;
        tft.fillRect(10, 300, constrain((int)(total/10000),0,100)*3, 10, TFT_GREEN);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            tft.fillRect(0, 260, 320, 60, TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 270); tft.print("Joc instal·lat!");
            tft.setCursor(10, 295); tft.print("Reiniciant...");
        } else { Update.printError(Serial); }
    }
}


// ============================================================
//  HANDLERS MCP — NO MODIFICAR
// ============================================================
void handleMcpTools() {
    String json = "{\"tools\":["
        "{\"name\":\"get_records\","
        "\"description\":\"Records i historial de puntuacions\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"get_status\","
        "\"description\":\"Estat de la consola: uptime i memoria\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"get_system_info\","
        "\"description\":\"Info hardware: CPU, flash, PSRAM\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
        "]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void handleMcpGetRecords() {
    String records = getAllRecords();
    String resp = "{\"content\":[{\"type\":\"text\",\"text\":" + records + "}]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
}

void handleMcpGetStatus() {
    unsigned long uptime = millis() / 1000;
    String resp = "{\"content\":[{\"type\":\"text\",\"text\":{"
                  "\"uptime_s\":" + String(uptime) + ","
                  "\"free_heap_bytes\":" + String(ESP.getFreeHeap()) + ","
                  "\"wifi_ssid\":\"ESPectro\","
                  "\"ip\":\"192.168.4.1\","
                  "\"version\":\"1.0.0\"}}]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
}

void handleMcpGetSystemInfo() {
    String resp = "{\"content\":[{\"type\":\"text\",\"text\":{"
                  "\"chip\":\"ESP32-S3\","
                  "\"cpu_freq_mhz\":" + String(ESP.getCpuFreqMHz()) + ","
                  "\"flash_size_mb\":" + String(ESP.getFlashChipSize()/1024/1024) + ","
                  "\"free_heap_bytes\":" + String(ESP.getFreeHeap()) + ","
                  "\"free_psram_bytes\":" + String(ESP.getFreePsram()) + ","
                  "\"sdk_version\":\"" + String(ESP.getSdkVersion()) + "\"}}]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
}

void handleMcpCall() {
    String tool = server.arg("tool");
    if      (tool == "get_records")     handleMcpGetRecords();
    else if (tool == "get_status")      handleMcpGetStatus();
    else if (tool == "get_system_info") handleMcpGetSystemInfo();
    else {
        String err = "{\"error\":\"Tool no trobada: " + tool + "\"}";
        server.send(404, "application/json", err);
    }
}

// ── Tasca WiFi (core 0, prioritat 1) ─────────────────────────
void wifiTask(void* param) {
    while (true) {
        if (wifiActiu) server.handleClient();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ============================================================
//  GAME LOADER — NO MODIFICAR
// ============================================================
void runGameLoader() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(tft.color565(255,80,0), TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(30, 40); tft.print("GAME LOADER");
    tft.drawFastHLine(10, 88, 300, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 108); tft.print("Xarxa WiFi:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 130); tft.print(AP_SSID);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 158); tft.print("Contrasenya:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 180); tft.print(AP_PASS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 210); tft.print("Obre al navegador:");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 232); tft.printf("http://%s", AP_IP);
    tft.drawFastHLine(10, 256, 300, TFT_DARKGREY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 268); tft.print("RECORD:");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 282);
    tft.printf("%s: %d pts", RECORD_KEY, loadRecord());
    tft.setTextColor(tft.color565(150,150,150), TFT_BLACK);
    tft.setCursor(10, 440); tft.print("Prem A per tornar al menu");

    while (true) {
        if (digitalRead(BTN_A_PIN) == LOW) {
            delay(300);
            return;
        }
        delay(20);
    }
}

// ============================================================
//  MENÚ PRINCIPAL — NO MODIFICAR (pots canviar el títol)
// ============================================================
void drawMenu(int bestScore) {
    tft.fillScreen(TFT_BLACK);

    // TODO: canvia el títol del teu joc
    tft.setTextColor(tft.color565(255,60,0), TFT_BLACK);
    tft.setTextSize(4);
    const char* linia1 = "FLAPPY";
    const char* linia2 = "BIRD";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(linia1)/2, 50);  tft.print(linia1);
    tft.setCursor(SCREEN_W/2 - tft.textWidth(linia2)/2, 100); tft.print(linia2);

    tft.drawFastHLine(40, 158, 240, tft.color565(255,60,0));

    tft.setTextColor(tft.color565(255,215,0), TFT_BLACK);
    tft.setTextSize(2);
    String best = "Record: " + String(bestScore) + " pts";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(best)/2, 175);
    tft.print(best);

    tft.drawFastHLine(40, 210, 240, tft.color565(255,60,0));

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(SCREEN_W/2 - tft.textWidth("Prem A per jugar")/2, 250);
    tft.print("Prem A per jugar");
    tft.setCursor(SCREEN_W/2 - tft.textWidth("Prem B per carregar")/2, 290);
    tft.print("Prem B per carregar");
    tft.setCursor(SCREEN_W/2 - tft.textWidth("un nou joc")/2, 312);
    tft.print("un nou joc");
    uint16_t verd = tft.color565(0, 220, 40);
    tft.setTextSize(1);
    tft.fillCircle(SCREEN_W/2 - 105, 360, 4, wifiActiu ? verd : tft.color565(80,80,80));
    tft.setTextColor(wifiActiu ? verd : tft.color565(120,120,120), TFT_BLACK);
    const char* w = wifiActiu ? "WiFi actiu - ESPectro / 192.168.4.1" : "WiFi inactiu";
    tft.setCursor(SCREEN_W/2 - 95, 356);
    tft.print(w);
}

// ============================================================
//  VARIABLES GLOBALS DEL JOC
// ============================================================

int fbBirdY, fbBirdVY;
int fbScore;
#define FB_BIRD_X 60
#define FB_BIRD_R 12
#define FB_GRAVITY 1        // acceleració per frame (abans 2 → massa nerviós)
#define FB_FLAP   -12       // impuls del salt
#define FB_MAX_FALL 11      // velocitat de caiguda màxima (evita plomades)
#define FB_PIPE_W 40
#define FB_GAP    120       // forat entre tubs (abans 100 → més indulgent)
#define FB_MAX_PIPES 3
#define FB_PIPE_SPEED 3     // velocitat de desplaçament dels tubs (px/frame)
#define FB_GROUND_H   20    // alçada del terra
// Franja (sprite) al voltant de l'ocell per dibuixar-lo sense parpelleig.
// L'ocell viu sempre a la columna x=FB_BIRD_X, així que només cal bufferar
// aquesta banda vertical (no cal un framebuffer de tota la pantalla).
#define FB_BAND_X  42       // x esquerra de la franja
#define FB_BAND_W  48       // amplada de la franja (cobreix cos + bec)
// Control del joystick (eix Y, 0-4095) amb histèresi per evitar redisparades
#define FB_JOY_HIGH   900   // cal superar aquesta deflexió per saltar
#define FB_JOY_LOW    350   // cal tornar per sota per rearmar el salt
#define FB_FLAP_COOLDOWN 130 // ms mínims entre salts
struct FbPipe { int x, gapY; };
FbPipe fbPipes[FB_MAX_PIPES];
bool fbStarted;

// ── Àudio no bloquejant (tasca FreeRTOS dedicada) ────────────
// El bucle del joc NOMÉS encua l'efecte; la tasca el reprodueix en
// segon pla. Així playTone() (que bloqueja) no encalla mai el joc.
struct SfxMsg { float freq; int dur; float vol; };
QueueHandle_t sfxQueue = nullptr;

void sfxTask(void* param) {
    SfxMsg m;
    while (true) {
        if (xQueueReceive(sfxQueue, &m, portMAX_DELAY) == pdTRUE)
            playTone(m.freq, m.dur, m.vol);
    }
}

// Encua un efecte; si la cua és plena, es descarta (mai bloqueja)
inline void sfx(float freq, int dur, float vol = 0.07f) {
    if (sfxQueue) { SfxMsg m{freq, dur, vol}; xQueueSend(sfxQueue, &m, 0); }
}


// ============================================================
//  TODO: LÒGICA DEL JOC
// ============================================================
void runGame() {
    // Colors reutilitzats
    const uint16_t SKY    = tft.color565(80, 180, 255);
    const uint16_t PIPE   = tft.color565(50, 180, 50);
    const uint16_t PIPE_D = tft.color565(30, 140, 30);
    const uint16_t GROUND = tft.color565(80, 50, 10);
    const uint16_t BEAK   = tft.color565(255, 120, 0);

    int bestScore = loadRecord();

    // ── Estat inicial ─────────────────────────────────────────
    fbBirdY  = SCREEN_H / 2;
    fbBirdVY = 0;
    fbScore  = 0;
    fbStarted = false;
    for (int i = 0; i < FB_MAX_PIPES; i++) {
        fbPipes[i].x    = SCREEN_W + i * (SCREEN_W / FB_MAX_PIPES + 20);
        fbPipes[i].gapY = 80 + random(SCREEN_H - FB_GAP - 160);
    }

    // Crea la tasca d'àudio un sol cop (core 0, com la de WiFi)
    static bool audioReady = false;
    if (!audioReady) {
        sfxQueue = xQueueCreate(8, sizeof(SfxMsg));
        xTaskCreatePinnedToCore(sfxTask, "sfx", 4096, NULL, 2, NULL, 0);
        audioReady = true;
    }

    // Sprite de la franja de l'ocell (doble buffer parcial, sense parpelleig).
    // Es crea un sol cop; si no hi ha prou memòria, useLayer=false i es
    // dibuixa l'ocell de la manera directa (amb una mica de parpelleig).
    static LGFX_Sprite birdLayer(&tft);
    static bool layerInit = false, useLayer = false;
    if (!layerInit) {
        birdLayer.setColorDepth(16);
        birdLayer.createSprite(FB_BAND_W, SCREEN_H);
        useLayer  = (birdLayer.getBuffer() != nullptr);
        layerInit = true;
    }

    // ── Calibratge del joystick (centre en repòs) ─────────────
    long sum = 0;
    for (int i = 0; i < 16; i++) { sum += analogRead(JOY_Y_PIN); delay(2); }
    int joyCenter = sum / 16;

    // Detector de "salt" amb histèresi + temps mínim entre salts:
    //   - salta quan el joystick es mou fort (eix Y) o es clica (JOY_SW)
    //   - cal recentrar-lo (per sota de FB_JOY_LOW) per rearmar
    //   - i deixar passar FB_FLAP_COOLDOWN ms → no es redispara sol
    bool flapArmed = true;
    unsigned long lastFlap = 0;
    auto readFlap = [&]() -> bool {
        int  defl    = abs(analogRead(JOY_Y_PIN) - joyCenter);
        bool clicked = (digitalRead(JOY_SW_PIN) == LOW);
        bool strong  = (defl > FB_JOY_HIGH) || clicked;
        bool weak    = (defl < FB_JOY_LOW)  && !clicked;

        if (weak) flapArmed = true;          // recentrat → rearma
        if (strong && flapArmed &&
            millis() - lastFlap > FB_FLAP_COOLDOWN) {
            flapArmed = false;
            lastFlap  = millis();
            return true;
        }
        return false;
    };

    // ── Pantalla d'espera ─────────────────────────────────────
    tft.fillScreen(SKY);
    tft.setTextColor(TFT_WHITE, SKY);
    tft.setTextSize(2);
    tft.setCursor(20, 200); tft.print("Mou el joystick");
    tft.setCursor(45, 225); tft.print("per saltar!");
    tft.setCursor(60, 260); tft.print("B = sortir");

    // Espera fins que es mogui el joystick per començar (o B per sortir)
    while (!fbStarted) {
        if (digitalRead(BTN_B_PIN) == LOW) return;   // sortir sense jugar (no compta)
        if (readFlap()) {
            fbStarted = true;
            fbBirdVY  = FB_FLAP;
            sfx(440, 30);
            tft.fillScreen(SKY);                     // neteja el text d'inici
        }
        delay(20);
    }

    int  prevBirdY = fbBirdY;
    bool gameOver  = false;
    unsigned long lastFrame = millis();

    // ── Bucle principal del joc (~40 fps) ─────────────────────
    while (true) {
        if (millis() - lastFrame < 25) { delay(1); continue; }
        lastFrame = millis();

        bool btnB = (digitalRead(BTN_B_PIN) == LOW);

        // Sortida manual: es desa la partida (compta com a jugada)
        if (btnB) {
            if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                saveRecord(fbScore);
                xSemaphoreGive(recordMutex);
            }
            return;
        }

        // Salt amb el joystick (moviment fort o clic), amb histèresi + cooldown
        if (readFlap()) { fbBirdVY = FB_FLAP; sfx(440, 30); }

        // ── Física ────────────────────────────────────────────
        fbBirdVY += FB_GRAVITY;
        if (fbBirdVY > FB_MAX_FALL) fbBirdVY = FB_MAX_FALL;   // limita la caiguda
        prevBirdY = fbBirdY;
        fbBirdY = constrain(fbBirdY + fbBirdVY, FB_BIRD_R, SCREEN_H - FB_BIRD_R);

        bool scored = false;

        // ── Dibuix directe de l'escenari (tubs + terra) ───────
        tft.startWrite();

        // Canonades
        for (int i = 0; i < FB_MAX_PIPES; i++) {
            fbPipes[i].x -= FB_PIPE_SPEED;

            // Esborra el rastre que deixa el tub pel costat dret
            // (el tub es mou cap a l'esquerra, així que la vora dreta queda enrere)
            tft.fillRect(fbPipes[i].x + FB_PIPE_W, 0,
                         FB_PIPE_SPEED + 16, SCREEN_H - FB_GROUND_H, SKY);

            // Reciclatge quan surt completament per l'esquerra
            if (fbPipes[i].x < -FB_PIPE_W) {
                fbPipes[i].x    = SCREEN_W;
                fbPipes[i].gapY = 80 + random(SCREEN_H - FB_GAP - 160);
            }

            int x    = fbPipes[i].x;
            int gapY = fbPipes[i].gapY;
            int botY = gapY + FB_GAP;

            // Tub superior + boca
            tft.fillRect(x, 0, FB_PIPE_W, gapY, PIPE);
            tft.fillRect(x - 3, gapY - 15, FB_PIPE_W + 6, 15, PIPE_D);
            // Tub inferior + boca
            tft.fillRect(x, botY, FB_PIPE_W, (SCREEN_H - FB_GROUND_H) - botY, PIPE);
            tft.fillRect(x - 3, botY, FB_PIPE_W + 6, 15, PIPE_D);

            // Puntuació: l'ocell acaba de passar el tub
            if (x + FB_PIPE_W < FB_BIRD_X && x + FB_PIPE_W >= FB_BIRD_X - FB_PIPE_SPEED)
                scored = true;

            // Col·lisió amb el tub
            if (FB_BIRD_X + FB_BIRD_R > x && FB_BIRD_X - FB_BIRD_R < x + FB_PIPE_W) {
                if (fbBirdY - FB_BIRD_R < gapY || fbBirdY + FB_BIRD_R > botY)
                    gameOver = true;
            }
        }

        // Terra
        tft.fillRect(0, SCREEN_H - FB_GROUND_H, SCREEN_W, FB_GROUND_H, GROUND);
        if (fbBirdY + FB_BIRD_R >= SCREEN_H - FB_GROUND_H) gameOver = true;

        tft.endWrite();

        if (scored) fbScore++;

        // ── Ocell ─────────────────────────────────────────────
        if (useLayer) {
            // Es redibuixa tota la franja en memòria i es bolca d'un sol cop:
            // l'ocell no "desapareix" mai perquè no s'esborra a pantalla.
            const int groundTop = SCREEN_H - FB_GROUND_H;
            birdLayer.fillScreen(SKY);

            // Trossos de tub que cauen dins de la franja
            for (int i = 0; i < FB_MAX_PIPES; i++) {
                int lx   = fbPipes[i].x - FB_BAND_X;     // x relativa a la franja
                int gapY = fbPipes[i].gapY;
                int botY = gapY + FB_GAP;
                birdLayer.fillRect(lx, 0, FB_PIPE_W, gapY, PIPE);
                birdLayer.fillRect(lx - 3, gapY - 15, FB_PIPE_W + 6, 15, PIPE_D);
                birdLayer.fillRect(lx, botY, FB_PIPE_W, groundTop - botY, PIPE);
                birdLayer.fillRect(lx - 3, botY, FB_PIPE_W + 6, 15, PIPE_D);
            }
            // Terra dins de la franja
            birdLayer.fillRect(0, groundTop, FB_BAND_W, FB_GROUND_H, GROUND);

            // Ocell (coordenades relatives a la franja)
            int bx = FB_BIRD_X - FB_BAND_X;
            birdLayer.fillCircle(bx, fbBirdY, FB_BIRD_R, TFT_YELLOW);
            birdLayer.fillCircle(bx + 6, fbBirdY - 4, 4, TFT_WHITE);
            birdLayer.fillCircle(bx + 7, fbBirdY - 3, 2, TFT_BLACK);
            birdLayer.fillTriangle(bx + FB_BIRD_R,     fbBirdY,
                                   bx + FB_BIRD_R + 8, fbBirdY - 3,
                                   bx + FB_BIRD_R + 8, fbBirdY + 3, BEAK);

            birdLayer.pushSprite(FB_BAND_X, 0);
        } else {
            // Fallback sense sprite: esborra i redibuixa directament
            tft.startWrite();
            tft.fillRect(FB_BIRD_X - FB_BIRD_R - 2, prevBirdY - FB_BIRD_R - 2,
                         (FB_BIRD_R * 2) + 14, (FB_BIRD_R * 2) + 4, SKY);
            tft.fillCircle(FB_BIRD_X, fbBirdY, FB_BIRD_R, TFT_YELLOW);
            tft.fillCircle(FB_BIRD_X + 6, fbBirdY - 4, 4, TFT_WHITE);
            tft.fillCircle(FB_BIRD_X + 7, fbBirdY - 3, 2, TFT_BLACK);
            tft.fillTriangle(FB_BIRD_X + FB_BIRD_R,     fbBirdY,
                             FB_BIRD_X + FB_BIRD_R + 8, fbBirdY - 3,
                             FB_BIRD_X + FB_BIRD_R + 8, fbBirdY + 3, BEAK);
            tft.endWrite();
        }

        // ── HUD (a sobre de tot) ──────────────────────────────
        tft.setTextColor(TFT_WHITE, SKY);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.printf("Pts:%d  Rec:%d ", fbScore, bestScore);

        // El so es reprodueix en segon pla (no bloqueja el bucle)
        if (scored) sfx(660, 40);

        // ── Fi de partida ─────────────────────────────────────
        if (gameOver) {
            sfx(180, 250, 0.10f);
            tft.setTextColor(TFT_RED, SKY); tft.setTextSize(3);
            tft.setCursor(50, 200); tft.print("GAME OVER");
            tft.setTextSize(2); tft.setTextColor(TFT_WHITE, SKY);
            tft.setCursor(60, 240); tft.printf("Punts: %d", fbScore);
            if (fbScore > bestScore) {
                tft.setTextColor(TFT_YELLOW, SKY);
                tft.setCursor(45, 275); tft.print("NOU RECORD!");
            }

            // Desa SEMPRE: historial + comptador de partides (+ record si cal).
            // saveRecord() ja actualitza el màxim només si la puntuació és més alta.
            if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                saveRecord(fbScore);
                xSemaphoreGive(recordMutex);
            }

            delay(2500);
            return;
        }
    }
}

// ============================================================
//  SETUP — NO MODIFICAR
// ============================================================
void setup() {
    Serial.begin(115200);
    pinMode(JOY_X_PIN,  INPUT);
    pinMode(JOY_Y_PIN,  INPUT);
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    pinMode(BTN_A_PIN,  INPUT_PULLUP);
    pinMode(BTN_B_PIN,  INPUT_PULLUP);

    tft.init();
    tft.setRotation(2);
    tft.setBrightness(255);

    recordMutex = xSemaphoreCreateMutex();
    audioInit();

    // Tasca música — core 0, prioritat 2
    // Descomenta si el teu joc usa música FreeRTOS:
    // xTaskCreatePinnedToCore(musicTask, "music", 4096, NULL, 2, NULL, 0);

    // Tasca WiFi — core 0, prioritat 1
    xTaskCreatePinnedToCore(wifiTask, "wifi", 4096, NULL, 1, NULL, 0);

    // Iniciar WiFi en segon pla
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.softAPConfig(
        IPAddress(192,168,4,1),
        IPAddress(192,168,4,1),
        IPAddress(255,255,255,0)
    );
    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/records", HTTP_GET,  handleRecords);
    server.on("/update",  HTTP_POST, handleUpdate, handleUpdateUpload);
    // Endpoints MCP
    server.on("/mcp/tools",      HTTP_GET, handleMcpTools);
    server.on("/mcp/tools/call", HTTP_GET, handleMcpCall);
    server.begin();
    wifiActiu = true;

    showSplash();
}

// ============================================================
//  LOOP — NO MODIFICAR
// ============================================================
void loop() {
    tft.fillScreen(TFT_BLACK);
    int best = loadRecord();
    drawMenu(best);

    while (true) {
        if (digitalRead(BTN_A_PIN) == LOW) {
            delay(50);
            runGame();
            break;
        }
        if (digitalRead(BTN_B_PIN) == LOW) {
            delay(50);
            runGameLoader();
            break;
        }
        delay(20);
    }
}
