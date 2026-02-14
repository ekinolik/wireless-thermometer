#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // GPIO 4

const char* HOSTNAME = "esp32-solar";

// 28165E6E00000045

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress solarPanelSensor = {
  0x28, 0x16, 0x5E, 0x6E, 0x00, 0x00, 0x00, 0x45
};
bool useAddress = true;

const char* sensorName = "solar_panel";

WebServer server(80);

float readTempC() {
  sensors.requestTemperatures();
  float t = useAddress ? sensors.getTempC(solarPanelSensor) : sensors.getTempCByIndex(0);

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

  s += "<hr><div class='muted'>IP: ";
  s += WiFi.localIP().toString();
  s += "<br>mDNS: http://";
  s += HOSTNAME;
  s += ".local/</div></div></body></html>";
  return s;
}

void handleRoot() {
  float tC = readTempC();
  server.send(200, "text/html", htmlPage(tC));
}

void handleJson() {
  float tC = readTempC();
  String out = "{";
  out += "\"hostname\":\"" + String(HOSTNAME) + "\",";
  out += "\"IP\":" + WiFi.localIP().toString() + "\",";
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

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to wifi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 20000) { // 20s timeout
      Serial.println("Wifi connection timeout.  Rebooting...");
      ESP.restart();
    }
  }

  Serial.println("\nWifi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP().toString());

  if (MDNS.begin(HOSTNAME)) {
    Serial.printf("mDNS has started: http://%s.local/\n", HOSTNAME);
  } else {
    Serial.printf("mDNS failed (IP still available)");
  }
}

void setup() {

  Serial.begin(115200);
  delay(300);
  uint32_t start = millis();
  while (!Serial || millis() - start < 5000) {
    delay(10);
  }

  sensors.begin();
  connectWifi();

  server.on("/", handleRoot);
  server.on("/json", handleJson);
  server.begin();
  Serial.println("HTTP Server started (port 80)");
}

void loop() {
  server.handleClient();
}