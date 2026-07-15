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
#define TRIG_PIN 6
#define ECHO_PIN 7
#define FLOW_PIN 13

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

// Altura do sensor até o fundo do aquário (em cm)
// Ajuste esse valor depois de medir no seu aquário
const float ALTURA_SENSOR = 30.0;

volatile unsigned long contadorPulsos = 0;
unsigned long ultimoTempoFluxo = 0;
float vazaoLmin = 0;

float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duracao == 0) {
    return -1;
  }

  float distancia = (duracao * 0.0343) / 2.0;
  return distancia;
}

void IRAM_ATTR contarPulso() {
  contadorPulsos++;
}

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
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), contarPulso, RISING);
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

  // HC-SR04 - Nível de água
  float distancia = medirDistancia();

  if (distancia < 0) {
    Serial.println("Erro ao ler HC-SR04!");
  } else {
    float nivelAgua = ALTURA_SENSOR - distancia;

    Serial.print("Distância: ");
    Serial.print(distancia);
    Serial.println(" cm");

    Serial.print("Nível da água: ");
    Serial.print(nivelAgua);
    Serial.println(" cm");

    mqtt.publish("aquasense/nivel", String(nivelAgua).c_str());

    if (nivelAgua < 10.0) {
      Serial.println("ALERTA: Nível de água BAIXO!");
    } else {
      Serial.println("Nível de água NORMAL.");
    }
  }

// YF-S201 / ZJ-S201 - Vazão de água
unsigned long agora = millis();

if (agora - ultimoTempoFluxo >= 1000) {
  noInterrupts();
  unsigned long pulsos = contadorPulsos;
  contadorPulsos = 0;
  interrupts();

  float segundosPassados = (agora - ultimoTempoFluxo) / 1000.0;
  float frequencia = pulsos / segundosPassados;
  vazaoLmin = frequencia / 7.5;

  ultimoTempoFluxo = agora;

  Serial.print("Vazão: ");
  Serial.print(vazaoLmin);
  Serial.println(" L/min");

  mqtt.publish("aquasense/vazao", String(vazaoLmin).c_str());
}

  delay(2000);
}