#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define SENSOR_PIN 4

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  Serial.println("AquaSense - Sensor de Temperatura iniciado");
}

void loop() {
  sensors.requestTemperatures(); //dedeqdfewdqewdqedqd
  float temperatura = sensors.getTempCByIndex(0);

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  delay(2000);
}