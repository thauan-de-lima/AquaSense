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
// WriteApi
const writeApi = influxClient.getWriteApi(process.env.INFLUX_ORG, process.env.INFLUX_BUCKET);
//QueryApi
const queryApi = influxClient.getQueryApi(process.env.INFLUX_ORG);

// Conexão MQTT
const mqttClient = mqtt.connect(`mqtts://${process.env.MQTT_HOST}:${process.env.MQTT_PORT}`, {
  username: process.env.MQTT_USER,
  password: process.env.MQTT_PASSWORD,
});

// Tempo máximo (em ms) sem receber dado para considerar o sensor offline
const TIMEOUT_OFFLINE = 5000; // 10 segundos

let dadosAquario = {
  ph: null,
  temperatura: null,
  temperaturaAmbiente: null,
  umidade: null,
  nivel: null,
  vazao: null,
  luminosidade: null,
  tds: null,
  turbidez: null,
  ultimaAtualizacao: null,
};

// Guarda o timestamp (em ms) da última leitura de CADA sensor individualmente
let ultimaLeitura = {
  ph: null,
  temperatura: null,
  temperaturaAmbiente: null,
  tds: null,
  nivel: null,
  umidade: null,
  turbidez: null,
  luminosidade: null,
  vazao: null,
};

// Descrição funcional de cada sensor (sem expor o nome do hardware)
const descricaoSensores = {
  ph: 'pH da Água',
  temperatura: 'Temperatura da Água',
  temperaturaAmbiente: 'Temperatura do Ambiente',
  tds: 'Sólidos Dissolvidos',
  nivel: 'Nível da Água',
  umidade: 'Umidade do Ar',
  turbidez: 'Turbidez da Água',
  luminosidade: 'Luminosidade',
  vazao: 'Vazão de Água',
};

mqttClient.on('connect', () => {
  console.log('Backend conectado ao broker MQTT!');
  mqttClient.subscribe('aquasense/temperatura');
  mqttClient.subscribe('aquasense/temperatura_ambiente');
  mqttClient.subscribe('aquasense/umidade');
  mqttClient.subscribe('aquasense/nivel');
  mqttClient.subscribe('aquasense/vazao');
  mqttClient.subscribe('aquasense/luminosidade');
  mqttClient.subscribe('aquasense/tds');
  mqttClient.subscribe('aquasense/turbidez')
  mqttClient.subscribe('aquasense/ph')
  console.log('Inscrito nos tópicos AquaSense');
});

mqttClient.on('message', (topic, message) => {
  const valor = parseFloat(message.toString());
  console.log(`Dado recebido — ${topic}: ${valor}`);

  const agora = new Date();
  dadosAquario.ultimaAtualizacao = agora.toISOString();

  if (topic === 'aquasense/temperatura') {
    dadosAquario.temperatura = valor;
    ultimaLeitura.temperatura = agora.getTime();
    const point = new Point('temperatura')
      .tag('sensor', 'ds18b20')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);

  } else if (topic === 'aquasense/temperatura_ambiente') {
    dadosAquario.temperaturaAmbiente = valor;
    ultimaLeitura.temperaturaAmbiente = agora.getTime();
    const point = new Point('temperatura_ambiente')
      .tag('sensor', 'dht22')
      .tag('local', 'ambiente')
      .floatField('valor', valor);
    writeApi.writePoint(point);

  } else if (topic === 'aquasense/umidade') {
    dadosAquario.umidade = valor;
    ultimaLeitura.umidade = agora.getTime();
    const point = new Point('umidade')
      .tag('sensor', 'dht22')
      .tag('local', 'ambiente')
      .floatField('valor', valor);
    writeApi.writePoint(point);

  } else if (topic === 'aquasense/nivel') {
    dadosAquario.nivel = valor;
    ultimaLeitura.nivel = agora.getTime();
    const point = new Point('nivel')
      .tag('sensor', 'hcsr04')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);
  }
    else if (topic === 'aquasense/vazao') {
    dadosAquario.vazao = valor;
    ultimaLeitura.vazao = agora.getTime();
    const point = new Point('vazao')
      .tag('sensor', 'zjs201')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);
  }
  else if (topic === 'aquasense/luminosidade') {
    dadosAquario.luminosidade = valor;
    ultimaLeitura.luminosidade = agora.getTime();
    const point = new Point('luminosidade')
      .tag('sensor', 'bh1750')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);
  }
  else if (topic === 'aquasense/tds') {
    dadosAquario.tds = valor;
    ultimaLeitura.tds = agora.getTime();
    const point = new Point('tds')
      .tag('sensor', 'tds-board-v1')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);
  }
  else if (topic === 'aquasense/turbidez') {
    dadosAquario.turbidez = valor;
    ultimaLeitura.turbidez = agora.getTime();
    const point = new Point('turbidez')
      .tag('sensor', 'turbidity-sensor-v1')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);
  }
  else if (topic === 'aquasense/ph') {
    dadosAquario.ph = valor;
    ultimaLeitura.ph = agora.getTime();
    const point = new Point('ph')
      .tag('sensor', 'ph4502c')
      .tag('local', 'aquario')
      .floatField('valor', valor);
    writeApi.writePoint(point);
  }

  writeApi.flush().catch(err => console.error('Erro ao salvar no InfluxDB:', err));
});

app.get('/api/dados', (req, res) => {
  res.json(dadosAquario);
});

// Nova rota: status online/offline de cada sensor
app.get('/api/status', (req, res) => {
  const agora = Date.now();
  const status = {};

  for (const sensor in ultimaLeitura) {
    const ultima = ultimaLeitura[sensor];
    const online = ultima !== null && (agora - ultima) < TIMEOUT_OFFLINE;

    status[sensor] = {
      descricao: descricaoSensores[sensor],
      online: online,
    };
  }

  res.json(status);
});

const INTERVALOS_VALIDOS = {
  '1h':  { range: '-1h',  every: '1m' },
  '24h': { range: '-24h', every: '5m' },
  '7d':  { range: '-7d',  every: '30m' },
};

const SENSORES_VALIDOS = [
  'temperatura', 'temperatura_ambiente', 'umidade', 'nivel',
  'vazao', 'luminosidade', 'tds', 'turbidez', 'ph'
];

app.get('/api/historico/:sensor', async (req, res) => {
  const { sensor } = req.params;
  const intervaloParam = req.query.intervalo || '24h';

  if (!SENSORES_VALIDOS.includes(sensor)) {
    return res.status(400).json({ erro: 'Sensor inválido' });
  }

  const config = INTERVALOS_VALIDOS[intervaloParam];
  if (!config) {
    return res.status(400).json({ erro: 'Intervalo inválido' });
  }

  const fluxQuery = `
    from(bucket: "${process.env.INFLUX_BUCKET}")
      |> range(start: ${config.range})
      |> filter(fn: (r) => r._measurement == "${sensor}")
      |> aggregateWindow(every: ${config.every}, fn: mean, createEmpty: false)
  `;

  try {
    const resultado = [];
    for await (const { values, tableMeta } of queryApi.iterateRows(fluxQuery)) {
      const linha = tableMeta.toObject(values);
      resultado.push({
        tempo: linha._time,
        valor: linha._value,
      });
    }
    res.json(resultado);
  } catch (erro) {
    console.error('Erro ao consultar histórico:', erro);
    res.status(500).json({ erro: 'Erro ao consultar dados' });
  }
});

app.get('/', (req, res) => {
  res.send('AquaSense Backend rodando!');
});

app.listen(PORT, () => {
  console.log(`Servidor rodando na porta ${PORT}`);
});