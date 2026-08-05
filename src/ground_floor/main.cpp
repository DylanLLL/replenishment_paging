#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============ FUNCTIONS ============

void blinkLEDs()
{
  for (int i = 0; i < 8; i++)
    digitalWrite(FLOOR_LED_PINS[i], HIGH);
  delay(200);
  for (int i = 0; i < 8; i++)
    digitalWrite(FLOOR_LED_PINS[i], LOW);
  delay(200);
}

void updateLEDs()
{
  for (int i = 0; i < 8; i++)
    digitalWrite(FLOOR_LED_PINS[i], floor_Alert[i] ? HIGH : LOW);
}

AlertBuzzer buzzer;

// Bit i set = floor/area i is alerting. Drives the shared melody sequencer.
uint8_t alertMask()
{
  uint8_t mask = 0;
  for (int i = 0; i < 8; i++)
    if (floor_Alert[i])
      mask |= (1 << i);
  return mask;
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String topicStr = String(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  Serial.print("MQTT in [");
  Serial.print(topicStr);
  Serial.print("]: ");
  Serial.println(msg);

  // Parse floor number and area from "warehouse/floor/X/A/alert"
  int s1 = topicStr.indexOf('/');
  int s2 = topicStr.indexOf('/', s1 + 1);
  int s3 = topicStr.indexOf('/', s2 + 1);
  int s4 = topicStr.indexOf('/', s3 + 1);
  if (s1 < 0 || s2 < 0 || s3 < 0 || s4 < 0)
    return;

  int floorNum = topicStr.substring(s2 + 1, s3).toInt();
  String area = topicStr.substring(s3 + 1, s4);
  if (floorNum < 1 || floorNum > 4 || (area != "A" && area != "B"))
    return;

  int idx = (floorNum - 1) + (area == "B" ? 4 : 0);
  if (msg == "ALERT")
    floor_Alert[idx] = true;
  else if (msg == "CLEAR")
    floor_Alert[idx] = false;

  updateLEDs();
}

void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    blinkLEDs();
    Serial.print(".");
  }
  Serial.print(" Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT()
{
  while (!mqttClient.connected())
  {
    String clientId = "groundfloor-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT as ");
    Serial.print(clientId);
    Serial.print("...");
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println(" connected.");
      mqttClient.subscribe("warehouse/floor/+/+/alert");
    }
    else
    {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Retrying in 3s.");
      delay(3000);
    }
  }
}

// ============ SETUP & LOOP ============

void setup()
{
  Serial.begin(115200);
  delay(100);

  for (int i = 0; i < 8; i++)
  {
    pinMode(FLOOR_LED_PINS[i], OUTPUT);
    digitalWrite(FLOOR_LED_PINS[i], LOW);
  }
  buzzer.begin(BUZZER_PIN);

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();

  Serial.println("Ground floor unit ready.");
}

void loop()
{
  if (millis() > RESET_INTERVAL)
  {
    Serial.println("Reset ESP");
    ESP.restart();
  }

  if (!mqttClient.connected())
    connectMQTT();
  mqttClient.loop();

  buzzer.update(alertMask());
}
