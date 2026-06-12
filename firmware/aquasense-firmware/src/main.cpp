
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WificlientSecure.h>
#include <PubSubClient.h>
#include "credentials.h"

#define SENSOR_PIN 4

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

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

void conectarMQTT() {
  espClient.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  Serial.print("Conectando ao broker MQTT");
  while (!mqtt.connected()){
    if (mqtt.connect("ESP32_AquaSense", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("");
      Serial.println("MQTT conectado!");
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  conectarWiFi();
  conectarMQTT();
  Serial.println("AquaSense - Sensor de Temperatura iniciado");
}

void loop() {
  if (!mqtt.connected()) {
    conectarMQTT();
  }
  mqtt.loop();

  sensors.requestTemperatures();
  float temperatura = sensors.getTempCByIndex(0);

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  String payload = String(temperatura);
  mqtt.publish("aquasense/temperatura", payload.c_str());
  Serial.println("Dado enviado ao broker MQTT!");

  if (temperatura > 30.0){
    Serial.println(" ALERTA: Temperatura ALTA!");
  } else if (temperatura < 22.0) {
    Serial.println(" ALERTA: Temperatura BAIXA!");
  } else {
    Serial.println("Temperatura NORMAL.");
  }

  delay(2000);
}