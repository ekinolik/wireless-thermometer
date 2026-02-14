#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // GPIO 4

// 28165E6E00000045

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress solarPanelSensor = {
  0x28, 0x16, 0x5E, 0x6E, 0x00, 0x00, 0x00, 0x45
};

const char* sensorName = "solar_panel";

void printAddress(DeviceAddress);

void setup() {

  Serial.begin(115200);
  delay(200);
  uint32_t start = millis();
  while (!Serial || millis() - start < 5000) {
    delay(10);
  }

  sensors.begin();

  Serial.println("DS18B20 starting...");
}

//void loop() {
//}
void loop() {
  // put your main code here, to run repeatedly:

  sensors.requestTemperatures();
  float tempC = sensors.getTempC(solarPanelSensor);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Solar panel sensor disconnected.");
  } else {
    float tempF = tempC * 9.0 / 5.0 + 32.0;
    Serial.printf("[%s]: %.2f C / %.2f F\n", sensorName, tempC, tempF);
  }

  delay(2000);
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}