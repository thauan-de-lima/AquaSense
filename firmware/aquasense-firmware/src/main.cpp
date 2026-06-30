#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "credentials.h"

#define SENSOR_PIN 4
#define DHT_PIN 5
#define DHT_TYPE DHT22

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);
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
  while (!mqtt.connected()) {
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
  dht.begin();
  conectarWiFi();
  conectarMQTT();
  Serial.println("AquaSense - Sensores iniciados");
}

void loop() {
  if (!mqtt.connected()) {
    conectarMQTT();
  }
  mqtt.loop();

  // DS18B20 - Temperatura da água
  sensors.requestTemperatures();
  float tempAgua = sensors.getTempCByIndex(0);

  Serial.print("Temp. água: ");
  Serial.print(tempAgua);
  Serial.println(" °C");

  mqtt.publish("aquasense/temperatura", String(tempAgua).c_str());

  if (tempAgua > 30.0) {
    Serial.println("ALERTA: Temperatura da água ALTA!");
  } else if (tempAgua < 22.0) {
    Serial.println("ALERTA: Temperatura da água BAIXA!");
  } else {
    Serial.println("Temperatura da água NORMAL.");
  }

  // DHT22 - Temperatura e umidade do ambiente
  float tempAmbiente = dht.readTemperature();
  float umidade = dht.readHumidity();

  if (isnan(tempAmbiente) || isnan(umidade)) {
    Serial.println("Erro ao ler DHT22!");
  } else {
    Serial.print("Temp. ambiente: ");
    Serial.print(tempAmbiente);
    Serial.println(" °C");

    Serial.print("Umidade: ");
    Serial.print(umidade);
    Serial.println(" %");

    mqtt.publish("aquasense/temperatura_ambiente", String(tempAmbiente).c_str());
    mqtt.publish("aquasense/umidade", String(umidade).c_str());
  }

  delay(2000);
}