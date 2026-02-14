#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4 // GPIO 4

// 28165E6E00000045

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensorAddress;

void printAddress(DeviceAddress);

void setup() {

  Serial.begin(115200);
  delay(200);
  uint32_t start = millis();
  while (!Serial || millis() - start < 5000) {
    delay(10);
  }
  //x�x�x�
  Serial.println("Scanning for DS18B20 devices...");
  sensors.begin();

  int count = sensors.getDeviceCount();
  Serial.printf("Found %d device(s)", count);

  if (count > 0 && sensors.getAddress(sensorAddress, 0)) {
    Serial.print("Sensor 0 address: ");
    printAddress(sensorAddress);
    Serial.println();
  } else {
    Serial.println("No sensor found...");
  }
  Serial.println("DS18B20 starting...");
}

//void loop() {
//}
void loop() {
  // put your main code here, to run repeatedly:

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor not found");
  } else {
    float tempF = tempC * 9.0 / 5.0 + 32.0;
    Serial.printf("Temp: %.2f C / %.2f F\n", tempC, tempF);
  }

  delay(2000);
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}