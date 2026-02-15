// Solar Sensor ID 28165E6E00000045

#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // GPIO 4

const char* HOSTNAME = "esp32-solar";

// Fallback AP (if WiFi is down too long)
static const bool ENABLE_FALLBACK_AP = true;

static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000; // initial connect attemp
static const uint32_t WIFI_HEALTH_CHECK_MS    = 5000;  // how often we check the link
static const uint32_t WIFI_DOWN_BEFORE_AP_MS  = 60000; // how long before AP mode

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress solarPanelSensor = {
  0x28, 0x16, 0x5E, 0x6E, 0x00, 0x00, 0x00, 0x45
};
bool useAddress = true;

const char* sensorName = "solar_panel";

WebServer server(80);

static uint32_t lastWifiCheckMs = 0;
static uint32_t wifiDownSinceMs = 0;
static bool mdnsRunning = false;

float readTempC() {
  sensors.requestTemperatures();
  float t = useAddress ? sensors.getTempC(solarPanelSensor) : sensors.getTempCByIndex(0);

  // DS18B20 "85C" is a common bogus value at startup / error states
  if (t == DEVICE_DISCONNECTED_C || t > 84.9f) return NAN;
  return t;
}

String  htmlPage(float tempC) {
  String s;
  s.reserve(800);
  s += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<meta http-equiv='refresh' content='5'>";
  s += "<title>ESP32 Solar Temp</title>";
  s += "<style>body{font-family:system-ui;margin:24px} .card{max-width:420px;padding:18px;border:1px solid #ddd;border-radius:14px}";
  s += ".big{font-size:42px;font-weight:700;margin:10px 0} .muted{color:#666}</style></head><body>";
  s += "<div class='card'><div class='muted'>ESP32 Wireless Thermometer</div>";

  if (isnan(tempC)) {
    s += "<div class='big'>Sensor error</div>";
  } else {
    float tempF = tempC * 9.0f / 5.0f + 32.0f;
    s += "<div class='big'>";
    s += String(tempF, 2);
    s += " &deg;F</div>";
    s += "<div class='muted'>";
    s += String(tempC, 2);
    s += " &deg;C</div>";
  }

  s += "<hr><div class='muted'>";
  if (WiFi.status() == WL_CONNECTED) {
    s += "Wi-Fi: connected<br />IP: ";
    s += WiFi.localIP().toString();
    s += "<br>mDNS: http://";
    s += HOSTNAME;
    s += ".local/";
  } else {
    s += "Wi-Fi: disconnected";
    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
      s += "<br />AP SSID: ";
      s += AP_SSID;
      s += "<br />AP IP: ";
      s += WiFi.localIP().toString();
    }
  }
  s += "</div></div></body></html>";

  return s;
}

void handleRoot() {
  float tC = readTempC();
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", htmlPage(tC));
}

void handleJson() {
  float tC = readTempC();
  String out = "{";
  out += "\"hostname\":\"" + String(HOSTNAME) + "\",";
  out += "\"wifi_connected\":" + String((WiFi.status() == WL_CONNECTED) ? "true" : "false") + ",";
  out += "\"IP\":" + WiFi.localIP().toString() + "\",";
  out += "\"AP_IP\":\"" + WiFi.softAPIP().toString() + "\",";
  if (isnan(tC)) {
    out += "\"ok\":\"false\"";
  } else {
    float tF = tC * 9.0f / 5.0f + 32.0f;
    out += "\"ok\":true,";
    out += "\"temp_c\":" + String(tC, 2) + ",";
    out += "\"temp_f\":" + String(tF, 2);
  }

  out += "}";
  server.send(200, "application/json", out);
}

void startMdnsIfNeeded() {
  if (mdnsRunning) return;
  if (WiFi.status() != WL_CONNECTED) return;

  // Starting mDNS repeatedly can be flaky; only start once per successful connect.
  if (MDNS.begin(HOSTNAME)) {
    mdnsRunning = true;
    Serial.printf("mDNS: http://%s.local/\n", HOSTNAME);
  } else {
    Serial.printf("mDNS failed (IP still works).");
  }
}
bool connectWifiOnce(uint32_t timeoutMs, bool keepAp) {
  wifi_mode_t mode = WiFi.getMode();
  if (keepAp && mode == WIFI_AP || mode == WIFI_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_STA);
  }

  WiFi.setHostname(HOSTNAME);

  // These help some routers behave better after drops.
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.printf("Connecting to WiFi SSID: %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wifi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP().toString());
    mdnsRunning = false;
    startMdnsIfNeeded();

    return true;
  }

  Serial.println("WiFi connect attempt failed!");

  return false;
}

void ensureFallbackAp() {
  if (!ENABLE_FALLBACK_AP) return;

  // If we're already in AP or AP+STA, no need to restart it.
  wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return;

  Serial.println("Starting fallback AP mode...");
  WiFi.mode(WIFI_AP_STA); // keep trying STA while AP is available.

  IPAddress apIP(172,16,0,1);
  IPAddress netM(255,255,255,0);
  bool cfgOk = WiFi.softAPConfig(apIP, apIP, netM);
  Serial.printf("softAPConfig: %s\n", cfgOk ? "ok" : "FAILED");

  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  if (ok) {
    Serial.printf("AP started. AP IP: %s\n", WiFi.softAPIP().toString());
  } else {
    Serial.println("AP start failed.");
  }
}

void wifiHealthTick() {
  uint32_t now = millis();
  if (now - lastWifiCheckMs < WIFI_HEALTH_CHECK_MS) return;
  lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    // We're good.
    wifiDownSinceMs = 0;
    startMdnsIfNeeded();
    return;
  }

  // WiFi is down.
  if (wifiDownSinceMs == 0) wifiDownSinceMs = now;

  Serial.println("WiFi down. Attempting reconnect...");
  connectWifiOnce(5000, true); // short attempt, don't block too long

  // If it's been down long enough, start AP fallback
  if ((now - wifiDownSinceMs) > WIFI_DOWN_BEFORE_AP_MS) {
    ensureFallbackAp();
  }
}

void setup() {

  Serial.begin(115200);
  delay(300);
  uint32_t start = millis();
  while (!Serial || millis() - start < 5000) {
    delay(10);
  }

  Serial.println("Starting sensors...");
  sensors.begin();
  //connectWifi();

  // Initial wifi connect.
  bool ok = connectWifiOnce(WIFI_CONNECT_TIMEOUT_MS, false);
  if (!ok && ENABLE_FALLBACK_AP) {
    ensureFallbackAp();
  }

  server.on("/", handleRoot);
  server.on("/json", handleJson);
  server.begin();
  Serial.println("HTTP Server started (port 80)");
}

void loop() {
  server.handleClient();
  //wifiHealthTick();
}