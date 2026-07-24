#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include "credentials.h"

#define SENSOR_PIN 4
#define DHT_PIN 5
#define DHT_TYPE DHT22
#define TRIG_PIN 6
#define ECHO_PIN 7
#define FLOW_PIN 13
#define TDS_PIN 10
#define TURBIDEZ_PIN 11
#define PH_PIN 12

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);
BH1750 luxMeter;
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

// Altura do sensor até o fundo do aquário (em cm)
// Ajuste esse valor depois de medir no seu aquário
const float ALTURA_SENSOR = 30.0;

const float VREF = 3.3;
const int ADC_RESOLUTION = 4096;


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
float lerTDS(float temperaturaAgua) {
  int leituraBruta = analogRead(TDS_PIN);
  float tensao = leituraBruta / (float)ADC_RESOLUTION * VREF;

  float coeficienteCompensacao = 1.0 + 0.02 * (temperaturaAgua - 25.0);
  float tensaoCompensada = tensao / coeficienteCompensacao;

  float tds = (133.42 * pow(tensaoCompensada, 3)
             - 255.86 * pow(tensaoCompensada, 2)
             + 857.39 * tensaoCompensada) * 0.5;

  return tds;
}

float lerTurbidez() {
  int leituraBruta = analogRead(TURBIDEZ_PIN);
  float tensao = leituraBruta / (float)ADC_RESOLUTION * VREF;

  float tensaoOriginal = tensao * 2.0;

  return tensaoOriginal;

}

float lerPH() {
  int leituraBruta = analogRead(PH_PIN);
  float tensao = leituraBruta / (float)ADC_RESOLUTION * VREF;

  float ph = -11.58 * tensao + 20.72;

  return ph;
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
  Wire.begin(8, 9);
  luxMeter.begin();
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
// BH1750 - Luminosidade
float lux = luxMeter.readLightLevel();

if (lux < 0) {
  Serial.println("Erro ao ler BH1750!");
} else {
  Serial.print("Luminosidade: ");
  Serial.print(lux);
  Serial.println(" lux");

  mqtt.publish("aquasense/luminosidade", String(lux).c_str());
}
// TDS Board - Sólidos Dissolvidos
float tds = lerTDS(tempAgua);

if (tds < 0) {
  Serial.println("Erro ao ler TDS!");
} else {
  Serial.print("TDS: ");
  Serial.print(tds);
  Serial.println(" ppm");

  mqtt.publish("aquasense/tds", String(tds).c_str());
}
// Sensor de Turbidez
float turbidezVolts = lerTurbidez();

Serial.print("Turbidez: ");
Serial.print(turbidezVolts);
Serial.println(" V");

mqtt.publish("aquasense/turbidez", String(turbidezVolts).c_str());

if (turbidezVolts < 2.5) {
  Serial.println("ALERTA: Água muito turva!");
} else {
  Serial.println("Turbidez NORMAL.");
}

// Módulo PH-4502C - pH da água
float ph = lerPH();

Serial.print("pH: ");
Serial.println(ph, 2);

mqtt.publish("aquasense/ph", String(ph).c_str());

if (ph < 6.0) {
  Serial.println("ALERTA: pH muito ácido!");
} else if (ph > 7.5) {
  Serial.println("ALERTA: pH muito alcalino!");
} else {
  Serial.println("pH NORMAL.");
}

  delay(2000);
}