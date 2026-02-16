// Solar Sensor ID 28165E6E00000045

#include "wifiportal.h"
#include "bootmode.h"

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // GPIO 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress solarPanelSensor = {
  0x28, 0x16, 0x5E, 0x6E, 0x00, 0x00, 0x00, 0x45
};
bool useAddress = true;

const char* sensorName = "solar_panel";

float readTempC() {
  Serial.println("Reading temp...");
  sensors.requestTemperatures();
  float t = useAddress ? sensors.getTempC(solarPanelSensor) : sensors.getTempCByIndex(0);
  Serial.printf("Temp: %f\n", t);

  // DS18B20 "85C" is a common bogus value at startup / error states
  if (t == DEVICE_DISCONNECTED_C || t > 84.9f) return NAN;
  return t;
}

String  appHomeHtml(bool wifiConnected, const String& ipStr, float tempC) {
  Serial.println("Returning app home...");
  String s;
  s.reserve(1000);
  s += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<meta http-equiv='refresh' content='5'>";
  s += "<title>ESP32 Solar Temp</title>";
  s += "<style>body{font-family:system-ui;margin:24px} .card{max-width:420px;padding:18px;border:1px solid #ddd;border-radius:14px}";
  s += ".big{font-size:42px;font-weight:700;margin:10px 0} .muted{color:#666}</style></head><body>";
  s += "<div class='card'><div class='muted'>ESP32 Wireless Thermometer</div>";

  s += "<p><b>Wi-Fi:</b> ";
  s += wifiConnected ? "connected" : "disconnected (setup mode)";
  s += "<br><b>IP:</b> ";
  s += ipStr;
  s += "</p>";

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

  s += "<hr><p class='muted'><a href='/wifi'>Configure Wi-Fi</a> | <a href='/json'>/json</a></p>";
  s += "</div></div></body></html>";

  return s;
}

int main() {
  return 0;
}

BootMode bootMode;
WifiPortal::Config portalCfg;
WifiPortal portal(portalCfg);

void setup() {
  Serial.begin(115200);
  delay(300);

  uint32_t start = millis();
  while (!Serial || millis() - start < 5000) {
    delay(10);
  }

  Serial.println("Starting sensors...");
  sensors.begin();

  BootMode::Action action = bootMode.begin();
  if (action == BootMode::Action::FactoryReset) {
    portal.factoryResetWifi();
    bootMode.resetWindow();
    delay(300);
    ESP.restart();
  }

  // Start wifi if possible (STA if possible, else AP).
  WifiPortal::Mode mode = portal.begin();

  // Register your app routes on the same server
  WebServer& web = portal.web();

  web.on("/", HTTP_GET, [&]() {
    float tC = readTempC();
    bool wifiConnected = portal.isStaMode();
    String ip = wifiConnected ? portal.staIP().toString() : portal.apIP().toString();
    web.sendHeader("Connection", "close");
    web.send(200, "text/html", appHomeHtml(wifiConnected, ip, tC));
  });

  web.on("/json", HTTP_GET, [&]() {
    float tC = readTempC();
    bool wifiConnected = (mode == WifiPortal::Mode::STA);
    String ip = wifiConnected ? portal.staIP().toString() : portal.apIP().toString();

    String out = "{";
    out += "\"mode\":\"" + String(wifiConnected ? "sta" : "ap") + "\",";
    out += "\"ip\":\"" + ip + "\",";
    if (isnan(tC)) {
      out += "\"ok\":false";
    } else {
      float f = tC * 9.0f/5.0f + 32.0f;
      out += "\"ok\":true,";
      out += "\"temp_c\":" + String(tC,2) + ",";
      out += "\"temp_f\":" + String(f,2);
    }
    out += "}";
    web.sendHeader("Connection", "close");
    web.send(200, "application/json", out);
  });

  Serial.println(mode == WifiPortal::Mode::STA ? "App ready on STA/IP hostname" : "App ready on AP setup mode (172.16.0.1)");
}

void loop() {
  portal.loop();
  bootMode.loop();
}