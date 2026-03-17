import { Kafka } from "kafkajs";
import { WebSocketServer, WebSocket } from "ws";

const brokers = process.env.KAFKA_BROKERS?.split(",") ?? [];
const topic = process.env.KAFKA_TOPIC ?? "sensortag-imu";

const kafka = new Kafka({
  clientId: "sensortag-web",
  brokers,
  ssl: true,
  sasl: {
    mechanism: "plain",
    username: process.env.KAFKA_API_KEY ?? "",
    password: process.env.KAFKA_API_SECRET ?? "",
  },
});

const consumer = kafka.consumer({ groupId: "sensortag-web-viewer" });
const wss = new WebSocketServer({ port: 3001 });
const clients = new Set<WebSocket>();

wss.on("connection", (ws) => {
  clients.add(ws);
  console.log(`Client connected (${clients.size} total)`);
  ws.on("close", () => {
    clients.delete(ws);
    console.log(`Client disconnected (${clients.size} total)`);
  });
});

async function start() {
  await consumer.connect();
  await consumer.subscribe({ topic, fromBeginning: false });
  console.log(`Kafka consumer connected, subscribed to "${topic}"`);
  console.log(`WebSocket server listening on ws://localhost:3001`);

  await consumer.run({
    eachMessage: async ({ message }) => {
      if (!message.value) return;
      const data = message.value.toString();
      for (const ws of clients) {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send(data);
        }
      }
    },
  });
}

start().catch((err) => {
  console.error("Fatal:", err);
  process.exit(1);
});
