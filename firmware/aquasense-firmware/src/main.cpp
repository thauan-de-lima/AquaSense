#include "credentials.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>

#define SENSOR_PIN 4

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);

void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  conectarWiFi();
  Serial.println("AquaSense - Sensor de Temperatura iniciado");
}

void loop() {
  sensors.requestTemperatures();
  float temperatura = sensors.getTempCByIndex(0);

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  if (temperatura > 30.0){
    Serial.println(" ALERTA: Temperatura ALTA!");
  } else if (temperatura < 22.0) {
    Serial.println(" ALERTA: Temperatura BAIXA!");
  } else {
    Serial.println("Temperatura NORMAL.");
  }

  delay(2000);
}