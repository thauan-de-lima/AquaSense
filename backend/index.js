require('dotenv').config();
const mqtt = require('mqtt');
const express = require('express');
const cors = require('cors');
const { InfluxDB, Point } = require('@influxdata/influxdb-client');

const app = express();
app.use(cors());

const PORT = process.env.PORT || 3000;

// Conexão InfluxDB
const influxClient = new InfluxDB({
  url: process.env.INFLUX_URL,
  token: process.env.INFLUX_TOKEN,
});
const writeApi = influxClient.getWriteApi(process.env.INFLUX_ORG, process.env.INFLUX_BUCKET);

// Conexão MQTT
const mqttClient = mqtt.connect(`mqtts://${process.env.MQTT_HOST}:${process.env.MQTT_PORT}`, {
  username: process.env.MQTT_USER,
  password: process.env.MQTT_PASSWORD,
});

let dadosAquario = {
  temperatura: null,
  ultimaAtualizacao: null,
};

mqttClient.on('connect', () => {
  console.log('Backend conectado ao broker MQTT!');
  mqttClient.subscribe('aquasense/temperatura');
  console.log('Inscrito no topic: aquasense/temperatura');
});

mqttClient.on('message', (topic, message) => {
  const valor = parseFloat(message.toString());
  console.log(`Dado recebido — ${topic}: ${valor}`);

  if (topic === 'aquasense/temperatura') {
    dadosAquario.temperatura = valor;
    dadosAquario.ultimaAtualizacao = new Date().toISOString();

    // Salvar no InfluxDB
    const point = new Point('temperatura')
      .tag('sensor', 'ds18b20')
      .tag('local', 'aquario')
      .floatField('valor', valor);

    writeApi.writePoint(point);
    writeApi.flush().catch(err => console.error('Erro ao salvar no InfluxDB:', err));
  }
});

app.get('/api/dados', (req, res) => {
  res.json(dadosAquario);
});

app.get('/', (req, res) => {
  res.send('AquaSense Backend rodando!');
});

app.listen(PORT, () => {
  console.log(`Servidor rodando na porta ${PORT}`);
});