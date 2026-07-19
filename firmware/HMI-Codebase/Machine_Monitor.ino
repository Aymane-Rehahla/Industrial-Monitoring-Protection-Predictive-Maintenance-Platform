/* ============================================================
   Machine_Monitor.ino — Updated for Detail Screen
   Sends: voltage_l1, current_l1, vibe_x, vibe_y, vibe_z,
          uptime, connected_models
   ============================================================ */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

/* ── WiFi ── */
const char* SSID     = "Dlink1200";
const char* PASSWORD = "123123123";

/* ── Server & WebSocket ── */
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

/* ── Sensor simulation state ── */
/* Replace these with real ADC reads when sensors are wired */
float voltage_l1 = 382.0;
float current_l1 =  18.3;
float vibe_x     =   0.12;
float vibe_y     =   0.09;
float vibe_z     =   0.07;

/* ── Runtime tracking ── */
unsigned long uptimeSec = 0;
unsigned long lastSecond = 0;

/* ============================================================
   Simulate sensor fluctuation
   Replace this function body with real ADC reads later
   ============================================================ */
void readSensors() {
  /* ── Voltage: ±1V random walk ── */
  voltage_l1 += (random(-10, 11) / 10.0);
  voltage_l1  = constrain(voltage_l1, 378.0, 386.0);

  /* ── Current: ±0.5A random walk ── */
  current_l1 += (random(-5, 6) / 10.0);
  current_l1  = constrain(current_l1, 15.0, 25.0);

  /* ── Vibration: ±0.02g ── */
  vibe_x += (random(-2, 3) / 100.0);
  vibe_x  = constrain(vibe_x, 0.0, 1.2);

  vibe_y += (random(-2, 3) / 100.0);
  vibe_y  = constrain(vibe_y, 0.0, 1.0);

  vibe_z += (random(-1, 2) / 100.0);
  vibe_z  = constrain(vibe_z, 0.0, 0.8);
}

/* ============================================================
   Build JSON payload
   ============================================================ */
String buildJSON() {
  JsonDocument doc;

  doc["voltage_l1"]        = round(voltage_l1 * 10) / 10.0;
  doc["current_l1"]        = round(current_l1 * 10) / 10.0;
  doc["vibe_x"]            = round(vibe_x * 1000) / 1000.0;
  doc["vibe_y"]            = round(vibe_y * 1000) / 1000.0;
  doc["vibe_z"]            = round(vibe_z * 1000) / 1000.0;
  doc["uptime"]            = uptimeSec;
  doc["connected_models"]  = 1;   /* Update when multi-machine */

  String out;
  serializeJson(doc, out);
  return out;
}

/* ============================================================
   WebSocket events
   ============================================================ */
void onWsEvent(AsyncWebSocket*       server,
               AsyncWebSocketClient* client,
               AwsEventType          type,
               void*                 arg,
               uint8_t*              data,
               size_t                len)
{
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] iPad connected — ID %u\n", client->id());

    /* Send first packet immediately on connect */
    client->text(buildJSON());
  }

  if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client %u disconnected\n", client->id());
  }

  if (type == WS_EVT_ERROR) {
    Serial.printf("[WS] Error on client %u\n", client->id());
  }
}

/* ============================================================
   SETUP
   ============================================================ */
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[BOOT] Machine Monitor HMI starting...");

  /* ── LittleFS ── */
  if (!LittleFS.begin()) {
    Serial.println("[ERROR] LittleFS mount failed!");
    return;
  }
  Serial.println("[OK] LittleFS mounted");

  /* ── WiFi ── */
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("[WiFi] Connecting");

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart > 15000) {
      Serial.println("\n[ERROR] WiFi timeout!");
      break;
    }
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("[HMI] Open this IP on your iPad");
  }

  /* ── Static file server ── */
  server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

  /* ── WebSocket ── */
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  /* ── 404 handler ── */
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[OK] HTTP + WebSocket server started");
}

/* ============================================================
   LOOP — 5Hz broadcast
   ============================================================ */
void loop() {

  /* ── Uptime counter ── */
  if (millis() - lastSecond >= 1000) {
    lastSecond = millis();
    uptimeSec++;
  }

  /* ── 5Hz sensor broadcast ── */
  static unsigned long lastBroadcast = 0;
  if (millis() - lastBroadcast >= 200) {
    lastBroadcast = millis();

    readSensors();

    /* Only broadcast if clients are connected */
    if (ws.count() > 0) {
      ws.textAll(buildJSON());
    }
  }

  /* ── Cleanup dead WebSocket clients ── */
  ws.cleanupClients();
}
